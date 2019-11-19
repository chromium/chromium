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
#include "build/build_config.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "gpu/command_buffer/common/mailbox.h"

class GrContext;

namespace gl {
class GLContext;
class GLImage;
class GLSurface;
}  // namespace gl

namespace gfx {
class GpuFence;
}  // namespace gfx

namespace gpu {
class MailboxManager;

namespace gles2 {
class FeatureInfo;
}  // namespace gles2
}  // namespace gpu

namespace viz {

class SkiaOutputDeviceGL final : public SkiaOutputDevice {
 public:
  SkiaOutputDeviceGL(
      gpu::MailboxManager* mailbox_manager,
      scoped_refptr<gl::GLSurface> gl_surface,
      scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
      const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback);
  ~SkiaOutputDeviceGL() override;

  void Initialize(GrContext* gr_context, gl::GLContext* gl_context);
  bool supports_alpha() {
    DCHECK(gr_context_);
    return supports_alpha_;
  }

  // SkiaOutputDevice implementation:
  bool Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               gfx::OverlayTransform transform) override;
  void SwapBuffers(BufferPresentedCallback feedback,
                   std::vector<ui::LatencyInfo> latency_info) override;
  void PostSubBuffer(const gfx::Rect& rect,
                     BufferPresentedCallback feedback,
                     std::vector<ui::LatencyInfo> latency_info) override;
  void SetDrawRectangle(const gfx::Rect& draw_rectangle) override;
  void SetGpuVSyncEnabled(bool enabled) override;
#if defined(OS_WIN)
  void SetEnableDCLayers(bool enable) override;
  void ScheduleDCLayers(std::vector<DCLayerOverlay> dc_layers) override;
#endif
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  SkSurface* BeginPaint() override;
  void EndPaint(const GrBackendSemaphore& semaphore) override;

 private:
  scoped_refptr<gl::GLImage> GetGLImageForMailbox(const gpu::Mailbox& mailbox);

  gpu::MailboxManager* const mailbox_manager_;

  scoped_refptr<gl::GLSurface> gl_surface_;
  GrContext* gr_context_ = nullptr;

  sk_sp<SkSurface> sk_surface_;

  bool supports_alpha_ = false;

  base::WeakPtrFactory<SkiaOutputDeviceGL> weak_ptr_factory_{this};

  // Used as callback for SwapBuffersAsync and PostSubBufferAsync to finish
  // operation
  void DoFinishSwapBuffers(const gfx::Size& size,
                           std::vector<ui::LatencyInfo> latency_info,
                           gfx::SwapResult result,
                           std::unique_ptr<gfx::GpuFence>);

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputDeviceGL);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_GL_H_
