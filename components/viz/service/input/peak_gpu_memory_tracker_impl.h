// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_INPUT_PEAK_GPU_MEMORY_TRACKER_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_INPUT_PEAK_GPU_MEMORY_TRACKER_IMPL_H_

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/resources/peak_gpu_memory_tracker.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class GpuServiceImpl;

// Tracks the peak memory of the GPU service for its lifetime. Upon its
// destruction a report will be requested from the GPU service. The peak will be
// reported to UMA Histograms.
//
// If the GPU is lost during this objects lifetime, upon destruction there will
// be no report to UMA Histograms. Emitting UMA Histograms is done by running
// PeakGpuMemoryCallback on GpuMain thread.
class VIZ_SERVICE_EXPORT PeakGpuMemoryTrackerImpl
    : public PeakGpuMemoryTracker {
 public:
  // Requests the GPU service to begin peak memory tracking.
  PeakGpuMemoryTrackerImpl(PeakGpuMemoryTracker::Usage usage,
                           GpuServiceImpl* gpu_service);
  // Requests the GPU service provides the peak memory, the result is presented
  // to UMA Histograms.
  ~PeakGpuMemoryTrackerImpl() override;

  PeakGpuMemoryTrackerImpl(const PeakGpuMemoryTrackerImpl*) = delete;
  PeakGpuMemoryTrackerImpl& operator=(const PeakGpuMemoryTrackerImpl&) = delete;

  void Cancel() override;

 private:
  bool canceled_ = false;
  PeakGpuMemoryTracker::Usage usage_;
  raw_ptr<GpuServiceImpl> gpu_service_;
  uint32_t sequence_num_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_INPUT_PEAK_GPU_MEMORY_TRACKER_IMPL_H_
