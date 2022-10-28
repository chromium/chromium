// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_DCOMP_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_DCOMP_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

namespace gl {
class GLSurface;
}  // namespace gl

namespace gpu {
class MailboxManager;
class SharedContextState;
class SharedImageRepresentationFactory;

namespace gles2 {
class FeatureInfo;
}  // namespace gles2
}  // namespace gpu

namespace viz {

class SkiaOutputDeviceDComp final : public SkiaOutputDevice {
 public:
  SkiaOutputDeviceDComp(
      gpu::MailboxManager* mailbox_manager,
      gpu::SharedImageRepresentationFactory*
          shared_image_representation_factory,
      gpu::SharedContextState* context_state,
      scoped_refptr<gl::GLSurface> gl_surface,
      scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);

  SkiaOutputDeviceDComp(const SkiaOutputDeviceDComp&) = delete;
  SkiaOutputDeviceDComp& operator=(const SkiaOutputDeviceDComp&) = delete;

  ~SkiaOutputDeviceDComp() override;

  // SkiaOutputDevice implementation:
  bool Reshape(const SkSurfaceCharacterization& characterization,
               const gfx::ColorSpace& color_space,
               float device_scale_factor,
               gfx::OverlayTransform transform) override;
  void SwapBuffers(BufferPresentedCallback feedback,
                   OutputSurfaceFrame frame) override;
  void PostSubBuffer(const gfx::Rect& rect,
                     BufferPresentedCallback feedback,
                     OutputSurfaceFrame frame) override;
  void CommitOverlayPlanes(BufferPresentedCallback feedback,
                           OutputSurfaceFrame frame) override;
  bool SetDrawRectangle(const gfx::Rect& draw_rectangle) override;
  void SetGpuVSyncEnabled(bool enabled) override;
  void SetEnableDCLayers(bool enable) override;
  void ScheduleOverlays(SkiaOutputSurface::OverlayList overlays) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  SkSurface* BeginPaint(
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndPaint() override;

 private:
  class OverlayData;

  // Use instead of calling FinishSwapBuffers() directly. On Windows this cleans
  // up old entries in |overlays_|.
  void DoFinishSwapBuffers(const gfx::Size& size,
                           OutputSurfaceFrame frame,
                           gfx::SwapCompletionResult result);
  // Used as callback for SwapBuffersAsync and PostSubBufferAsync to finish
  // operation
  void DoFinishSwapBuffersAsync(const gfx::Size& size,
                                OutputSurfaceFrame frame,
                                gfx::SwapCompletionResult result);

  gpu::OverlayImageRepresentation::ScopedReadAccess* BeginOverlayAccess(
      const gpu::Mailbox& mailbox);

  void CreateSkSurface();

  // Mailboxes of overlays scheduled in the current frame.
  base::flat_set<gpu::Mailbox> scheduled_overlay_mailboxes_;

  // Holds references to overlay textures so they aren't destroyed while in use.
  base::flat_map<gpu::Mailbox, OverlayData> overlays_;

  const raw_ptr<gpu::MailboxManager> mailbox_manager_;

  const raw_ptr<gpu::SharedImageRepresentationFactory>
      shared_image_representation_factory_;

  const raw_ptr<gpu::SharedContextState> context_state_;
  scoped_refptr<gl::GLSurface> gl_surface_;
  const bool supports_async_swap_;

  uint64_t backbuffer_estimated_size_ = 0;

  gfx::Size size_;
  SkColorType color_type_;
  gfx::ColorSpace color_space_;
  GrGLFramebufferInfo framebuffer_info_ = {};
  int sample_count_ = 1;
  sk_sp<SkSurface> sk_surface_;

  base::WeakPtrFactory<SkiaOutputDeviceDComp> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_DCOMP_H_
