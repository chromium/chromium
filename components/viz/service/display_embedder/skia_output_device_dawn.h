// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_DAWN_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_DAWN_H_

#include <memory>
#include <vector>

#include "build/build_config.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "third_party/dawn/include/dawn/dawn_wsi.h"
#include "third_party/dawn/include/dawn/native/DawnNative.h"
#include "third_party/dawn/include/dawn/webgpu.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/child_window_win.h"

namespace viz {

class DawnContextProvider;

class SkiaOutputDeviceDawn : public SkiaOutputDevice {
 public:
  SkiaOutputDeviceDawn(
      DawnContextProvider* context_provider,
      gfx::SurfaceOrigin origin,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);

  SkiaOutputDeviceDawn(const SkiaOutputDeviceDawn&) = delete;
  SkiaOutputDeviceDawn& operator=(const SkiaOutputDeviceDawn&) = delete;

  ~SkiaOutputDeviceDawn() override;

  gpu::SurfaceHandle GetChildSurfaceHandle() const;

  // SkiaOutputDevice implementation:
  bool Reshape(const SkSurfaceCharacterization& characterization,
               const gfx::ColorSpace& color_space,
               float device_scale_factor,
               gfx::OverlayTransform transform) override;
  void Present(const absl::optional<gfx::Rect>& update_rect,
               BufferPresentedCallback feedback,
               OutputSurfaceFrame frame) override;
  SkSurface* BeginPaint(
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndPaint() override;

 private:
  // Create a platform-specific swapchain implementation.
  void CreateSwapChainImplementation();

  DawnContextProvider* const context_provider_;
  DawnSwapChainImplementation swap_chain_implementation_;
  wgpu::SwapChain swap_chain_;
  wgpu::Texture texture_;
  sk_sp<SkSurface> sk_surface_;
  std::unique_ptr<gfx::VSyncProvider> vsync_provider_;

  gfx::Size size_;
  sk_sp<SkColorSpace> sk_color_space_;
  int sample_count_ = 1;

  // D3D12 requires that we use flip model swap chains. Flip swap chains
  // require that the swap chain be connected with DWM. DWM requires that
  // the rendering windows are owned by the process that's currently doing
  // the rendering. gl::ChildWindowWin creates and owns a window which is
  // reparented by the browser to be a child of its window.
  gl::ChildWindowWin child_window_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_DAWN_H_
