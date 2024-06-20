// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_render_copy_results.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl_on_gpu.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/skia/include/core/SkPixelRef.h"

namespace viz {

AsyncReadResultHelper::AsyncReadResultHelper(
    SkiaOutputSurfaceImplOnGpu* impl_on_gpu,
    std::unique_ptr<const SkSurface::AsyncReadResult> result)
    : lock_(impl_on_gpu->GetAsyncReadResultLock()),
      impl_on_gpu_(impl_on_gpu),
      result_(std::move(result)) {
  base::AutoLock auto_lock(lock());
  impl_on_gpu_->AddAsyncReadResultHelperWithLock(this);
}

AsyncReadResultHelper::~AsyncReadResultHelper() {
  base::AutoLock auto_lock(lock());
  if (impl_on_gpu_) {
    impl_on_gpu_->RemoveAsyncReadResultHelperWithLock(this);
  }
}

base::Lock& AsyncReadResultHelper::lock() const {
  return lock_->lock();
}

void AsyncReadResultHelper::reset() {
  AssertLockAcquired();
  impl_on_gpu_ = nullptr;
  result_.reset();
}

const SkSurface::AsyncReadResult* AsyncReadResultHelper::operator->() const {
  AssertLockAcquired();
  return result_.get();
}

AsyncReadResultHelper::operator bool() const {
  AssertLockAcquired();
  return !!result_;
}

void AsyncReadResultHelper::AssertLockAcquired() const {
  if (lock_)
    lock().AssertAcquired();
}

ReadPixelsContext::ReadPixelsContext(
    std::unique_ptr<CopyOutputRequest> request,
    const gfx::Rect& result_rect,
    const gfx::ColorSpace& color_space,
    base::WeakPtr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu)
    : request(std::move(request)),
      result_rect(result_rect),
      color_space(color_space),
      impl_on_gpu(impl_on_gpu) {}

ReadPixelsContext::~ReadPixelsContext() = default;

CopyOutputResultSkiaRGBA::CopyOutputResultSkiaRGBA(
    SkiaOutputSurfaceImplOnGpu* impl,
    const gfx::Rect& rect,
    std::unique_ptr<const SkSurface::AsyncReadResult> result,
    const gfx::ColorSpace& color_space)
    : CopyOutputResult(Format::RGBA,
                       Destination::kSystemMemory,
                       rect,
                       /*needs_lock_for_bitmap=*/true),
      result_(impl, std::move(result)),
      color_space_(color_space.ToSkColorSpace()) {}

CopyOutputResultSkiaRGBA::~CopyOutputResultSkiaRGBA() {
  // cached_bitmap()->pixelRef() should not be used after
  // CopyOutputResultSkiaRGBA is released.
  DCHECK(!cached_bitmap()->pixelRef() || cached_bitmap()->pixelRef()->unique());
}

// static
void CopyOutputResultSkiaRGBA::OnReadbackDone(
    void* c,
    std::unique_ptr<const SkSurface::AsyncReadResult> async_result) {
  auto context = base::WrapUnique(static_cast<ReadPixelsContext*>(c));

  // This will automatically send an empty result.
  auto* impl_on_gpu = context->impl_on_gpu.get();
  if (!impl_on_gpu)
    return;

  impl_on_gpu->ReadbackDone();

  // This will automatically send an empty result.
  if (!async_result)
    return;

  auto result = std::make_unique<CopyOutputResultSkiaRGBA>(
      impl_on_gpu, context->result_rect, std::move(async_result),
      context->color_space);
  context->request->SendResult(std::move(result));
}

const SkBitmap& CopyOutputResultSkiaRGBA::AsSkBitmap() const {
  if (!result_) {
    // The |result_| has been reset, the cached_bitmap() should be reset too.
    *cached_bitmap() = SkBitmap();
  } else if (!bitmap_created_) {
    const auto* data = result_->data(0);
    auto row_bytes = result_->rowBytes(0);

    // TODO(https://bugs.chromium.org/p/skia/issues/detail?id=14389):
    // BGRA is not supported on iOS, so explicitly request RGBA here. This
    // should not prevent readback, however, so once that is fixed, this code
    // could be removed.
    auto info =
#if BUILDFLAG(IS_IOS)
        SkImageInfo::Make(size().width(), size().height(),
                          kRGBA_8888_SkColorType, kPremul_SkAlphaType,
                          color_space_);
#else
        SkImageInfo::MakeN32Premul(size().width(), size().height(),
                                   color_space_);
#endif  // BUILDFLAG(IS_IOS)

    SkBitmap bitmap;
    bitmap.installPixels(info, const_cast<void*>(data), row_bytes);

    *cached_bitmap() = std::move(bitmap);
    bitmap_created_ = true;
  }

  return CopyOutputResult::AsSkBitmap();
}

bool CopyOutputResultSkiaRGBA::LockSkBitmap() const {
  result_.lock().Acquire();
  if (!result_) {
    result_.lock().Release();
    return false;
  }
  return true;
}

void CopyOutputResultSkiaRGBA::UnlockSkBitmap() const {
  result_.lock().AssertAcquired();
  result_.lock().Release();
}

ReadbackContextTexture::ReadbackContextTexture(
    base::WeakPtr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu,
    std::unique_ptr<CopyOutputRequest> request,
    const gfx::Rect& result_rect,
    const gpu::Mailbox& mailbox,
    const gfx::ColorSpace& color_space)
    : impl_on_gpu_(impl_on_gpu),
      request_(std::move(request)),
      result_rect_(result_rect),
      mailbox_(mailbox),
      color_space_(color_space) {}

ReadbackContextTexture::~ReadbackContextTexture() = default;

void ReadbackContextTexture::OnMailboxReady(GrGpuFinishedContext c) {
  auto context = base::WrapUnique(static_cast<ReadbackContextTexture*>(c));
  context->OnMailboxReadyInternal();
  // `context` is destroyed when this goes out of scope.
}

void ReadbackContextTexture::OnMailboxReadyInternal() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (impl_on_gpu_) {
    impl_on_gpu_->ReadbackDone();
  }

