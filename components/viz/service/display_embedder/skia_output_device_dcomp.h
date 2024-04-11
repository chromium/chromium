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
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gfx/frame_data.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/presenter.h"

namespace gl {
class DCLayerOverlayImage;
}  // namespace gl

namespace gpu {
class SharedContextState;
class SharedImageRepresentationFactory;
class SharedImageFactory;
class SharedImageManager;

namespace gles2 {
class FeatureInfo;
}  // namespace gles2

namespace raster {
class GrShaderCache;
}  // namespace raster
}  // namespace gpu

namespace viz {

class SkiaOutputSurfaceDependency;

// Base class for DComp-backed OutputDevices.
class SkiaOutputDeviceDComp : public SkiaOutputDevice {
 public:
  SkiaOutputDeviceDComp(
      SkiaOutputSurfaceDependency* deps,
      gpu::SharedImageFactory* shared_image_factory,
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
  bool Reshape(const SkImageInfo& image_info,
               const gfx::ColorSpace& color_space,
               int sample_count,
               float device_scale_factor,
               gfx::OverlayTransform transform) override;
  SkSurface* BeginPaint(
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndPaint() override;
  bool IsPrimaryPlaneOverlay() const override;

 private:
  class OverlayData;

  std::optional<gl::DCLayerOverlayImage> BeginOverlayAccess(
      const gpu::Mailbox& mailbox);

  void CreateSkSurface();

  // Populate |overlays_| with DComp surfaces that contain copies of overlays
  // with non-scanout resources.
  bool EnsureDCompSurfaceCopiesForNonOverlayResources(
      const SkiaOutputSurface::OverlayList& overlays);

  // Force the next present to return |gfx::SwapResult::SWAP_FAILED|. This
  // function allows |ScheduleOverlays| to be fallible.
  void ForceFailureOnNextSwap();

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

  bool force_failure_on_next_swap_ = false;

  const raw_ptr<gpu::raster::GrShaderCache> gr_shader_cache_;
  const raw_ref<gpu::SharedImageManager> shared_image_manager_;
  const raw_ref<gpu::SharedImageFactory> shared_image_factory_;

  base::WeakPtrFactory<SkiaOutputDeviceDComp> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_DCOMP_H_
