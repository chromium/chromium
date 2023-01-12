// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_GL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_GL_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/viz/service/display_embedder/output_presenter.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "ui/gfx/ca_layer_result.h"

namespace gl {
class Presenter;
}  // namespace gl

namespace viz {

class VIZ_SERVICE_EXPORT OutputPresenterGL : public OutputPresenter {
 public:
  static const uint32_t kDefaultSharedImageUsage;

  OutputPresenterGL(
      scoped_refptr<gl::Presenter> presenter,
      SkiaOutputSurfaceDependency* deps,
      gpu::SharedImageFactory* factory,
      gpu::SharedImageRepresentationFactory* representation_factory,
      uint32_t shared_image_usage = kDefaultSharedImageUsage);
  ~OutputPresenterGL() override;

  // OutputPresenter implementation:
  void InitializeCapabilities(OutputSurface::Capabilities* capabilities) final;
  bool Reshape(const SkSurfaceCharacterization& characterization,
               const gfx::ColorSpace& color_space,
               float device_scale_factor,
               gfx::OverlayTransform transform) final;
  std::vector<std::unique_ptr<Image>> AllocateImages(
      gfx::ColorSpace color_space,
      gfx::Size image_size,
      size_t num_images) final;
  std::unique_ptr<Image> AllocateSingleImage(gfx::ColorSpace color_space,
                                             gfx::Size image_size) final;
  void SwapBuffers(SwapCompletionCallback completion_callback,
                   BufferPresentedCallback presentation_callback,
                   gfx::FrameData data) final;
  void PostSubBuffer(const gfx::Rect& rect,
                     SwapCompletionCallback completion_callback,
                     BufferPresentedCallback presentation_callback,
                     gfx::FrameData data) final;
  void CommitOverlayPlanes(SwapCompletionCallback completion_callback,
                           BufferPresentedCallback presentation_callback,
                           gfx::FrameData data) final;
  void SchedulePrimaryPlane(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane& plane,
      Image* image,
      bool is_submitted) final;
  void ScheduleOverlayPlane(
      const OutputPresenter::OverlayPlaneCandidate& overlay_plane_candidate,
      ScopedOverlayAccess* access,
      std::unique_ptr<gfx::GpuFence> acquire_fence) final;
  bool SupportsGpuVSync() const final;
  void SetGpuVSyncEnabled(bool enabled) final;
  void SetVSyncDisplayID(int64_t display_id) final;
#if BUILDFLAG(IS_MAC)
  void SetCALayerErrorCode(gfx::CALayerResult ca_layer_error_code) final;
#endif

 private:
  scoped_refptr<gl::Presenter> presenter_;
  raw_ptr<SkiaOutputSurfaceDependency> dependency_;
  const bool supports_async_swap_;

  ResourceFormat image_format_ = RGBA_8888;

  // Shared Image factories
  const raw_ptr<gpu::SharedImageFactory> shared_image_factory_;
  const raw_ptr<gpu::SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  uint32_t shared_image_usage_;

#if BUILDFLAG(IS_MAC)
  gfx::CALayerResult ca_layer_error_code_ = gfx::kCALayerSuccess;
#endif
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_GL_H_
