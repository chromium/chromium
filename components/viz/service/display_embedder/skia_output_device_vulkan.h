// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_VULKAN_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_VULKAN_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/vulkan/vulkan_swap_chain.h"

namespace gpu {
class VulkanSurface;
}

namespace viz {

class VulkanContextProvider;

class SkiaOutputDeviceVulkan final : public SkiaOutputDevice {
 public:
  SkiaOutputDeviceVulkan(
      VulkanContextProvider* context_provider,
      gpu::SurfaceHandle surface_handle,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);
  ~SkiaOutputDeviceVulkan() override;

  // SkiaOutputDevice implementation:
  bool Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               gfx::OverlayTransform transform) override;
  void SwapBuffers(BufferPresentedCallback feedback,
                   std::vector<ui::LatencyInfo> latency_info) override;
  SkSurface* BeginPaint() override;
  void EndPaint(const GrBackendSemaphore& semaphore) override;

 private:
  void CreateVulkanSurface();
  void CreateSkSurface();

  VulkanContextProvider* const context_provider_;

  const gpu::SurfaceHandle surface_handle_;
  std::unique_ptr<gpu::VulkanSurface> vulkan_surface_;

  base::Optional<gpu::VulkanSwapChain::ScopedWrite> scoped_write_;

  // SkSurfaces for swap chain images.
  std::vector<sk_sp<SkSurface>> sk_surfaces_;

  sk_sp<SkColorSpace> sk_color_space_;

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputDeviceVulkan);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_VULKAN_H_
