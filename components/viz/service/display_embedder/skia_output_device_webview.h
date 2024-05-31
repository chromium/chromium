// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_WEBVIEW_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_WEBVIEW_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/service/display_embedder/skia_output_device.h"

namespace gl {
class GLSurface;
}  // namespace gl

namespace gpu {
class SharedContextState;
}  // namespace gpu

namespace viz {

class SkiaOutputDeviceWebView : public SkiaOutputDevice {
 public:
  SkiaOutputDeviceWebView(
      gpu::SharedContextState* context_state,
      scoped_refptr<gl::GLSurface> gl_surface,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);
  SkiaOutputDeviceWebView(const SkiaOutputDeviceWebView&) = delete;
  SkiaOutputDeviceWebView& operator=(const SkiaOutputDeviceWebView&) = delete;
  ~SkiaOutputDeviceWebView() override;

  // SkiaOutputDevice implementation:
  bool Reshape(const ReshapeParams& params) override;
  void Present(const std::optional<gfx::Rect>& update_rect,
               BufferPresentedCallback feedback,
               OutputSurfaceFrame frame) override;

  SkSurface* BeginPaint(
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndPaint() override;

 private:
  void InitSkiaSurface(unsigned int fbo);

  const raw_ptr<gpu::SharedContextState> context_state_;
  scoped_refptr<gl::GLSurface> gl_surface_;

  sk_sp<SkSurface> sk_surface_;

  gfx::Size size_;
  sk_sp<SkColorSpace> sk_color_space_;
  unsigned int last_frame_buffer_object_ = -1;

  base::WeakPtrFactory<SkiaOutputDeviceWebView> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_WEBVIEW_H_
