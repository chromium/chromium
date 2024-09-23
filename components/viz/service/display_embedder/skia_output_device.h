// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "ui/gfx/swap_result.h"

class GrDirectContext;
class SkSurface;

namespace gfx {
class Rect;
class Size;
struct PresentationFeedback;
}  // namespace gfx

namespace gpu {
class MemoryTracker;
class MemoryTypeTracker;
}  // namespace gpu

namespace skgpu::graphite {
class Context;
class Recording;
}  // namespace skgpu::graphite

namespace viz {

class VulkanContextProvider;

class VIZ_SERVICE_EXPORT SkiaOutputDevice {
 public:
  // A helper class for defining a BeginPaint() and EndPaint() scope.
  class VIZ_SERVICE_EXPORT ScopedPaint {
   public:
    ScopedPaint(std::vector<GrBackendSemaphore> end_semaphores,
                SkiaOutputDevice* device,
                SkSurface* sk_surface);

    ScopedPaint(const ScopedPaint&) = delete;
    ScopedPaint& operator=(const ScopedPaint&) = delete;

    ~ScopedPaint();

    // This can be null.
    SkSurface* sk_surface() const { return sk_surface_; }
    SkCanvas* GetCanvas();

    // Ganesh
    GrSemaphoresSubmitted Flush(VulkanContextProvider* vulkan_context_provider,
                                std::vector<GrBackendSemaphore> end_semaphores,
                                base::OnceClosure on_finished);
    bool Wait(int num_semaphores,
              const GrBackendSemaphore wait_semaphores[],
              bool delete_semaphores_after_wait);
    bool Draw(sk_sp<const GrDeferredDisplayList> ddl);

    // Graphite
    bool Draw(std::unique_ptr<skgpu::graphite::Recording> graphite_recording,
              base::OnceClosure on_finished);

    std::vector<GrBackendSemaphore> TakeEndPaintSemaphores() {
      std::vector<GrBackendSemaphore> semaphores;
      semaphores.swap(end_semaphores_);
      return semaphores;
    }

   private:
    std::vector<GrBackendSemaphore> end_semaphores_;
    const raw_ptr<SkiaOutputDevice, DanglingUntriaged> device_;
    // Null when using vulkan secondary command buffer.
    const raw_ptr<SkSurface, DanglingUntriaged> sk_surface_;
  };

  using BufferPresentedCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback& feedback)>;
  using DidSwapBufferCompleteCallback =
      base::RepeatingCallback<void(gpu::SwapBuffersCompleteParams,
                                   const gfx::Size& pixel_size,
                                   gfx::GpuFenceHandle release_fence)>;
  using ReleaseOverlaysCallback =
      base::RepeatingCallback<void(const std::vector<gpu::Mailbox>)>;

  SkiaOutputDevice(
      GrDirectContext* gr_context,
      skgpu::graphite::Context* graphite_context,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback,
      ReleaseOverlaysCallback release_overlays_callback = base::DoNothing());

  SkiaOutputDevice(const SkiaOutputDevice&) = delete;
  SkiaOutputDevice& operator=(const SkiaOutputDevice&) = delete;

  virtual ~SkiaOutputDevice();

  // Begins a paint scope. The base implementation fails when the SkSurface
  // cannot be initialized, but devices that don't draw to a SkSurface (i.e
  // |SkiaOutputDeviceVulkanSecondaryCB|) can override this to bypass the
  // check.
  virtual std::unique_ptr<SkiaOutputDevice::ScopedPaint> BeginScopedPaint();

  // Changes the size of draw surface and invalidates it's contents.
  struct ReshapeParams {
    SkImageInfo image_info;
    // This is redundant with `image_info.colorSpace()`.
    gfx::ColorSpace color_space;
    int sample_count = 1;
    float device_scale_factor = 1.f;
    gfx::OverlayTransform transform = gfx::OVERLAY_TRANSFORM_NONE;

    gfx::Size GfxSize() const {
      return gfx::SkISizeToSize(image_info.dimensions());
    }
  };
  virtual bool Reshape(const ReshapeParams& params) = 0;

  // For devices that supports viewporter.
  virtual void SetViewportSize(const gfx::Size& viewport_size);

