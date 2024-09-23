// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_DAWN_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_DAWN_H_

#include <memory>
#include <vector>

#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "third_party/dawn/include/dawn/native/DawnNative.h"
#include "third_party/dawn/include/dawn/webgpu.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_WIN)
#include "ui/gl/child_window_win.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "ui/gl/android/scoped_a_native_window.h"
#endif

namespace gpu {
class SharedContextState;
}  // namespace gpu

namespace viz {

class SkiaOutputDeviceDawn : public SkiaOutputDevice {
 public:
  using PassKey = base::PassKey<SkiaOutputDeviceDawn>;

  static std::unique_ptr<SkiaOutputDeviceDawn> Create(
      scoped_refptr<gpu::SharedContextState> context_state,
      gfx::SurfaceOrigin origin,
      gpu::SurfaceHandle surface_handle,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);

  SkiaOutputDeviceDawn(
      scoped_refptr<gpu::SharedContextState> context_state,
      gfx::SurfaceOrigin origin,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback,
      base::PassKey<SkiaOutputDeviceDawn>);

  SkiaOutputDeviceDawn(const SkiaOutputDeviceDawn&) = delete;
  SkiaOutputDeviceDawn& operator=(const SkiaOutputDeviceDawn&) = delete;

  ~SkiaOutputDeviceDawn() override;

#if BUILDFLAG(IS_WIN)
  gpu::SurfaceHandle GetChildSurfaceHandle() const {
    return child_window_.window();
  }
#endif

  // SkiaOutputDevice implementation:
  bool Reshape(const ReshapeParams& params) override;
  void Present(const std::optional<gfx::Rect>& update_rect,
               BufferPresentedCallback feedback,
               OutputSurfaceFrame frame) override;
  SkSurface* BeginPaint(
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndPaint() override;

 private:
  bool Initialize(gpu::SurfaceHandle surface_handle);

  scoped_refptr<gpu::SharedContextState> context_state_;
  wgpu::Surface surface_;
  wgpu::Texture texture_;
  sk_sp<SkSurface> sk_surface_;
  std::unique_ptr<gfx::VSyncProvider> vsync_provider_;

  gfx::Size size_;
  sk_sp<SkColorSpace> sk_color_space_;
  int sample_count_ = 1;

#if BUILDFLAG(IS_WIN)
  // D3D requires that we use flip model swap chains. Flip swap chains require
  // that the swap chain be connected with DWM. DWM requires that the rendering
  // windows are owned by the process that's currently doing the rendering.
  // gl::ChildWindowWin creates and owns a window which is reparented by the
  // browser to be a child of its window.
  gl::ChildWindowWin child_window_;
#endif

#if BUILDFLAG(IS_ANDROID)
  // Use ScopedANativeWindow to keep the window alive
  gl::ScopedANativeWindow android_native_window_;
#endif
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_DAWN_H_
