// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_RENDER_COPY_RESULTS_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_RENDER_COPY_RESULTS_H_

#include <memory>

#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl_on_gpu.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/skia/include/core/SkPixelRef.h"

namespace viz {

// This header file contains classes related to servicing CopyOutputRequests
// from SkiaOutputSurfaceImplOnGpu.

class AsyncReadResultLock
    : public base::RefCountedThreadSafe<AsyncReadResultLock> {
 public:
  AsyncReadResultLock() = default;
  base::Lock& lock() { return lock_; }

 private:
  friend class base::RefCountedThreadSafe<AsyncReadResultLock>;
  ~AsyncReadResultLock() = default;

  base::Lock lock_;
};

// Helper class for holding AsyncReadResult.
class AsyncReadResultHelper {
 public:
  explicit AsyncReadResultHelper(
      SkiaOutputSurfaceImplOnGpu* impl_on_gpu,
      std::unique_ptr<const SkSurface::AsyncReadResult> result);

  ~AsyncReadResultHelper();

  base::Lock& lock() const;
  void reset();

  const SkSurface::AsyncReadResult* operator->() const;

  explicit operator bool() const;

 private:
  void AssertLockAcquired() const;

  const scoped_refptr<AsyncReadResultLock> lock_;
  SkiaOutputSurfaceImplOnGpu* impl_on_gpu_;
  std::unique_ptr<const SkSurface::AsyncReadResult> result_;
};

struct ReadPixelsContext {
  ReadPixelsContext(std::unique_ptr<CopyOutputRequest> request,
                    const gfx::Rect& result_rect,
                    const gfx::ColorSpace& color_space,
                    base::WeakPtr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu);
  ~ReadPixelsContext();

  std::unique_ptr<CopyOutputRequest> request;
  gfx::Rect result_rect;
  gfx::ColorSpace color_space;
  base::WeakPtr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu;
};

class CopyOutputResultSkiaRGBA : public CopyOutputResult {
 public:
  CopyOutputResultSkiaRGBA(
      SkiaOutputSurfaceImplOnGpu* impl,
      const gfx::Rect& rect,
      std::unique_ptr<const SkSurface::AsyncReadResult> result,
      const gfx::ColorSpace& color_space);

  ~CopyOutputResultSkiaRGBA() override;

  static void OnReadbackDone(
      void* c,
      std::unique_ptr<const SkSurface::AsyncReadResult> async_result);

  const SkBitmap& AsSkBitmap() const override;

  bool LockSkBitmap() const override NO_THREAD_SAFETY_ANALYSIS;

  void UnlockSkBitmap() const override NO_THREAD_SAFETY_ANALYSIS;

 private:
  AsyncReadResultHelper result_;
  const sk_sp<SkColorSpace> color_space_;
  mutable bool bitmap_created_ = false;
};

class CopyOutputResultSkiaYUV : public CopyOutputResult {
 public:
  CopyOutputResultSkiaYUV(
      SkiaOutputSurfaceImplOnGpu* impl,
      const gfx::Rect& rect,
      std::unique_ptr<const SkSurface::AsyncReadResult> result);
  ~CopyOutputResultSkiaYUV() override;

  static void OnReadbackDone(
      void* c,
      std::unique_ptr<const SkSurface::AsyncReadResult> async_result);

  // CopyOutputResult implementation:
  bool ReadI420Planes(uint8_t* y_out,
                      int y_out_stride,
                      uint8_t* u_out,
                      int u_out_stride,
                      uint8_t* v_out,
                      int v_out_stride) const override;

 private:
  uint32_t width(int plane) const {
    return plane == 0 ? size().width() : size().width() / 2;
  }

  uint32_t height(int plane) const {
    return plane == 0 ? size().height() : size().height() / 2;
  }

  AsyncReadResultHelper result_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_RENDER_COPY_RESULTS_H_
