// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_FUZZER_SOFTWARE_OUTPUT_SURFACE_PROVIDER_H_
#define COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_FUZZER_SOFTWARE_OUTPUT_SURFACE_PROVIDER_H_

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "components/viz/service/display_embedder/output_surface_provider.h"

namespace viz {

// OutputSurfaceProvider implementation that provides SoftwareOutputSurface
// (no-op by default, with an option to dump pixmap to a PNG for debugging).
class FuzzerSoftwareOutputSurfaceProvider : public OutputSurfaceProvider {
 public:
  explicit FuzzerSoftwareOutputSurfaceProvider(
      std::optional<base::FilePath> png_dir_path);

  FuzzerSoftwareOutputSurfaceProvider(
      const FuzzerSoftwareOutputSurfaceProvider&) = delete;
  FuzzerSoftwareOutputSurfaceProvider& operator=(
      const FuzzerSoftwareOutputSurfaceProvider&) = delete;

  ~FuzzerSoftwareOutputSurfaceProvider() override;

  // OutputSurfaceProvider implementation.
  std::unique_ptr<DisplayCompositorMemoryAndTaskController> CreateGpuDependency(
      bool gpu_compositing,
      gpu::SurfaceHandle surface_handle) override;
  std::unique_ptr<OutputSurface> CreateOutputSurface(
      gpu::SurfaceHandle surface_handle,
      bool gpu_compositing,
      mojom::DisplayClient* display_client,
      DisplayCompositorMemoryAndTaskController* gpu_dependency,
      const RendererSettings& renderer_settings,
      const DebugRendererSettings* debug_settings) override;
  gpu::SharedImageManager* GetSharedImageManager() override;
  gpu::SyncPointManager* GetSyncPointManager() override;
  gpu::Scheduler* GetGpuScheduler() override;

 private:
  std::optional<base::FilePath> png_dir_path_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_FUZZER_SOFTWARE_OUTPUT_SURFACE_PROVIDER_H_
