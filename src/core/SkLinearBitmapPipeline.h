/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkLinearBitmapPipeline_DEFINED
#define SkLinearBitmapPipeline_DEFINED

#include "SkColor.h"
#include "SkImageInfo.h"
#include "SkMatrix.h"
#include "SkShader.h"

class SkEmbeddableLinearPipeline;

enum SkGammaType {
    kLinear_SkGammaType,
    kSRGB_SkGammaType,
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// SkLinearBitmapPipeline - encapsulates all the machinery for doing floating point pixel
// processing in a linear color space.
// Note: this class has unusual alignment requirements due to its use of SIMD instructions. The
// class SkEmbeddableLinearPipeline below manages these requirements.
class SkLinearBitmapPipeline {
public:
    SkLinearBitmapPipeline(
        const SkMatrix& inverse,
        SkFilterQuality filterQuality,
        SkShader::TileMode xTile, SkShader::TileMode yTile,
        SkColor paintColor,
        const SkPixmap& srcPixmap);

    SkLinearBitmapPipeline(
        const SkLinearBitmapPipeline& pipeline,
        const SkPixmap& srcPixmap,
        SkBlendMode blendMode,
        const SkImageInfo& dstInfo);

    static bool ClonePipelineForBlitting(
        SkEmbeddableLinearPipeline* pipelineStorage,
        const SkLinearBitmapPipeline& pipeline,
        SkMatrix::TypeMask matrixMask,
        SkShader::TileMode xTileMode,
        SkShader::TileMode yTileMode,
        SkFilterQuality filterQuality,
        const SkPixmap& srcPixmap,
        float finalAlpha,
        SkBlendMode,
        const SkImageInfo& dstInfo);

    ~SkLinearBitmapPipeline();

    void shadeSpan4f(int x, int y, SkPM4f* dst, int count);
    void blitSpan(int32_t x, int32_t y, void* dst, int count);

    template<typename Base, size_t kSize, typename Next = void>
    class Stage {
    public:
        Stage() : fIsInitialized{false} {}
        ~Stage();

        template<typename Variant, typename... Args>
        void initStage(Next* next, Args&& ... args);

        template<typename Variant, typename... Args>
        void initSink(Args&& ... args);

        template <typename To, typename From>
        To* getInterface();

        // Copy this stage to `cloneToStage` with `next` as its next stage
        // (not necessarily the same as our next, you see), returning `cloneToStage`.
        // Note: There is no cloneSinkTo method because the code usually places the top part of
        // the pipeline on a new sampler.
        Base* cloneStageTo(Next* next, Stage* cloneToStage) const;

        Base* get() const { return reinterpret_cast<Base*>(fSpace); }
        Base* operator->() const { return this->get(); }
        Base& operator*() const { return *(this->get()); }

    private:
        std::function<void (Next*, void*)> fStageCloner;
        mutable char                       fSpace[kSize];
        bool                               fIsInitialized;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////
// PolyMemory
    template <typename Base, size_t kSize>
    class PolyMemory {
    public:
        PolyMemory() : fIsInitialized{false} { }
        ~PolyMemory() {
            if (fIsInitialized) {
                this->get()->~Base();
            }
        }
        template<typename Variant, typename... Args>
        void init(Args&& ... args) {
            SkASSERTF(sizeof(Variant) <= sizeof(fSpace),
                      "Size Variant: %d, Space: %d", sizeof(Variant), sizeof(fSpace));

            new (&fSpace) Variant(std::forward<Args>(args)...);
            fIsInitialized = true;
        }

        Base* get() const { return reinterpret_cast<Base*>(fSpace); }
        Base* operator->() const { return this->get(); }
        Base& operator*() const { return *(this->get()); }

    private:
        mutable char fSpace[kSize];
        bool         fIsInitialized;

    };

    class PointProcessorInterface;
    class SampleProcessorInterface;
    class BlendProcessorInterface;
    class DestinationInterface;
    class PixelAccessorInterface;

    // These values were generated by the assert above in Stage::init{Sink|Stage}.
    using MatrixStage  = Stage<PointProcessorInterface,     56, PointProcessorInterface>;
    using TileStage    = Stage<PointProcessorInterface,     48, SampleProcessorInterface>;
    using SampleStage  = Stage<SampleProcessorInterface,   160, BlendProcessorInterface>;
    using BlenderStage = Stage<BlendProcessorInterface,     48>;
    using Accessor     = PolyMemory<PixelAccessorInterface, 64>;

private:
    PointProcessorInterface* fFirstStage;
    MatrixStage              fMatrixStage;
    TileStage                fTileStage;
    SampleStage              fSampleStage;
    BlenderStage             fBlenderStage;
    DestinationInterface*    fLastStage;
    Accessor                 fAccessor;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// SkEmbeddableLinearPipeline - manage stricter alignment needs for SkLinearBitmapPipeline.
class SkEmbeddableLinearPipeline {
public:
    SkEmbeddableLinearPipeline() { }
    ~SkEmbeddableLinearPipeline() {
        if (fInitialized) {
            get()->~SkLinearBitmapPipeline();
        }
    }

    template <typename... Args>
    void init(Args&&... args) {
        new (fPipelineStorage) SkLinearBitmapPipeline{std::forward<Args>(args)...};
        fInitialized = true;
    }

    SkLinearBitmapPipeline* get() const {
        return reinterpret_cast<SkLinearBitmapPipeline*>(fPipelineStorage);
    }
    SkLinearBitmapPipeline& operator*()  const { return *this->get(); }
    SkLinearBitmapPipeline* operator->() const { return  this->get(); }

private:
    alignas(SkLinearBitmapPipeline) mutable char fPipelineStorage[sizeof(SkLinearBitmapPipeline)];
    bool                                         fInitialized {false};
};

#endif  // SkLinearBitmapPipeline_DEFINED
