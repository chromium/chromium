// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_GWP_ASAN_FEATURES_H_
#define COMPONENTS_GWP_ASAN_CLIENT_GWP_ASAN_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/gwp_asan/client/export.h"

namespace gwp_asan::internal {

GWP_ASAN_EXPORT BASE_DECLARE_FEATURE(kGwpAsanMalloc);
GWP_ASAN_EXPORT BASE_DECLARE_FEATURE(kGwpAsanPartitionAlloc);

// GWP-ASan allows for per-process parameters.
// If no per-process parameter is found, GWP-ASan falls back on
// the global defaults codified in `gwp_asan.cc`.

// Browser reservation params.
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocBrowserMaxAllocations;
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocBrowserMaxMetadata;
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocBrowserTotalPages;

// Browser sampling params.
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocBrowserAllocationSamplingMultiplier;
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocBrowserAllocationSamplingRange;

// Renderer sampling params.
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocRendererAllocationSamplingMultiplier;
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocRendererAllocationSamplingRange;

// Renderer sampling params (for PartitionAlloc, not PA-E).
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanPartitionAllocRendererAllocationSamplingMultiplier;
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanPartitionAllocRendererAllocationSamplingRange;

// GPU reservation params.
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocGpuMaxAllocations;
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocGpuMaxMetadata;
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocGpuTotalPages;

// GPU sampling params.
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocGpuAllocationSamplingMultiplier;
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocGpuAllocationSamplingRange;

GWP_ASAN_EXPORT BASE_DECLARE_FEATURE(kExtremeLightweightUAFDetector);
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kExtremeLightweightUAFDetectorSamplingFrequency;
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kExtremeLightweightUAFDetectorQuarantineCapacityForSmallObjectsInBytes;
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kExtremeLightweightUAFDetectorQuarantineCapacityForLargeObjectsInBytes;
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kExtremeLightweightUAFDetectorObjectSizeThresholdInBytes;
enum class ExtremeLightweightUAFDetectorTargetProcesses {
  kAllProcesses,
  kBrowserProcessOnly,
  kNonRendererProcesses,
};
GWP_ASAN_EXPORT extern const base::FeatureParam<
    ExtremeLightweightUAFDetectorTargetProcesses>
    kExtremeLightweightUAFDetectorTargetProcesses;

}  // namespace gwp_asan::internal

#endif  // COMPONENTS_GWP_ASAN_CLIENT_GWP_ASAN_FEATURES_H_
