// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_OUTPUT_SURFACE_PROVIDER_H_
#define COMPONENTS_VIZ_TEST_TEST_OUTPUT_SURFACE_PROVIDER_H_

#include <memory>

#include "components/viz/service/display_embedder/output_surface_provider.h"

namespace viz {

// Test implementation that creates a fake OutputSurface.
class TestOutputSurfaceProvider : public OutputSurfaceProvider {
 public:
  TestOutputSurfaceProvider();

  TestOutputSurfaceProvider(const TestOutputSurfaceProvider&) = delete;
  TestOutputSurfaceProvider& operator=(const TestOutputSurfaceProvider&) =
      delete;

  ~TestOutputSurfaceProvider() override;

  // OutputSurfaceProvider implementation.
  std::unique_ptr<DisplayCompositorMemoryAndTaskController> CreateGpuDependency(
      bool gpu_compositing,
      gpu::SurfaceHandle surface_handle) override;
  std::unique_ptr<OutputSurface> CreateOutputSurface(
      gpu::SurfaceHandle surface_handle,
      bool gpu_compositing,
      mojom::DisplayClient* display_client,
      DisplayCompositorMemoryAndTaskController* display_controller,
      const RendererSettings& renderer_settings,
      const DebugRendererSettings* debug_settings) override;
  gpu::SharedImageManager* GetSharedImageManager() override;
  gpu::SyncPointManager* GetSyncPointManager() override;
  gpu::Scheduler* GetGpuScheduler() override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_OUTPUT_SURFACE_PROVIDER_H_
