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

// Base class for DComp-backed OutputDevices.
class SkiaOutputDeviceDComp : public SkiaOutputDevice {
 public:
  SkiaOutputDeviceDComp(const SkiaOutputDeviceDComp&) = delete;
  SkiaOutputDeviceDComp& operator=(const SkiaOutputDeviceDComp&) = delete;

  ~SkiaOutputDeviceDComp() override;

  // SkiaOutputDevice implementation:
  void SwapBuffers(BufferPresentedCallback feedback,
                   OutputSurfaceFrame frame) override;
  void PostSubBuffer(const gfx::Rect& rect,
                     BufferPresentedCallback feedback,
                     OutputSurfaceFrame frame) override;
  void ScheduleOverlays(SkiaOutputSurface::OverlayList overlays) override;

 protected:
  SkiaOutputDeviceDComp(
      gpu::MailboxManager* mailbox_manager,
      gpu::SharedImageRepresentationFactory*
          shared_image_representation_factory,
      gpu::SharedContextState* context_state,
      gl::GLSurface* gl_surface,
      scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);

  class OverlayData;

  gpu::OverlayImageRepresentation::ScopedReadAccess* BeginOverlayAccess(
      const gpu::Mailbox& mailbox);

  void CreateSkSurface();

  virtual bool ScheduleDCLayer(
      std::unique_ptr<ui::DCRendererLayerParams> params) = 0;

  virtual gfx::Size GetRootSurfaceSize() const = 0;

  virtual gfx::SwapResult DoPostSubBuffer(const gfx::Rect& rect,
                                          BufferPresentedCallback feedback,
                                          gl::FrameData data) = 0;

  // Mailboxes of overlays scheduled in the current frame.
  base::flat_set<gpu::Mailbox> scheduled_overlay_mailboxes_;

  // Holds references to overlay textures so they aren't destroyed while in use.
  base::flat_map<gpu::Mailbox, OverlayData> overlays_;

  const raw_ptr<gpu::MailboxManager> mailbox_manager_;

  const raw_ptr<gpu::SharedImageRepresentationFactory>
      shared_image_representation_factory_;

  const raw_ptr<gpu::SharedContextState> context_state_;

  base::WeakPtrFactory<SkiaOutputDeviceDComp> weak_ptr_factory_{this};
};

// A DComp-backed OutputDevice whose root surface is wrapped in a GLSurface.
class VIZ_SERVICE_EXPORT SkiaOutputDeviceDCompGLSurface final
    : public SkiaOutputDeviceDComp {
 public:
  SkiaOutputDeviceDCompGLSurface(
      gpu::MailboxManager* mailbox_manager,
      gpu::SharedImageRepresentationFactory*
          shared_image_representation_factory,
      gpu::SharedContextState* context_state,
      scoped_refptr<gl::GLSurface> gl_surface,
      scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);

  ~SkiaOutputDeviceDCompGLSurface() override;

  // SkiaOutputDevice implementation:
  bool Reshape(const SkSurfaceCharacterization& characterization,
               const gfx::ColorSpace& color_space,
               float device_scale_factor,
               gfx::OverlayTransform transform) override;
  bool SetDrawRectangle(const gfx::Rect& draw_rectangle) override;
  void SetEnableDCLayers(bool enable) override;
  void SetGpuVSyncEnabled(bool enabled) override;
  SkSurface* BeginPaint(
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndPaint() override;

 protected:
  bool ScheduleDCLayer(
      std::unique_ptr<ui::DCRendererLayerParams> params) override;
  gfx::Size GetRootSurfaceSize() const override;
  gfx::SwapResult DoPostSubBuffer(const gfx::Rect& rect,
                                  BufferPresentedCallback feedback,
                                  gl::FrameData data) override;

 private:
  scoped_refptr<gl::GLSurface> gl_surface_;

  gfx::Size size_;
  gfx::ColorSpace color_space_;
  GrGLFramebufferInfo framebuffer_info_ = {};
  sk_sp<SkSurface> sk_surface_;

  uint64_t backbuffer_estimated_size_ = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_DCOMP_H_
