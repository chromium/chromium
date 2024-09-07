// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_EXTREME_LIGHTWEIGHT_DETECTOR_MALLOC_SHIMS_H_
#define COMPONENTS_GWP_ASAN_CLIENT_EXTREME_LIGHTWEIGHT_DETECTOR_MALLOC_SHIMS_H_

#include <cstddef>  // for size_t

#include "components/gwp_asan/client/export.h"
#include "partition_alloc/lightweight_quarantine.h"

namespace gwp_asan::internal {

struct GWP_ASAN_EXPORT ExtremeLightweightDetectorOptions {
  size_t sampling_frequency;
  size_t quarantine_capacity_for_small_objects_in_bytes;
  size_t quarantine_capacity_for_large_objects_in_bytes;
  size_t object_size_threshold_in_bytes;
};

GWP_ASAN_EXPORT void InstallExtremeLightweightDetectorHooks(
    const ExtremeLightweightDetectorOptions& options);

// Elud = Extreme Lightweight UAF Detector
GWP_ASAN_EXPORT partition_alloc::internal::LightweightQuarantineBranch&
GetEludQuarantineBranchForSmallObjectsForTesting();
GWP_ASAN_EXPORT partition_alloc::internal::LightweightQuarantineBranch&
GetEludQuarantineBranchForLargeObjectsForTesting();

}  // namespace gwp_asan::internal

#endif  // COMPONENTS_GWP_ASAN_CLIENT_EXTREME_LIGHTWEIGHT_DETECTOR_MALLOC_SHIMS_H_
