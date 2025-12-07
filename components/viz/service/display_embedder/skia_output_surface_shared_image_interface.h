// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_SHARED_IMAGE_INTERFACE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_SHARED_IMAGE_INTERFACE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/service/shared_image_interface_in_process_base.h"

namespace gpu {
class SharedImageFactory;
struct SyncToken;
}  // namespace gpu

namespace viz {
class SkiaOutputSurfaceImpl;
class SkiaOutputSurfaceImplOnGpu;

// This wraps `SkiaOutputSurfaceImpl` to implement `SharedImageInterface`.
class VIZ_SERVICE_EXPORT SkiaOutputSurfaceSharedImageInterface
    : public gpu::SharedImageInterfaceInProcessBase {
 public:
  explicit SkiaOutputSurfaceSharedImageInterface(
      SkiaOutputSurfaceImpl& output_surface,
      SkiaOutputSurfaceImplOnGpu& output_surface_on_gpu);

  // Call from destructor of `output_surface_` to break reference cycle.
  // This will also break the reference to `output_surface_on_gpu_`. This can
  // only be called from the host thread.
  void DetachOutputSurfaceOnHostThread();

 protected:
  ~SkiaOutputSurfaceSharedImageInterface() override;

  // SharedImageInterfaceInProcessBase:
  void ScheduleGpuTask(base::OnceClosure task,
                       std::vector<gpu::SyncToken> sync_token_fences,
                       const gpu::SyncToken& release) override;
  gpu::SharedImageFactory* GetSharedImageFactoryOnGpuThread() override;
  bool MakeContextCurrentOnGpuThread(bool needs_gl) override;
  void MarkContextLostOnGpuThread() override;

 private:
  // This breaks the reference to `output_surface_on_gpu_`.
  void DetachOutputSurfaceOnGpuThread();

  // This schedules a GPU task from the host thread.
  void ScheduleGpuTaskOnHostThread(
      base::OnceClosure task,
      std::vector<gpu::SyncToken> sync_token_fences);

  raw_ptr<SkiaOutputSurfaceImpl> output_surface_;
  raw_ptr<SkiaOutputSurfaceImplOnGpu> output_surface_on_gpu_;
  scoped_refptr<base::SequencedTaskRunner> host_task_runner_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_SHARED_IMAGE_INTERFACE_H_
