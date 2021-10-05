// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_render_copy_results.h"

#include <memory>
#include <utility>

#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/skia/include/core/SkPixelRef.h"

namespace viz {

AsyncReadResultHelper::AsyncReadResultHelper(
    SkiaOutputSurfaceImplOnGpu* impl_on_gpu,
    std::unique_ptr<const SkSurface::AsyncReadResult> result)
    : lock_(impl_on_gpu->GetAsyncReadResultLock()),
      impl_on_gpu_(impl_on_gpu),
      result_(std::move(result)) {
  impl_on_gpu_->AddAsyncReadResultHelper(this);
}

AsyncReadResultHelper::~AsyncReadResultHelper() {
  if (impl_on_gpu_) {
    impl_on_gpu_->RemoveAsyncReadResultHelper(this);
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
    auto info = SkImageInfo::MakeN32Premul(size().width(), size().height(),
                                           color_space_);
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

}  // namespace viz
