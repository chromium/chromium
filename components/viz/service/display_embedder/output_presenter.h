// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_H_

#include <memory>
#include <vector>

#include "base/callback_helpers.h"
#include "build/build_config.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_surface.h"

namespace gpu {
class SharedImageFactory;
class SharedImageRepresentationFactory;
}  // namespace gpu

namespace viz {

class SkiaOutputSurfaceDependency;

class VIZ_SERVICE_EXPORT OutputPresenter {
 public:
  class Image {
   public:
    Image(gpu::SharedImageFactory* factory,
          gpu::SharedImageRepresentationFactory* representation_factory,
          SkiaOutputSurfaceDependency* deps);
    virtual ~Image();

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    virtual bool Initialize(const gfx::Size& size,
                            const gfx::ColorSpace& color_space,
                            SharedImageFormat format,
                            uint32_t shared_image_usage);

    gpu::SkiaImageRepresentation* skia_representation() {
      return skia_representation_.get();
    }

    void BeginWriteSkia(int sample_count);
    SkSurface* sk_surface();
    std::vector<GrBackendSemaphore> TakeEndWriteSkiaSemaphores();
    void EndWriteSkia(bool force_flush = false);
    void PreGrContextSubmit();

    // Set the image as purgeable. Returns false if the image was already
    // purgeable.
    bool SetPurgeable();
    void SetNotPurgeable();

    virtual void BeginPresent() = 0;
    virtual void EndPresent(gfx::GpuFenceHandle release_fence) = 0;
    virtual int GetPresentCount() const = 0;
    virtual void OnContextLost() = 0;

    const gpu::Mailbox& mailbox() const { return mailbox_; }

    base::WeakPtr<Image> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

   protected:
    const raw_ptr<gpu::SharedImageFactory> factory_;
    const raw_ptr<gpu::SharedImageRepresentationFactory>
        representation_factory_;
    const raw_ptr<SkiaOutputSurfaceDependency> deps_;
    gpu::Mailbox mailbox_;
    bool is_purgeable_ = false;

    std::unique_ptr<gpu::SkiaImageRepresentation> skia_representation_;
    std::unique_ptr<gpu::SkiaImageRepresentation::ScopedWriteAccess>
        scoped_skia_write_access_;

    std::unique_ptr<gpu::OverlayImageRepresentation> overlay_representation_;
    std::unique_ptr<gpu::OverlayImageRepresentation::ScopedReadAccess>
        scoped_overlay_read_access_;

    int present_count_ = 0;

    std::vector<GrBackendSemaphore> end_semaphores_;
    base::WeakPtrFactory<Image> weak_ptr_factory_{this};
  };

  OutputPresenter() = default;
  virtual ~OutputPresenter() = default;

  using BufferPresentedCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback& feedback)>;
  using SwapCompletionCallback =
      base::OnceCallback<void(gfx::SwapCompletionResult)>;

  virtual void InitializeCapabilities(
      OutputSurface::Capabilities* capabilities) = 0;
  virtual bool Reshape(const SkSurfaceCharacterization& characterization,
                       const gfx::ColorSpace& color_space,
                       float device_scale_factor,
                       gfx::OverlayTransform transform) = 0;
  virtual std::vector<std::unique_ptr<Image>> AllocateImages(
      gfx::ColorSpace color_space,
      gfx::Size image_size,
      size_t num_images) = 0;
  // This function exists because the Fuchsia call to 'AllocateImages' does not
  // support single image allocation.
  virtual std::unique_ptr<Image> AllocateSingleImage(
      gfx::ColorSpace color_space,
      gfx::Size image_size);
  virtual void SwapBuffers(SwapCompletionCallback completion_callback,
                           BufferPresentedCallback presentation_callback,
                           gl::FrameData data) = 0;
  virtual void PostSubBuffer(const gfx::Rect& rect,
                             SwapCompletionCallback completion_callback,
                             BufferPresentedCallback presentation_callback,
                             gl::FrameData data) = 0;
  virtual void CommitOverlayPlanes(
      SwapCompletionCallback completion_callback,
      BufferPresentedCallback presentation_callback,
      gl::FrameData data) = 0;
  virtual void SchedulePrimaryPlane(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane& plane,
      Image* image,
      bool is_submitted) = 0;
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_OZONE)
  using OverlayPlaneCandidate = OverlayCandidate;
#elif BUILDFLAG(IS_APPLE)
  using OverlayPlaneCandidate = CALayerOverlay;
#elif BUILDFLAG(IS_WIN)
  using OverlayPlaneCandidate = DCLayerOverlay;
#else
  // Default.
  using OverlayPlaneCandidate = OverlayCandidate;
#endif
  using ScopedOverlayAccess = gpu::OverlayImageRepresentation::ScopedReadAccess;
  virtual void ScheduleOverlayPlane(
      const OverlayPlaneCandidate& overlay_plane_candidate,
      ScopedOverlayAccess* access,
      std::unique_ptr<gfx::GpuFence> acquire_fence) = 0;
#if BUILDFLAG(IS_MAC)
  virtual void SetCALayerErrorCode(gfx::CALayerResult ca_layer_error_code) {}
#endif
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_H_
