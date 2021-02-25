// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_VULKAN_SECONDARY_CB_OFFSCREEN_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_VULKAN_SECONDARY_CB_OFFSCREEN_H_

#include <vector>

#include "components/viz/service/display_embedder/skia_output_device_offscreen.h"
#include "gpu/command_buffer/service/shared_context_state.h"

namespace viz {

// Draw into an offscreen buffer which is then drawn to into the secondary
// command buffer. This is meant to for debugging direct compositing with
// secondary command buffers.
class SkiaOutputDeviceVulkanSecondaryCBOffscreen final
    : public SkiaOutputDeviceOffscreen {
 public:
  SkiaOutputDeviceVulkanSecondaryCBOffscreen(
      scoped_refptr<gpu::SharedContextState> context_state,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);
  ~SkiaOutputDeviceVulkanSecondaryCBOffscreen() override;

  SkSurface* BeginPaint(
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void SwapBuffers(BufferPresentedCallback feedback,
                   OutputSurfaceFrame frame) override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_VULKAN_SECONDARY_CB_OFFSCREEN_H_
