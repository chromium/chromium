// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_SHARED_IMAGE_INTERFACE_PROVIDER_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_SHARED_IMAGE_INTERFACE_PROVIDER_H_

#include <memory>

#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
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

  // Virtual for testing.
  virtual gpu::SharedImageInterface* GetSharedImageInterface();

 private:
  // Sequence checker for tasks that run on the gpu "thread".
  SEQUENCE_CHECKER(gpu_sequence_checker_);

  bool NeedsNewSharedImageInterface();

  void CreateSharedImageInterface();
  void CreateSharedImageInterfaceOnGpu(base::WaitableEvent* event);
  void OnContextLost() override;

  // These are accessed by both threads, but compositor threads blocks when GPU
  // work thread is happening.
  const raw_ptr<GpuServiceImpl> gpu_service_;
  std::unique_ptr<gpu::SchedulerSequence> scheduler_sequence_;
  scoped_refptr<gpu::SharedImageInterfaceInProcess> shared_image_interface_;

  // These are accessed by both threads and are synchronized by the lock.
  base::Lock context_lock_;
  // Shared context state is nullptr in software, and is never lost.
  bool context_lost_ GUARDED_BY(context_lock_) = false;
  scoped_refptr<gpu::SharedContextState> shared_context_state_
      GUARDED_BY(context_lock_);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_SHARED_IMAGE_INTERFACE_PROVIDER_H_
