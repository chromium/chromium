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
#include "ui/gfx/frame_data.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/presenter.h"

namespace gl {
class DCLayerOverlayImage;
struct DCLayerOverlayParams;
}  // namespace gl

namespace gpu {
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
  void Present(const absl::optional<gfx::Rect>& update_rect,
               BufferPresentedCallback feedback,
               OutputSurfaceFrame frame) override;
  void ScheduleOverlays(SkiaOutputSurface::OverlayList overlays) override;

 protected:
  class OverlayData;

  SkiaOutputDeviceDComp(
      gpu::SharedImageRepresentationFactory*
          shared_image_representation_factory,
      gpu::SharedContextState* context_state,
      scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);

  absl::optional<gl::DCLayerOverlayImage> BeginOverlayAccess(
      const gpu::Mailbox& mailbox);

  void CreateSkSurface();

  virtual bool ScheduleDCLayer(
      std::unique_ptr<gl::DCLayerOverlayParams> params) = 0;

  virtual void DoPresent(
      const gfx::Rect& rect,
      gl::GLSurface::SwapCompletionCallback completion_callback,
      BufferPresentedCallback feedback,
      gfx::FrameData data) = 0;

  // Mailboxes of overlays scheduled in the current frame.
  base::flat_set<gpu::Mailbox> scheduled_overlay_mailboxes_;

  // Holds references to overlay textures so they aren't destroyed while in use.
  base::flat_map<gpu::Mailbox, OverlayData> overlays_;

  const raw_ptr<gpu::SharedImageRepresentationFactory>
      shared_image_representation_factory_;

  const raw_ptr<gpu::SharedContextState> context_state_;
  gfx::Size size_;

 private:
  // Completion callback for |DoPresent|.
  void OnPresentFinished(OutputSurfaceFrame frame,
                         const gfx::Size& swap_size,
                         gfx::SwapCompletionResult result);

  base::WeakPtrFactory<SkiaOutputDeviceDComp> weak_ptr_factory_{this};
};

// A DComp-backed OutputDevice whose root surface is wrapped in a GLSurface.
// It is intended to be replaced by |SkiaOutputDeviceDCompPresenter| when
// |DirectCompositionSurfaceWin| is removed.
class VIZ_SERVICE_EXPORT SkiaOutputDeviceDCompGLSurface final
    : public SkiaOutputDeviceDComp {
 public:
  SkiaOutputDeviceDCompGLSurface(
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
      std::unique_ptr<gl::DCLayerOverlayParams> params) override;
  void DoPresent(const gfx::Rect& rect,
                 gl::GLSurface::SwapCompletionCallback completion_callback,
                 BufferPresentedCallback feedback,
                 gfx::FrameData data) override;

 private:
  scoped_refptr<gl::GLSurface> gl_surface_;

  gfx::ColorSpace color_space_;
  GrGLFramebufferInfo framebuffer_info_ = {};
  sk_sp<SkSurface> sk_surface_;

  uint64_t backbuffer_estimated_size_ = 0;
};

// A DComp-backed OutputDevice that directly owns the root surface.
class VIZ_SERVICE_EXPORT SkiaOutputDeviceDCompPresenter final
    : public SkiaOutputDeviceDComp {
 public:
  SkiaOutputDeviceDCompPresenter(
      gpu::SharedImageRepresentationFactory*
          shared_image_representation_factory,
      gpu::SharedContextState* context_state,
      scoped_refptr<gl::Presenter> presenter,
      scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);

  ~SkiaOutputDeviceDCompPresenter() override;

  // SkiaOutputDevice implementation:
  bool Reshape(const SkSurfaceCharacterization& characterization,
               const gfx::ColorSpace& color_space,
               float device_scale_factor,
               gfx::OverlayTransform transform) override;
  bool SetDrawRectangle(const gfx::Rect& draw_rectangle) override;
  void SetGpuVSyncEnabled(bool enabled) override;
  SkSurface* BeginPaint(
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndPaint() override;
  bool IsPrimaryPlaneOverlay() const override;

 protected:
  bool ScheduleDCLayer(
      std::unique_ptr<gl::DCLayerOverlayParams> params) override;
  void DoPresent(const gfx::Rect& rect,
                 gl::Presenter::SwapCompletionCallback completion_callback,
                 BufferPresentedCallback feedback,
                 gfx::FrameData data) override;

 private:
  // Any implementation capable of scheduling a DComp layer. Currently only
  // |DCompPresenter|.
  scoped_refptr<gl::Presenter> presenter_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_DCOMP_H_
