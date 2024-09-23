// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_OUTPUT_SURFACE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_OUTPUT_SURFACE_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "build/build_config.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/display/external_use_client.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/display/render_pass_alpha_type.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/vulkan/buildflags.h"
#include "media/gpu/buildflags.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/gpu_fence_handle.h"

#if BUILDFLAG(IS_WIN)
#include "components/viz/service/display/dc_layer_overlay.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "components/viz/service/display/ca_layer_overlay.h"
#endif

class SkCanvas;
class SkImage;

namespace gfx {
class ColorSpace;
}  // namespace gfx

namespace viz {

class OverlayCandidate;
class ContextLostObserver;
class CopyOutputRequest;

namespace copy_output {
struct RenderPassGeometry;
}  // namespace copy_output

// This class extends the OutputSurface for SkiaRenderer needs.
class VIZ_SERVICE_EXPORT SkiaOutputSurface : public OutputSurface,
                                             public ExternalUseClient {
 public:
  using OverlayList = std::vector<OverlayCandidate>;

  SkiaOutputSurface();

  SkiaOutputSurface(const SkiaOutputSurface&) = delete;
  SkiaOutputSurface& operator=(const SkiaOutputSurface&) = delete;

  ~SkiaOutputSurface() override;

  SkiaOutputSurface* AsSkiaOutputSurface() override;

  // Begin painting the current frame. This method will create a
  // GrDeferredDisplayListRecorder and return a SkCanvas of it.
  // The SkiaRenderer will use this SkCanvas to paint the current
  // frame.
  // And this SkCanvas may become invalid, when FinishPaintCurrentFrame is
  // called.
  virtual SkCanvas* BeginPaintCurrentFrame() = 0;

  // Make a promise SkImage from the given |image_context|. The SkiaRenderer can
  // use the image with SkCanvas returned by |GetSkCanvasForCurrentFrame|, but
  // Skia will not read the content of the resource until the |sync_token| in
  // the |image_context| is satisfied. The SwapBuffers should take care of this
  // by scheduling a GPU task with all resource sync tokens recorded by
  // MakePromiseSkImage for the current frame. The |yuv_color_space| is the
  // original color space needed for yuv to rgb conversion.
  virtual void MakePromiseSkImage(
      ExternalUseClient::ImageContext* image_context,
      const gfx::ColorSpace& yuv_color_space,
      bool force_rgbx) = 0;

  // Called if SwapBuffers() will be skipped.
  virtual void SwapBuffersSkipped(const gfx::Rect root_pass_damage_rect) = 0;

  // Begin painting a render pass. This method will create a
  // GrDeferredDisplayListRecorder and return a SkCanvas of it. The SkiaRenderer
  // will use this SkCanvas to paint the render pass.
  // Note: BeginPaintRenderPass cannot be called without finishing the prior
  // paint render pass.
  virtual SkCanvas* BeginPaintRenderPass(const AggregatedRenderPassId& id,
                                         const gfx::Size& size,
                                         SharedImageFormat format,
                                         RenderPassAlphaType alpha_type,
                                         skgpu::Mipmapped mipmap,
                                         bool scanout_dcomp_surface,
                                         sk_sp<SkColorSpace> color_space,
                                         bool is_overlay,
                                         const gpu::Mailbox& mailbox) = 0;

  // Create an overdraw recorder for the current paint which will be drawn on
  // top of the current canvas when EndPaint() is called. Returns the new
  // wrapped SkCanvas to be used by SkiaRenderer.
  // This should be called for the root render pass only when
  // debug_settings.show_overdraw_feedback = true.
  virtual SkCanvas* RecordOverdrawForCurrentPaint() = 0;

  // Finish painting the current frame or current render pass, depends on which
  // BeginPaint function is called last. This method will schedule a GPU task to
  // play the DDL back on GPU thread on a cached SkSurface.
  // Optionally the caller may specify |on_finished| callback to be called after
  // the GPU has finished processing all submitted commands. The callback may be
  // called on a different thread. The caller may also specify
  // |return_release_fence_cb| callback to be called after all commands are
  // submitted. The callback will return the release fence which will be
  // signaled once the submitted commands are processed.
  // |update_rect| should be the scissor rect used to clear the render pass
  // backing and cull its draw quads.
  // When finishing painting of a render pass that will be presented as an
  // overlay, |is_overlay| should be true so the GPU thread knows to keep the
  // ScopedWriteAccess open long enough.
  virtual void EndPaint(
      base::OnceClosure on_finished,
      base::OnceCallback<void(gfx::GpuFenceHandle)> return_release_fence_cb,
      const gfx::Rect& update_rect,
      bool is_overlay) = 0;

  // Make a promise SkImage from a render pass id. The render pass has been
  // painted with BeginPaintRenderPass and FinishPaintRenderPass. The format
  // and mipmap must match arguments used for BeginPaintRenderPass() to paint
  // this render pass.
  virtual sk_sp<SkImage> MakePromiseSkImageFromRenderPass(
      const AggregatedRenderPassId& id,
      const gfx::Size& size,
      SharedImageFormat format,
      bool mipmap,
      sk_sp<SkColorSpace> color_space,
      const gpu::Mailbox& mailbox) = 0;

  // Remove cached resources generated by BeginPaintRenderPass and
  // FinishPaintRenderPass.
  virtual void RemoveRenderPassResource(
      std::vector<AggregatedRenderPassId> ids) = 0;

  // Copy the output of the current frame if the |mailbox| is zero, otherwise
  // create an SkSurface for the given |mailbox| and copy the output.
  virtual void CopyOutput(const copy_output::RenderPassGeometry& geometry,
                          const gfx::ColorSpace& color_space,
                          std::unique_ptr<CopyOutputRequest> request,
                          const gpu::Mailbox& mailbox) = 0;

  // Schedule drawing overlays at next SwapBuffers() call. Waits on
  // |sync_tokens| for the overlay textures to be ready before scheduling.
  // Optionally the caller may specify |on_finished| callback to be called after
  // the GPU has finished processing all submitted commands. The callback may be
  // called on a different thread.
  virtual void ScheduleOverlays(OverlayList overlays,
                                std::vector<gpu::SyncToken> sync_tokens) = 0;

  // Add context lost observer.
  virtual void AddContextLostObserver(ContextLostObserver* observer) = 0;

  // Remove context lost observer.
  virtual void RemoveContextLostObserver(ContextLostObserver* observer) = 0;

  // Only used for SkiaOutputSurfaceImpl unit tests.
  virtual void ScheduleGpuTaskForTesting(
      base::OnceClosure callback,
      std::vector<gpu::SyncToken> sync_tokens) = 0;
  // TODO(crbug.com/40279197): tests should not need to poll for async work
  // completion.
  virtual void CheckAsyncWorkCompletionForTesting() = 0;

  // Android specific, asks GLSurfaceEGLSurfaceControl to not detach child
  // surface controls during destruction. This is necessary for cases when we
  // switch from chrome to other app, the OS will take care of the cleanup.
  virtual void PreserveChildSurfaceControls() = 0;

  // Flush pending GPU tasks. This method returns a sync token which can be
  // waited on in a command buffer to ensure all pending tasks are executed on
  // the GPU main thread.
  virtual gpu::SyncToken Flush() = 0;

  // Enqueue a GPU task to create a shared image with the specified params and
  // returns the mailbox.
  // Note: |kTopLeft_GrSurfaceOrigin| is used for all images.
  virtual gpu::Mailbox CreateSharedImage(SharedImageFormat format,
                                         const gfx::Size& size,
                                         const gfx::ColorSpace& color_space,
                                         RenderPassAlphaType alpha_type,
                                         gpu::SharedImageUsageSet usage,
                                         std::string_view debug_label,
                                         gpu::SurfaceHandle surface_handle) = 0;

  // Enqueue a GPU task to create a 1x1 shared image of the specified color.
  virtual gpu::Mailbox CreateSolidColorSharedImage(
      const SkColor4f& color,
      const gfx::ColorSpace& color_space) = 0;

  // Enqueue a GPU task to delete the specified shared image.
  virtual void DestroySharedImage(const gpu::Mailbox& mailbox) = 0;

  // Enqueue a GPU task to set specified shared image as `purgeable`.
  virtual void SetSharedImagePurgeable(const gpu::Mailbox& mailbox,
                                       bool purgeable) = 0;

  virtual bool SupportsBGRA() const = 0;

#if BUILDFLAG(ENABLE_VULKAN) && BUILDFLAG(IS_CHROMEOS) && \
    BUILDFLAG(USE_V4L2_CODEC)
  virtual void DetileOverlay(gpu::Mailbox input,
                             const gfx::Size& input_visible_size,
                             gpu::SyncToken input_sync_token,
                             gpu::Mailbox output,
                             const gfx::RectF& display_rect,
                             const gfx::RectF& crop_rect,
                             gfx::OverlayTransform transform,
                             bool is_10bit) = 0;

  virtual void CleanupImageProcessor() = 0;
#endif
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_OUTPUT_SURFACE_H_