  request_->SendResult(std::make_unique<CopyOutputTextureResult>(
      request_->result_format(), result_rect_,
      CopyOutputResult::TextureResult(mailbox_, color_space_),
      CopyOutputResult::ReleaseCallbacks()));
}

CopyOutputResultSkiaYUV::CopyOutputResultSkiaYUV(
    SkiaOutputSurfaceImplOnGpu* impl,
    const gfx::Rect& rect,
    std::unique_ptr<const SkSurface::AsyncReadResult> result)
    : CopyOutputResult(Format::I420_PLANES,
                       Destination::kSystemMemory,
                       rect,
                       /*needs_lock_for_bitmap=*/false),
      result_(impl, std::move(result)) {
#if DCHECK_IS_ON()
  base::AutoLock auto_lock(result_.lock());
  DCHECK_EQ(3, result_->count());
  DCHECK_EQ(0, size().width() % 2);
  DCHECK_EQ(0, size().height() % 2);
#endif
}

CopyOutputResultSkiaYUV::~CopyOutputResultSkiaYUV() = default;

// static
void CopyOutputResultSkiaYUV::OnReadbackDone(
    void* c,
    std::unique_ptr<const SkSurface::AsyncReadResult> async_result) {
  auto context = base::WrapUnique(static_cast<ReadPixelsContext*>(c));
  auto* impl_on_gpu = context->impl_on_gpu.get();

  // This will automatically send an empty result.
  if (!impl_on_gpu)
    return;

  impl_on_gpu->ReadbackDone();

  // This will automatically send an empty result.
  if (!async_result)
    return;

  auto result = std::make_unique<CopyOutputResultSkiaYUV>(
      impl_on_gpu, context->result_rect, std::move(async_result));
  context->request->SendResult(std::move(result));
}

// CopyOutputResult implementation:
bool CopyOutputResultSkiaYUV::ReadI420Planes(uint8_t* y_out,
                                             int y_out_stride,
                                             uint8_t* u_out,
                                             int u_out_stride,
                                             uint8_t* v_out,
                                             int v_out_stride) const {
  // Hold the lock so the AsyncReadResultHelper will not be reset during
  // pixel data reading.
  base::AutoLock auto_lock(result_.lock());

  // The |result_| has been reset.
  if (!result_)
    return false;

  auto* data0 = static_cast<const uint8_t*>(result_->data(0));
  auto* data1 = static_cast<const uint8_t*>(result_->data(1));
  auto* data2 = static_cast<const uint8_t*>(result_->data(2));
  libyuv::CopyPlane(data0, result_->rowBytes(0), y_out, y_out_stride, width(0),
                    height(0));
  libyuv::CopyPlane(data1, result_->rowBytes(1), u_out, u_out_stride, width(1),
                    height(1));
  libyuv::CopyPlane(data2, result_->rowBytes(2), v_out, v_out_stride, width(2),
                    height(2));
  return true;
}

