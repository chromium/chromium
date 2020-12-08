// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_X11_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_X11_H_

#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "components/viz/service/display_embedder/output_presenter.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image_factory.h"

namespace base {
class Thread;
}
namespace viz {

class VIZ_SERVICE_EXPORT OutputPresenterX11 : public OutputPresenter {
 public:
  static const uint32_t kDefaultSharedImageUsage;

  static std::unique_ptr<OutputPresenterX11> Create(
      SkiaOutputSurfaceDependency* deps,
      gpu::SharedImageFactory* factory,
      gpu::SharedImageRepresentationFactory* representation_factory);

  OutputPresenterX11(
      SkiaOutputSurfaceDependency* deps,
      gpu::SharedImageFactory* factory,
      gpu::SharedImageRepresentationFactory* representation_factory,
      uint32_t shared_image_usage = kDefaultSharedImageUsage);
  ~OutputPresenterX11() override;

  // OutputPresenter implementation:
  void InitializeCapabilities(OutputSurface::Capabilities* capabilities) final;
  bool Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               gfx::BufferFormat format,
               gfx::OverlayTransform transform) final;
  std::vector<std::unique_ptr<Image>> AllocateImages(
      gfx::ColorSpace color_space,
      gfx::Size image_size,
      size_t num_images) final;
  void SwapBuffers(SwapCompletionCallback completion_callback,
                   BufferPresentedCallback presentation_callback) final;
  void PostSubBuffer(const gfx::Rect& rect,
                     SwapCompletionCallback completion_callback,
                     BufferPresentedCallback presentation_callback) final;
  void CommitOverlayPlanes(SwapCompletionCallback completion_callback,
                           BufferPresentedCallback presentation_callback) final;
  void SchedulePrimaryPlane(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane& plane,
      Image* image,
      bool is_submitted) final;
  void ScheduleOverlays(SkiaOutputSurface::OverlayList overlays,
                        std::vector<ScopedOverlayAccess*> accesses) final;

 private:
  bool Initialize();

  SkiaOutputSurfaceDependency* dependency_;

  // Shared Image factories
  gpu::SharedImageFactory* const shared_image_factory_;
  gpu::SharedImageRepresentationFactory* const
      shared_image_representation_factory_;
  const uint32_t shared_image_usage_;

  int depth_ = 0;

  ResourceFormat image_format_ = BGRA_8888;

  std::vector<std::vector<uint64_t>> modifier_vectors_;
  bool supports_rgba_ = false;

  Image* scheduled_image_ = nullptr;

  // Thread for using present extension to present pixmap.
  std::unique_ptr<base::Thread> x11_thread_;

  class OnX11;
  std::unique_ptr<OnX11> on_x11_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_X11_H_
