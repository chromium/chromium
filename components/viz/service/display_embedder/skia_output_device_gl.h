// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_GL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_GL_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/service/display_embedder/skia_output_device.h"

namespace gl {
class GLImage;
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

class SkiaOutputDeviceGL final : public SkiaOutputDevice {
 public:
  SkiaOutputDeviceGL(
      gpu::MailboxManager* mailbox_manager,
      gpu::SharedImageRepresentationFactory*
          shared_image_representation_factory,
      gpu::SharedContextState* context_state,
      scoped_refptr<gl::GLSurface> gl_surface,
      scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);
  ~SkiaOutputDeviceGL() override;

  // SkiaOutputDevice implementation:
  bool Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               gfx::BufferFormat format,
               gfx::OverlayTransform transform) override;
  void SwapBuffers(BufferPresentedCallback feedback,
                   std::vector<ui::LatencyInfo> latency_info) override;
  void PostSubBuffer(const gfx::Rect& rect,
                     BufferPresentedCallback feedback,
                     std::vector<ui::LatencyInfo> latency_info) override;
  void CommitOverlayPlanes(BufferPresentedCallback feedback,
                           std::vector<ui::LatencyInfo> latency_info) override;
  bool SetDrawRectangle(const gfx::Rect& draw_rectangle) override;
  void SetGpuVSyncEnabled(bool enabled) override;
  void SetEnableDCLayers(bool enable) override;
  void ScheduleOverlays(SkiaOutputSurface::OverlayList overlays) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  SkSurface* BeginPaint(
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndPaint() override;

 private:
  // Used as callback for SwapBuffersAsync and PostSubBufferAsync to finish
  // operation
  void DoFinishSwapBuffers(const gfx::Size& size,
                           std::vector<ui::LatencyInfo> latency_info,
                           gfx::SwapCompletionResult result);

  scoped_refptr<gl::GLImage> GetGLImageForMailbox(const gpu::Mailbox& mailbox);

  gpu::MailboxManager* const mailbox_manager_;

  gpu::SharedImageRepresentationFactory* const
      shared_image_representation_factory_;

  gpu::SharedContextState* const context_state_;
  scoped_refptr<gl::GLSurface> gl_surface_;
  const bool supports_async_swap_;

  sk_sp<SkSurface> sk_surface_;

  uint64_t backbuffer_estimated_size_ = 0;

  base::WeakPtrFactory<SkiaOutputDeviceGL> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputDeviceGL);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_GL_H_
