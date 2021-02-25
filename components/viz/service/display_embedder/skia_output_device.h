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
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/src/gpu/GrSemaphore.h"
#include "ui/gfx/swap_result.h"

class GrDirectContext;
class SkSurface;

namespace base {
class SequencedTaskRunner;
}

namespace gfx {
class ColorSpace;
class Rect;
class Size;
struct PresentationFeedback;
}  // namespace gfx

namespace gpu {
class MemoryTracker;
class MemoryTypeTracker;
}  // namespace gpu

namespace ui {
class LatencyTracker;
}

namespace viz {

class VulkanContextProvider;

class SkiaOutputDevice {
 public:
  // A helper class for defining a BeginPaint() and EndPaint() scope.
  class ScopedPaint {
   public:
    ScopedPaint(std::vector<GrBackendSemaphore> end_semaphores,
                SkiaOutputDevice* device,
                SkSurface* sk_surface);
    ~ScopedPaint();

    // This can be null.
    SkSurface* sk_surface() const { return sk_surface_; }
    SkCanvas* GetCanvas();
    GrSemaphoresSubmitted Flush(VulkanContextProvider* vulkan_context_provider,
                                std::vector<GrBackendSemaphore> end_semaphores,
                                base::OnceClosure on_finished);
    bool Wait(int num_semaphores,
              const GrBackendSemaphore wait_semaphores[],
              bool delete_semaphores_after_wait);
    bool Draw(sk_sp<const SkDeferredDisplayList> ddl);

    std::vector<GrBackendSemaphore> TakeEndPaintSemaphores() {
      std::vector<GrBackendSemaphore> semaphores;
      semaphores.swap(end_semaphores_);
      return semaphores;
    }

   private:
    std::vector<GrBackendSemaphore> end_semaphores_;
    SkiaOutputDevice* const device_;
    // Null when using vulkan secondary command buffer.
    SkSurface* const sk_surface_;

    DISALLOW_COPY_AND_ASSIGN(ScopedPaint);
  };

  using BufferPresentedCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback& feedback)>;
  using DidSwapBufferCompleteCallback =
      base::RepeatingCallback<void(gpu::SwapBuffersCompleteParams,
                                   const gfx::Size& pixel_size)>;
  SkiaOutputDevice(
      GrDirectContext* gr_context,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);
  virtual ~SkiaOutputDevice();

  // Begins a paint scope. The base implementation fails when the SkSurface
  // cannot be initialized, but devices that don't draw to a SkSurface (i.e
  // |SkiaOutputDeviceVulkanSecondaryCB|) can override this to bypass the
  // check.
  virtual std::unique_ptr<SkiaOutputDevice::ScopedPaint> BeginScopedPaint();

  // Changes the size of draw surface and invalidates it's contents.
  virtual bool Reshape(const gfx::Size& size,
                       float device_scale_factor,
                       const gfx::ColorSpace& color_space,
                       gfx::BufferFormat format,
                       gfx::OverlayTransform transform) = 0;

  // Submit the GrContext and run |callback| after. Note most but not all
  // implementations will run |callback| in this call stack.
  // If the |sync_cpu| flag is true this function will return once the gpu
  // has finished with all submitted work.
  virtual void Submit(bool sync_cpu, base::OnceClosure callback);

  // Presents the back buffer.
  virtual void SwapBuffers(BufferPresentedCallback feedback,
                           OutputSurfaceFrame frame) = 0;
  virtual void PostSubBuffer(const gfx::Rect& rect,
                             BufferPresentedCallback feedback,
                             OutputSurfaceFrame frame);
  virtual void CommitOverlayPlanes(BufferPresentedCallback feedback,
                                   OutputSurfaceFrame frame);

  // Set the rectangle that will be drawn into on the surface.
  virtual bool SetDrawRectangle(const gfx::Rect& draw_rectangle);

  // Enable or disable DC layers. Must be called before DC layers are scheduled.
  virtual void SetEnableDCLayers(bool enabled);

  virtual void SetGpuVSyncEnabled(bool enabled);

  // Whether the output device's primary plane is an overlay. This returns true
  // is the SchedulePrimaryPlane function is implemented.
  virtual bool IsPrimaryPlaneOverlay() const;

