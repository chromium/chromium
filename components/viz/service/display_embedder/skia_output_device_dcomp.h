// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_DCOMP_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_DCOMP_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "ui/gl/presenter.h"

namespace gl {
class DCLayerOverlayImage;
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
  SkiaOutputDeviceDComp(
      gpu::SharedImageRepresentationFactory*
          shared_image_representation_factory,
      gpu::SharedContextState* context_state,
      scoped_refptr<gl::Presenter> presenter,
      scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);

  SkiaOutputDeviceDComp(const SkiaOutputDeviceDComp&) = delete;
  SkiaOutputDeviceDComp& operator=(const SkiaOutputDeviceDComp&) = delete;

  ~SkiaOutputDeviceDComp() override;

  // SkiaOutputDevice implementation:
  void Present(const std::optional<gfx::Rect>& update_rect,
               BufferPresentedCallback feedback,
               OutputSurfaceFrame frame) override;
  void ScheduleOverlays(SkiaOutputSurface::OverlayList overlays) override;
  bool Reshape(const ReshapeParams& params) override;
  SkSurface* BeginPaint(
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndPaint() override;

 private:
  class OverlayData;

  std::optional<gl::DCLayerOverlayImage> BeginOverlayAccess(
      const gpu::Mailbox& mailbox);

  void CreateSkSurface();

  // Mailboxes of overlays scheduled in the current frame.
  base::flat_set<gpu::Mailbox> scheduled_overlay_mailboxes_;

  // Holds references to overlay textures so they aren't destroyed while in use.
  base::flat_map<gpu::Mailbox, OverlayData> overlays_;

  const raw_ptr<gpu::SharedImageRepresentationFactory>
      shared_image_representation_factory_;

  const raw_ptr<gpu::SharedContextState> context_state_;
  gfx::Size size_;

  // Completion callback for |DoPresent|.
  void OnPresentFinished(OutputSurfaceFrame frame,
                         const gfx::Size& swap_size,
                         gfx::SwapCompletionResult result);

  // Any implementation capable of scheduling a DComp layer. Currently only
  // |DCompPresenter|.
  scoped_refptr<gl::Presenter> presenter_;

  base::WeakPtrFactory<SkiaOutputDeviceDComp> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_DCOMP_H_
