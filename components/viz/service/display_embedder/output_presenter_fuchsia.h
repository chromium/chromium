// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_FUCHSIA_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_FUCHSIA_H_

#include <fuchsia/images/cpp/fidl.h>

#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "components/viz/service/display_embedder/output_presenter.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image_factory.h"

namespace ui {
class PlatformWindowSurface;
}  // namespace ui

namespace viz {

class VIZ_SERVICE_EXPORT OutputPresenterFuchsia : public OutputPresenter {
 public:
  static std::unique_ptr<OutputPresenterFuchsia> Create(
      ui::PlatformWindowSurface* window_surface,
      SkiaOutputSurfaceDependency* deps,
      gpu::SharedImageFactory* shared_image_factory,
      gpu::SharedImageRepresentationFactory* representation_factory);

  OutputPresenterFuchsia(
      fuchsia::images::ImagePipe2Ptr image_pipe,
      SkiaOutputSurfaceDependency* deps,
      gpu::SharedImageFactory* shared_image_factory,
      gpu::SharedImageRepresentationFactory* representation_factory);
  ~OutputPresenterFuchsia() override;

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
  struct PendingFrame {
    PendingFrame();
    ~PendingFrame();

    PendingFrame(PendingFrame&&);
    PendingFrame& operator=(PendingFrame&&);

    uint32_t buffer_collection_id;
    uint32_t image_id;

    std::vector<zx::event> acquire_fences;
    std::vector<zx::event> release_fences;

    SwapCompletionCallback completion_callback;
    BufferPresentedCallback presentation_callback;

    // Indicates that this is the last frame for this buffer collection and that
    // the collection can be removed after the frame is presented.
    bool remove_buffer_collection = false;
  };

  void PresentNextFrame();
  void OnPresentComplete(fuchsia::images::PresentationInfo presentation_info);

  fuchsia::sysmem::AllocatorPtr sysmem_allocator_;
  fuchsia::images::ImagePipe2Ptr image_pipe_;
  SkiaOutputSurfaceDependency* const dependency_;
  gpu::SharedImageFactory* const shared_image_factory_;
  gpu::SharedImageRepresentationFactory* const
      shared_image_representation_factory_;

  gfx::Size frame_size_;
  gfx::BufferFormat buffer_format_ = gfx::BufferFormat::RGBA_8888;

  // Last buffer collection ID for the ImagePipe. Incremented every time buffers
  // are reallocated.
  uint32_t last_buffer_collection_id_ = 0;

  // Counter to generate image IDs for the ImagePipe.
  uint32_t last_image_id_ = 0;

  std::unique_ptr<gpu::SysmemBufferCollection> buffer_collection_;

  // The next frame to be submitted by SwapBuffers().
  base::Optional<PendingFrame> next_frame_;

  base::circular_deque<PendingFrame> pending_frames_;

  bool present_is_pending_ = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_FUCHSIA_H_