  // Schedule the output device's back buffer as an overlay plane. The scheduled
  // primary plane will be on screen when SwapBuffers() or PostSubBuffer() is
  // called.
  virtual void SchedulePrimaryPlane(
      const base::Optional<
          OverlayProcessorInterface::OutputSurfaceOverlayPlane>& plane);

  // Schedule overlays which will be on screen when SwapBuffers() or
  // PostSubBuffer() is called.
  virtual void ScheduleOverlays(SkiaOutputSurface::OverlayList overlays);

  const OutputSurface::Capabilities& capabilities() const {
    return capabilities_;
  }

  // EnsureBackbuffer called when output surface is visible and may be drawn to.
  // DiscardBackbuffer called when output surface is hidden and will not be
  // drawn to. Default no-op.
  virtual void EnsureBackbuffer();
  virtual void DiscardBackbuffer();

  bool is_emulated_rgbx() const { return is_emulated_rgbx_; }

  void SetDrawTimings(base::TimeTicks submitted, base::TimeTicks started);

 protected:
  // Only valid between StartSwapBuffers and FinishSwapBuffers.
  class SwapInfo {
   public:
    SwapInfo(uint64_t swap_id,
             BufferPresentedCallback feedback,
             base::TimeTicks viz_scheduled_draw,
             base::TimeTicks gpu_started_draw);
    SwapInfo(SwapInfo&& other);
    ~SwapInfo();
    const gpu::SwapBuffersCompleteParams& Complete(
        gfx::SwapCompletionResult result,
        const base::Optional<gfx::Rect>& damage_area,
        std::vector<gpu::Mailbox> released_overlays,
        const gpu::Mailbox& primary_plane_mailbox);
    void CallFeedback();

   private:
    BufferPresentedCallback feedback_;
    gpu::SwapBuffersCompleteParams params_;
  };

  // Begin paint the back buffer.
  virtual SkSurface* BeginPaint(
      std::vector<GrBackendSemaphore>* end_semaphores) = 0;

  // End paint the back buffer.
  virtual void EndPaint() = 0;

  // Overridden by SkiaOutputDeviceVulkanSecondaryCB.
  virtual SkCanvas* GetCanvas(SkSurface* sk_surface);
  virtual GrSemaphoresSubmitted Flush(
      SkSurface* sk_surface,
      VulkanContextProvider* vulkan_context_provider,
      std::vector<GrBackendSemaphore> end_semaphores,
      base::OnceClosure on_finished);
  virtual bool Wait(SkSurface* sk_surface,
                    int num_semaphores,
                    const GrBackendSemaphore wait_semaphores[],
                    bool delete_semaphores_after_wait);
  virtual bool Draw(SkSurface* sk_surface,
                    sk_sp<const SkDeferredDisplayList> ddl);

  // Helper method for SwapBuffers() and PostSubBuffer(). It should be called
  // at the beginning of SwapBuffers() and PostSubBuffer() implementations
  void StartSwapBuffers(BufferPresentedCallback feedback);

  // Helper method for SwapBuffers() and PostSubBuffer(). It should be called
  // at the end of SwapBuffers() and PostSubBuffer() implementations
  void FinishSwapBuffers(
      gfx::SwapCompletionResult result,
      const gfx::Size& size,
      OutputSurfaceFrame frame,
      const base::Optional<gfx::Rect>& damage_area = base::nullopt,
      std::vector<gpu::Mailbox> released_overlays = {},
      const gpu::Mailbox& primary_plane_mailbox = gpu::Mailbox());

  GrDirectContext* const gr_context_;

  OutputSurface::Capabilities capabilities_;

  uint64_t swap_id_ = 0;
  DidSwapBufferCompleteCallback did_swap_buffer_complete_callback_;

  base::queue<SwapInfo> pending_swaps_;
  base::TimeTicks viz_scheduled_draw_;
  base::TimeTicks gpu_started_draw_;

  // RGBX format is emulated with RGBA.
  bool is_emulated_rgbx_ = false;

  std::unique_ptr<gpu::MemoryTypeTracker> memory_type_tracker_;

 private:
  std::unique_ptr<ui::LatencyTracker> latency_tracker_;
  // task runner for latency tracker.
  scoped_refptr<base::SequencedTaskRunner> latency_tracker_runner_;

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputDevice);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_H_
