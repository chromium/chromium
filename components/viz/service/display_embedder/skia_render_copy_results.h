// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_RENDER_COPY_RESULTS_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_RENDER_COPY_RESULTS_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "ui/gfx/color_space.h"

namespace viz {

// This header file contains classes related to servicing CopyOutputRequests
// from SkiaOutputSurfaceImplOnGpu.

class SkiaOutputSurfaceImplOnGpu;

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
  raw_ptr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu_;
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

// Context that is responsible for sending a CopyOutputTextureResult once the
// GPU work that populates the GpuMemoryBuffer has completed.
class ReadbackContextTexture {
 public:
  // Will be called with `ReadbackContextTexture*`:
  static void OnMailboxReady(GrGpuFinishedContext context);

  ReadbackContextTexture(base::WeakPtr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu,
                         std::unique_ptr<CopyOutputRequest> request,
                         const gfx::Rect& result_rect,
                         const gpu::Mailbox& mailbox,
                         const gfx::ColorSpace& color_space);
  ~ReadbackContextTexture();

 private:
  void OnMailboxReadyInternal();

  // Needed to notify `SkiaOutputSurfaceImplOnGpu` that readback has completed.
  // GPU work is not a readback, but we rely on the same mechanism for nudging
  // Skia to periodically check for asynchronous event completion.
  base::WeakPtr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu_;
  // Request that we will send a response to:
  std::unique_ptr<CopyOutputRequest> request_;

  // Data needed to create a response to `request_`:
  gfx::Rect result_rect_;
  gpu::Mailbox mailbox_;
  gfx::ColorSpace color_space_;

  THREAD_CHECKER(thread_checker_);
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

// Represents entire NV12 readback request. NV12 readback request consists of
// reading back from 2 surfaces.
class NV12PlanesReadbackContext
    : public base::RefCounted<NV12PlanesReadbackContext> {
 public:
  NV12PlanesReadbackContext(
      base::WeakPtr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu,
      std::unique_ptr<CopyOutputRequest> request,
      const gfx::Rect& result_rect);

  void PlaneReadbackDone(
      int plane_index,
      std::unique_ptr<const SkSurface::AsyncReadResult> async_result);

 private:
  friend class base::RefCounted<NV12PlanesReadbackContext>;
  ~NV12PlanesReadbackContext();

  base::WeakPtr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu_;
  std::unique_ptr<CopyOutputRequest> request_;

  gfx::Rect result_rect_;

  // NV12 readback always starts with 2 outstanding requests:
  int outstanding_reads_ = CopyOutputResult::kNV12MaxPlanes;

  std::array<std::unique_ptr<const SkSurface::AsyncReadResult>,
             CopyOutputResult::kNV12MaxPlanes>
      plane_results_;
};

// Represents a readback request for a specific NV12 plane.
struct NV12PlanePixelReadContext {
  NV12PlanePixelReadContext(
      scoped_refptr<NV12PlanesReadbackContext> nv12_planes_readback,
      int plane_index);
  ~NV12PlanePixelReadContext();

  scoped_refptr<NV12PlanesReadbackContext> nv12_planes_readback;
  int plane_index;
};

class CopyOutputResultSkiaNV12 : public CopyOutputResult {
 public:
  CopyOutputResultSkiaNV12(
      SkiaOutputSurfaceImplOnGpu* impl,
      const gfx::Rect& rect,
      std::array<std::unique_ptr<const SkImage::AsyncReadResult>, 2>
          async_read_results);
  ~CopyOutputResultSkiaNV12() override;

  // CopyOutputResult implementation:
  bool ReadNV12Planes(uint8_t* y_out,
                      int y_out_stride,
                      uint8_t* uv_out,
                      int uv_out_stride) const override;

  static void OnNV12PlaneReadbackDone(
      void* c,
      std::unique_ptr<const SkSurface::AsyncReadResult> async_result);

 private:
  uint32_t width(int plane) const { return size().width(); }

  uint32_t height(int plane) const {
    return plane == 0 ? size().height() : size().height() / 2;
  }

  std::vector<std::unique_ptr<AsyncReadResultHelper>> plane_readback_results_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_RENDER_COPY_RESULTS_H_
