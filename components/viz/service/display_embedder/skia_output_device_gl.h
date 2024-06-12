// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_GL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_GL_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

namespace gl {
class GLSurface;
}  // namespace gl

namespace gpu {
class SharedContextState;

namespace gles2 {
class FeatureInfo;
}  // namespace gles2
}  // namespace gpu

namespace viz {

class SkiaOutputDeviceGL final : public SkiaOutputDevice {
 public:
  SkiaOutputDeviceGL(
      gpu::SharedContextState* context_state,
      scoped_refptr<gl::GLSurface> gl_surface,
      scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);

  SkiaOutputDeviceGL(const SkiaOutputDeviceGL&) = delete;
  SkiaOutputDeviceGL& operator=(const SkiaOutputDeviceGL&) = delete;

  ~SkiaOutputDeviceGL() override;

  // SkiaOutputDevice implementation:
  bool Reshape(const ReshapeParams& params) override;
  void Present(const std::optional<gfx::Rect>& update_rect,
               BufferPresentedCallback feedback,
               OutputSurfaceFrame frame) override;
  SkSurface* BeginPaint(
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndPaint() override;

 private:
  class MultiSurfaceSwapBuffersTracker;

  // Use instead of calling FinishSwapBuffers() directly.
  void DoFinishSwapBuffers(const gfx::Size& size,
                           OutputSurfaceFrame frame,
                           gfx::SwapCompletionResult result);
  // Used as callback for SwapBuffersAsync and PostSubBufferAsync to finish
  // operation
  void DoFinishSwapBuffersAsync(const gfx::Size& size,
                                OutputSurfaceFrame frame,
                                gfx::SwapCompletionResult result);

  void CreateSkSurface();

  const raw_ptr<gpu::SharedContextState> context_state_;
  scoped_refptr<gl::GLSurface> gl_surface_;
  const bool supports_async_swap_;

  uint64_t backbuffer_estimated_size_ = 0;

  sk_sp<SkSurface> sk_surface_;

  std::unique_ptr<MultiSurfaceSwapBuffersTracker>
      multisurface_swapbuffers_tracker_;

  base::WeakPtrFactory<SkiaOutputDeviceGL> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_GL_H_