NV12PlanesReadbackContext::NV12PlanesReadbackContext(
    base::WeakPtr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu,
    std::unique_ptr<CopyOutputRequest> request,
    const gfx::Rect& result_rect)
    : impl_on_gpu_(impl_on_gpu),
      request_(std::move(request)),
      result_rect_(result_rect) {}
NV12PlanesReadbackContext::~NV12PlanesReadbackContext() = default;

void NV12PlanesReadbackContext::PlaneReadbackDone(
    int plane_index,
    std::unique_ptr<const SkSurface::AsyncReadResult> async_result) {
  DCHECK_LT(plane_index, 2) << "NV12 readback requests at most 2 planes!";

  if (!impl_on_gpu_) {
    // We could not identify the impl_on_gpu, which means there's nothing else
    // we can do here (everything should already be cleaned up).
    return;
  }

  impl_on_gpu_->ReadbackDone();

  // Mark plane readback as completed and store results in the readback context:
  outstanding_reads_--;
  plane_results_[plane_index] = std::move(async_result);

  // Check if all planes have finished readback, send the result and clean up
  // the readback context if so:
  if (outstanding_reads_ == 0) {
    auto result = std::make_unique<CopyOutputResultSkiaNV12>(
        impl_on_gpu_.get(), result_rect_, std::move(plane_results_));

    request_->SendResult(std::move(result));
  }
}

NV12PlanePixelReadContext::NV12PlanePixelReadContext(
    scoped_refptr<NV12PlanesReadbackContext> nv12_planes_readback,
    int plane_index)
    : nv12_planes_readback(std::move(nv12_planes_readback)),
      plane_index(plane_index) {}
NV12PlanePixelReadContext::~NV12PlanePixelReadContext() = default;

CopyOutputResultSkiaNV12::CopyOutputResultSkiaNV12(
    SkiaOutputSurfaceImplOnGpu* impl,
    const gfx::Rect& rect,
    std::array<std::unique_ptr<const SkImage::AsyncReadResult>, 2>
        async_read_results)
    : CopyOutputResult(Format::I420_PLANES,
                       Destination::kSystemMemory,
                       rect,
                       /*needs_lock_for_bitmap=*/false) {
  DCHECK_EQ(0, size().width() % 2);
  DCHECK_EQ(0, size().height() % 2);

  for (auto& async_read_result : async_read_results) {
    plane_readback_results_.push_back(std::make_unique<AsyncReadResultHelper>(
        impl, std::move(async_read_result)));
  }
}

CopyOutputResultSkiaNV12::~CopyOutputResultSkiaNV12() = default;

bool CopyOutputResultSkiaNV12::ReadNV12Planes(uint8_t* y_out,
                                              int y_out_stride,
                                              uint8_t* uv_out,
                                              int uv_out_stride) const {
  const auto plane_pointer = [y_out, uv_out](int plane_number) {
    return plane_number == 0 ? y_out : uv_out;
  };

  const auto plane_stride = [y_out_stride, uv_out_stride](int plane_number) {
    return plane_number == 0 ? y_out_stride : uv_out_stride;
  };

  for (size_t i = 0; i < kNV12MaxPlanes; ++i) {
    // The ptr to the helper is not set, which means that readback for this
    // plane has failed. Return failure, partial results are not useful.
    if (!plane_readback_results_[i]) {
      return false;
    }

    const AsyncReadResultHelper& result = *plane_readback_results_[i].get();

    // Hold the lock so the AsyncReadResultHelper will not be reset during
    // pixel data reading.
    base::AutoLock auto_lock(result.lock());
    DCHECK_EQ(1, result->count());

    // The |result| has been reset.
    if (!result) {
      return false;
    }

    auto* data = static_cast<const uint8_t*>(result->data(0));
    libyuv::CopyPlane(data, result->rowBytes(0), plane_pointer(i),
                      plane_stride(i), width(i), height(i));
  }

  return true;
}

// static
void CopyOutputResultSkiaNV12::OnNV12PlaneReadbackDone(
    void* c,
    std::unique_ptr<const SkSurface::AsyncReadResult> async_result) {
  auto context = base::WrapUnique(static_cast<NV12PlanePixelReadContext*>(c));

  context->nv12_planes_readback->PlaneReadbackDone(context->plane_index,
                                                   std::move(async_result));
}

}  // namespace viz
