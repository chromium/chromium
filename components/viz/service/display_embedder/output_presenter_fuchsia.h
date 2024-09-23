// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_FUCHSIA_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_FUCHSIA_H_

#include <memory>
#include <vector>

#include "components/viz/service/display_embedder/output_presenter.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/ozone/public/overlay_plane.h"

namespace ui {
class PlatformWindowSurface;
}  // namespace ui

namespace viz {

class SkiaOutputSurfaceDependency;

class VIZ_SERVICE_EXPORT OutputPresenterFuchsia : public OutputPresenter {
 public:
  static std::unique_ptr<OutputPresenterFuchsia> Create(
      ui::PlatformWindowSurface* window_surface,
      SkiaOutputSurfaceDependency* deps);

  OutputPresenterFuchsia(ui::PlatformWindowSurface* window_surface,
                         SkiaOutputSurfaceDependency* deps);
  ~OutputPresenterFuchsia() override;

  // OutputPresenter implementation:
  void InitializeCapabilities(OutputSurface::Capabilities* capabilities) final;
  bool Reshape(const ReshapeParams& params) final;
  void Present(SwapCompletionCallback completion_callback,
               BufferPresentedCallback presentation_callback,
               gfx::FrameData data) final;
  void ScheduleOverlayPlane(
      const OutputPresenter::OverlayPlaneCandidate& overlay_plane_candidate,
      ScopedOverlayAccess* access,
      std::unique_ptr<gfx::GpuFence> acquire_fence) final;

 private:
  struct PendingFrame {
    PendingFrame();
    ~PendingFrame();

    PendingFrame(PendingFrame&&);
    PendingFrame& operator=(PendingFrame&&);

    // Primary plane pixmap.
    scoped_refptr<gfx::NativePixmap> native_pixmap;

    std::vector<gfx::GpuFenceHandle> acquire_fences;
    std::vector<gfx::GpuFenceHandle> release_fences;

    // Vector of overlays that are associated with this frame.
    std::vector<ui::OverlayPlane> overlays;
  };

  ui::PlatformWindowSurface* const window_surface_;
  SkiaOutputSurfaceDependency* const dependency_;

  // The next frame to be submitted by SwapBuffers().
  std::optional<PendingFrame> next_frame_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_FUCHSIA_H_
