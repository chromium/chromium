// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/input/peak_gpu_memory_tracker_impl.h"

#include <memory>
#include <utility>

#include "components/viz/common/resources/peak_gpu_memory_callback.h"
#include "components/viz/common/resources/peak_gpu_memory_tracker_util.h"
#include "components/viz/service/gl/gpu_service_impl.h"

namespace viz {

PeakGpuMemoryTrackerImpl::PeakGpuMemoryTrackerImpl(
    PeakGpuMemoryTracker::Usage usage,
    GpuServiceImpl* gpu_service)
    : usage_(usage),
      gpu_service_(gpu_service),
      sequence_num_(GetNextSequenceNumber(SequenceLocation::kGpuProcess)) {
  // Actually performs request to GPU service to begin memory tracking for
  // |sequence_number_|.
  if (gpu_service_) {
    gpu_service_->StartPeakMemoryMonitor(sequence_num_);
  }
}

PeakGpuMemoryTrackerImpl::~PeakGpuMemoryTrackerImpl() {
  if (canceled_) {
    return;
  }

  // PeakGpuMemoryCallback safely runs on GpuMain thread when peak GPU memory
  // usage is requested from the VizCompositor thread. |testing_callback| is not
  // provided here, since no synchronization of work is required by tests as
  // it's directly run on GpuMain thread.
  gpu_service_->GetPeakMemoryUsage(
      sequence_num_,
      base::BindOnce(&PeakGpuMemoryCallback, usage_, base::DoNothing()));
}

void PeakGpuMemoryTrackerImpl::Cancel() {
  canceled_ = true;
  if (gpu_service_) {
    gpu_service_->GetPeakMemoryUsage(sequence_num_, base::DoNothing());
  }
}

}  // namespace viz