  // Submit the GrContext and run |callback| after. Note most but not all
  // implementations will run |callback| in this call stack.
  // If the |sync_cpu| flag is true this function will return once the gpu
  // has finished with all submitted work.
  virtual void Submit(bool sync_cpu, base::OnceClosure callback);

  // Presents the back buffer. Optional `update_rect` represents hint of the
  // rect that was updated in the back buffer. If not specified the whole buffer
  // is supposed to be updated.
  virtual void Present(const std::optional<gfx::Rect>& update_rect,
                       BufferPresentedCallback feedback,
                       OutputSurfaceFrame frame) = 0;

  virtual void SetVSyncDisplayID(int64_t display_id) {}

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

  // Acknowledges a SwapBuffers request without actually attempting to swap.
  // This should be called when the GPU thread decides to skip a swap that was
  // invoked by the viz thread to ensure that we still run the relevant metrics
  // bookkeeping.
  virtual void SwapBuffersSkipped(BufferPresentedCallback feedback,
                                  OutputSurfaceFrame frame);

  bool is_emulated_rgbx() const { return is_emulated_rgbx_; }

  void SetDrawTimings(base::TimeTicks submitted, base::TimeTicks started);

  void SetDependencyTimings(base::TimeTicks task_ready);

  // Copy and return the contents of the surface owned by this device. If this
  // output device is surfaceless, then reads back from the OS compositor tree,
  // including non-protected overlays.
  virtual void ReadbackForTesting(base::OnceCallback<void(SkBitmap)> callback);

 protected:
  // Only valid between StartSwapBuffers and FinishSwapBuffers.
  class SwapInfo {
   public:
    SwapInfo(uint64_t swap_id,
             BufferPresentedCallback feedback,
             base::TimeTicks viz_scheduled_draw,
             base::TimeTicks gpu_started_draw,
             base::TimeTicks task_ready);
    SwapInfo(SwapInfo&& other);
    ~SwapInfo();
    uint64_t SwapId();
    const gpu::SwapBuffersCompleteParams& Complete(
        gfx::SwapCompletionResult result,
        const std::optional<gfx::Rect>& damage_area,
        std::vector<gpu::Mailbox> released_overlays,
        int64_t swap_trace_id);
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
                    sk_sp<const GrDeferredDisplayList> ddl);
  virtual bool Draw(
      SkSurface* sk_surface,
      std::unique_ptr<skgpu::graphite::Recording> graphite_recording,
      base::OnceClosure on_finished);

  // Helper method for SwapBuffers() and PostSubBuffer(). It should be called
  // at the beginning of SwapBuffers() and PostSubBuffer() implementations
  void StartSwapBuffers(BufferPresentedCallback feedback);

  // Helper method for SwapBuffers() and PostSubBuffer(). It should be called
  // at the end of SwapBuffers() and PostSubBuffer() implementations
  void FinishSwapBuffers(
      gfx::SwapCompletionResult result,
      const gfx::Size& size,
      OutputSurfaceFrame frame,
      const std::optional<gfx::Rect>& damage_area = std::nullopt,
      std::vector<gpu::Mailbox> released_overlays = {});

  // TODO(crbug.com/40266876): Reset device on context loss to fix dangling ptr.
  const raw_ptr<GrDirectContext, DanglingUntriaged> gr_context_;
  const raw_ptr<skgpu::graphite::Context> graphite_context_;

  OutputSurface::Capabilities capabilities_;

  uint64_t swap_id_ = 0;
  DidSwapBufferCompleteCallback did_swap_buffer_complete_callback_;
  ReleaseOverlaysCallback release_overlays_callback_;

  base::queue<SwapInfo> pending_swaps_;
  base::TimeTicks viz_scheduled_draw_;
  base::TimeTicks gpu_started_draw_;
  base::TimeTicks gpu_task_ready_;

  // RGBX format is emulated with RGBA.
  bool is_emulated_rgbx_ = false;

  std::unique_ptr<gpu::MemoryTypeTracker> memory_type_tracker_;

 private:
  // A mapping from skipped swap ID to its corresponding OutputSurfaceFrame.
  base::flat_map<uint64_t, OutputSurfaceFrame> skipped_swap_info_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_H_
