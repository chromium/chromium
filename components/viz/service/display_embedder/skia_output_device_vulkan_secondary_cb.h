// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_VULKAN_SECONDARY_CB_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_VULKAN_SECONDARY_CB_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/viz/service/display_embedder/skia_output_device.h"

namespace viz {

class VulkanContextProvider;

class SkiaOutputDeviceVulkanSecondaryCB final : public SkiaOutputDevice {
 public:
  SkiaOutputDeviceVulkanSecondaryCB(
      VulkanContextProvider* context_provider,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);

  std::unique_ptr<SkiaOutputDevice::ScopedPaint> BeginScopedPaint() override;
  void Submit(bool sync_cpu, base::OnceClosure callback) override;
  bool Reshape(const ReshapeParams& params) override;
  void Present(const std::optional<gfx::Rect>& update_rect,
               BufferPresentedCallback feedback,
               OutputSurfaceFrame frame) override;
  SkSurface* BeginPaint(
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndPaint() override;

  SkCanvas* GetCanvas(SkSurface* sk_surface) override;
  GrSemaphoresSubmitted Flush(SkSurface* sk_surface,
                              VulkanContextProvider* vulkan_context_provider,
                              std::vector<GrBackendSemaphore> end_semaphores,
                              base::OnceClosure on_finished) override;
  bool Wait(SkSurface* sk_surface,
            int num_semaphores,
            const GrBackendSemaphore wait_semaphores[],
            bool delete_semaphores_after_wait) override;
  bool Draw(SkSurface* sk_surface,
            sk_sp<const GrDeferredDisplayList> ddl) override;

 private:
  const raw_ptr<VulkanContextProvider> context_provider_;
  gfx::Size size_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_VULKAN_SECONDARY_CB_H_
