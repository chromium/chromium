// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_output_surface_provider.h"

#include "components/viz/service/display/display_compositor_memory_and_task_controller.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/test/fake_output_surface.h"

namespace viz {

TestOutputSurfaceProvider::TestOutputSurfaceProvider() = default;

TestOutputSurfaceProvider::~TestOutputSurfaceProvider() = default;

std::unique_ptr<DisplayCompositorMemoryAndTaskController>
TestOutputSurfaceProvider::CreateGpuDependency(
    bool gpu_compositing,
    gpu::SurfaceHandle surface_handle,
    const RendererSettings& renderer_settings) {
  // The output surface doesn't have a real gpu thread, and there is no overlay
  // support.
  return nullptr;
}

std::unique_ptr<OutputSurface> TestOutputSurfaceProvider::CreateOutputSurface(
    gpu::SurfaceHandle surface_handle,
    bool gpu_compositing,
    mojom::DisplayClient* display_client,
    DisplayCompositorMemoryAndTaskController* display_controller,
    const RendererSettings& renderer_settings,
    const DebugRendererSettings* debug_settings) {
  if (gpu_compositing) {
    return FakeOutputSurface::Create3d();
  } else {
    return FakeOutputSurface::CreateSoftware(
        std::make_unique<SoftwareOutputDevice>());
  }
}
}  // namespace viz
