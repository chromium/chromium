// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "components/viz/service/display/output_surface.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/src/gpu/GrSemaphore.h"
#include "ui/gfx/swap_result.h"
#include "ui/latency/latency_tracker.h"

class SkSurface;

namespace gfx {
class ColorSpace;
class GpuFence;
class Rect;
class Size;
struct PresentationFeedback;
}  // namespace gfx

// TODO(crbug.com/996004): Remove this once we use BufferQueue SharedImage
// implementation.
namespace gl {
class GLImage;
}

namespace viz {
#if defined(OS_WIN)
class DCLayerOverlay;
#endif

class SkiaOutputDevice {
 public:
  // A helper class for defining a BeginPaint() and EndPaint() scope.
  class ScopedPaint {
   public:
    explicit ScopedPaint(SkiaOutputDevice* device)
        : device_(device), sk_surface_(device->BeginPaint()) {
      DCHECK(sk_surface_);
    }
    ~ScopedPaint() { device_->EndPaint(semaphore_); }

    SkSurface* sk_surface() const { return sk_surface_; }
    void set_semaphore(const GrBackendSemaphore& semaphore) {
      DCHECK(!semaphore_.isInitialized());
      semaphore_ = semaphore;
    }

   private:
    SkiaOutputDevice* const device_;
    SkSurface* const sk_surface_;
    GrBackendSemaphore semaphore_;

    DISALLOW_COPY_AND_ASSIGN(ScopedPaint);
  };

  using BufferPresentedCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback& feedback)>;
  using DidSwapBufferCompleteCallback =
      base::RepeatingCallback<void(gpu::SwapBuffersCompleteParams,
                                   const gfx::Size& pixel_size)>;
  SkiaOutputDevice(
      bool need_swap_semaphore,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);
  virtual ~SkiaOutputDevice();

  // Changes the size of draw surface and invalidates it's contents.
  virtual bool Reshape(const gfx::Size& size,
                       float device_scale_factor,
                       const gfx::ColorSpace& color_space,
                       bool has_alpha,
                       gfx::OverlayTransform transform) = 0;

  // Presents the back buffer.
  virtual void SwapBuffers(BufferPresentedCallback feedback,
                           std::vector<ui::LatencyInfo> latency_info) = 0;
  virtual void PostSubBuffer(const gfx::Rect& rect,
                             BufferPresentedCallback feedback,
                             std::vector<ui::LatencyInfo> latency_info);

  // TODO(crbug.com/996004): Should use BufferQueue SharedImage
  // implementation instead of GLImage.
  virtual gl::GLImage* GetOverlayImage();
  virtual std::unique_ptr<gfx::GpuFence> SubmitOverlayGpuFence();

  // Set the rectangle that will be drawn into on the surface.
  virtual void SetDrawRectangle(const gfx::Rect& draw_rectangle);

  virtual void SetGpuVSyncEnabled(bool enabled);
#if defined(OS_WIN)
  virtual void SetEnableDCLayers(bool enabled);
  virtual void ScheduleDCLayers(std::vector<DCLayerOverlay> dc_layers);
#endif

  const OutputSurface::Capabilities& capabilities() const {
    return capabilities_;
  }

  // EnsureBackbuffer called when output surface is visible and may be drawn to.
  // DiscardBackbuffer called when output surface is hidden and will not be
  // drawn to. Default no-op.
  virtual void EnsureBackbuffer();
  virtual void DiscardBackbuffer();

  bool need_swap_semaphore() const { return need_swap_semaphore_; }
  bool is_emulated_rgbx() const { return is_emulated_rgbx_; }

 protected:
  // Begin paint the back buffer.
  virtual SkSurface* BeginPaint() = 0;

  // End paint the back buffer.
  virtual void EndPaint(const GrBackendSemaphore& semaphore) = 0;

  // Helper method for SwapBuffers() and PostSubBuffer(). It should be called
  // at the beginning of SwapBuffers() and PostSubBuffer() implementations
  void StartSwapBuffers(base::Optional<BufferPresentedCallback> feedback);

  // Helper method for SwapBuffers() and PostSubBuffer(). It should be called
  // at the end of SwapBuffers() and PostSubBuffer() implementations
  void FinishSwapBuffers(gfx::SwapResult result,
                         const gfx::Size& size,
                         std::vector<ui::LatencyInfo> latency_info);

  OutputSurface::Capabilities capabilities_;

  const bool need_swap_semaphore_;
  uint64_t swap_id_ = 0;
  DidSwapBufferCompleteCallback did_swap_buffer_complete_callback_;

  // Only valid between StartSwapBuffers and FinishSwapBuffers.
  class SwapInfo {
   public:
    SwapInfo(uint64_t swap_id,
             base::Optional<BufferPresentedCallback> feedback);
    SwapInfo(SwapInfo&& other);
    ~SwapInfo();
    const gpu::SwapBuffersCompleteParams& Complete(gfx::SwapResult result);
    void CallFeedback();

   private:
    base::Optional<BufferPresentedCallback> feedback_;
    gpu::SwapBuffersCompleteParams params_;
  };

  base::queue<SwapInfo> pending_swaps_;

  ui::LatencyTracker latency_tracker_;

  // RGBX format is emulated with RGBA.
  bool is_emulated_rgbx_ = false;

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputDevice);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_H_
