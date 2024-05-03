// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_SHARED_IMAGE_INTERFACE_PROVIDER_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_SHARED_IMAGE_INTERFACE_PROVIDER_H_

#include <memory>

#include "base/sequence_checker.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/service/shared_context_state.h"

namespace base {
class WaitableEvent;
}  // namespace base

namespace gpu {
class SharedImageInterface;
class SharedImageInterfaceInProcess;
class SchedulerSequence;
}  // namespace gpu

namespace viz {
class GpuServiceImpl;

class VIZ_SERVICE_EXPORT SharedImageInterfaceProvider
    : public gpu::SharedContextState::ContextLostObserver {
 public:
  explicit SharedImageInterfaceProvider(GpuServiceImpl* gpu_service);
  ~SharedImageInterfaceProvider() override;

  gpu::SharedImageInterface* GetSharedImageInterface();

 private:
  // Sequence checker for tasks that run on the gpu "thread".
  SEQUENCE_CHECKER(gpu_sequence_checker_);

  bool NeedsNewSharedImageInterface() const;

  void CreateSharedImageInterface();
  void CreateSharedImageInterfaceOnGpu(base::WaitableEvent* event);
  void OnContextLost() override;

  const raw_ptr<GpuServiceImpl> gpu_service_;
  scoped_refptr<gpu::SharedContextState> shared_context_state_;
  std::unique_ptr<gpu::SchedulerSequence> scheduler_sequence_;
  scoped_refptr<gpu::SharedImageInterfaceInProcess> shared_image_interface_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_SHARED_IMAGE_INTERFACE_PROVIDER_H_
