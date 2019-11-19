// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_BUFFER_QUEUE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_BUFFER_QUEUE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/service/shared_image_factory.h"

namespace gl {
class GLSurface;
}  // namespace gl

namespace viz {

class SkiaOutputSurfaceDependency;

class VIZ_SERVICE_EXPORT SkiaOutputDeviceBufferQueue final
    : public SkiaOutputDevice {
 public:
  SkiaOutputDeviceBufferQueue(
      scoped_refptr<gl::GLSurface> gl_surface,
      SkiaOutputSurfaceDependency* deps,
      const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback);
  SkiaOutputDeviceBufferQueue(
      scoped_refptr<gl::GLSurface> gl_surface,
      SkiaOutputSurfaceDependency* deps,
      const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback,
      uint32_t shared_image_usage);
  ~SkiaOutputDeviceBufferQueue() override;

  void SwapBuffers(BufferPresentedCallback feedback,
                   std::vector<ui::LatencyInfo> latency_info) override;
  void PostSubBuffer(const gfx::Rect& rect,
                     BufferPresentedCallback feedback,
                     std::vector<ui::LatencyInfo> latency_info) override;
  bool Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               gfx::OverlayTransform transform) override;
  SkSurface* BeginPaint() override;
  void EndPaint(const GrBackendSemaphore& semaphore) override;
  bool supports_alpha() { return true; }

  gl::GLImage* GetOverlayImage() override;
  // Creates and submits gpu fence
  std::unique_ptr<gfx::GpuFence> SubmitOverlayGpuFence() override;

 private:
  friend class SkiaOutputDeviceBufferQueueTest;
  class Image;

  Image* GetCurrentImage();
  std::unique_ptr<Image> GetNextImage();
  void PageFlipComplete();
  void FreeAllSurfaces();
  // Used as callback for SwapBuffersAsync and PostSubBufferAsync to finish
  // operation
  void DoFinishSwapBuffers(const gfx::Size& size,
                           std::vector<ui::LatencyInfo> latency_info,
                           gfx::SwapResult result,
                           std::unique_ptr<gfx::GpuFence>);

  SkiaOutputSurfaceDependency* const dependency_;
  scoped_refptr<gl::GLSurface> gl_surface_;
  // Format of images
  gfx::ColorSpace color_space_;
  gfx::Size image_size_;
  ResourceFormat image_format_;

  // This image is currently used by Skia as RenderTarget. This may be nullptr
  // if no drawing in progress or if allocation failed at bind.
  std::unique_ptr<Image> current_image_;
  // The image currently on the screen, if any.
  std::unique_ptr<Image> displayed_image_;
  // These are free for use, and are not nullptr.
  std::vector<std::unique_ptr<Image>> available_images_;
  // These have been scheduled to display but are not displayed yet.
  // Entries of this deque may be nullptr, if they represent frames that have
  // been destroyed.
  base::circular_deque<std::unique_ptr<Image>> in_flight_images_;

  // Shared Image factories
  gpu::SharedImageFactory shared_image_factory_;
  std::unique_ptr<gpu::SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  uint32_t shared_image_usage_;

  base::WeakPtrFactory<SkiaOutputDeviceBufferQueue> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(SkiaOutputDeviceBufferQueue);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_BUFFER_QUEUE_H_
