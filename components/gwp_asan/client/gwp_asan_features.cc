// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/gwp_asan_features.h"

#include "build/build_config.h"

namespace gwp_asan::internal {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) ||                                          \
    (BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_64_BITS))
constexpr base::FeatureState kDefaultEnabled = base::FEATURE_ENABLED_BY_DEFAULT;
#else
constexpr base::FeatureState kDefaultEnabled =
    base::FEATURE_DISABLED_BY_DEFAULT;
#endif

BASE_FEATURE(kGwpAsanMalloc, kDefaultEnabled);
BASE_FEATURE(kGwpAsanPartitionAlloc, kDefaultEnabled);

#if BUILDFLAG(IS_ANDROID)

// Browser reservation params.
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocBrowserMaxAllocations{&kGwpAsanMalloc,
                                        "BrowserMaxAllocations", 210};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocBrowserMaxMetadata{&kGwpAsanMalloc, "BrowserMaxMetadata",
                                     765};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocBrowserTotalPages{&kGwpAsanMalloc, "BrowserTotalPages", 1536};

// Browser sampling params.
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocBrowserAllocationSamplingMultiplier{
        &kGwpAsanMalloc, "BrowserAllocationSamplingMultiplier", 1500};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocBrowserAllocationSamplingRange{
        &kGwpAsanMalloc, "BrowserAllocationSamplingRange", 16};

// Renderer sampling params.
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocRendererAllocationSamplingMultiplier{
        &kGwpAsanMalloc, "RendererAllocationSamplingMultiplier", 1500};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocRendererAllocationSamplingRange{
        &kGwpAsanMalloc, "RendererAllocationSamplingRange", 12};

// Renderer sampling params (for PartitionAlloc, not PA-E).
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanPartitionAllocRendererAllocationSamplingMultiplier{
        &kGwpAsanPartitionAlloc, "RendererAllocationSamplingMultiplier", 1500};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanPartitionAllocRendererAllocationSamplingRange{
        &kGwpAsanPartitionAlloc, "RendererAllocationSamplingRange", 12};

// GPU reservation params.
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocGpuMaxAllocations{&kGwpAsanMalloc, "GpuMaxAllocations", 140};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocGpuMaxMetadata{&kGwpAsanMalloc, "GpuMaxMetadata", 510};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocGpuTotalPages{&kGwpAsanMalloc, "GpuTotalPages", 1024};

// GPU sampling params.
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocGpuAllocationSamplingMultiplier{
        &kGwpAsanMalloc, "GpuAllocationSamplingMultiplier", 1500};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocGpuAllocationSamplingRange{&kGwpAsanMalloc,
                                             "GpuAllocationSamplingRange", 16};

#elif BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX)

// Browser reservation params.
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocBrowserMaxAllocations{&kGwpAsanMalloc,
                                        "BrowserMaxAllocations", 150};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocBrowserMaxMetadata{&kGwpAsanMalloc, "BrowserMaxMetadata",
                                     630};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocBrowserTotalPages{&kGwpAsanMalloc, "BrowserTotalPages", 6144};

// Browser sampling params.
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocBrowserAllocationSamplingMultiplier{
        &kGwpAsanMalloc, "BrowserAllocationSamplingMultiplier", 1200};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocBrowserAllocationSamplingRange{
        &kGwpAsanMalloc, "BrowserAllocationSamplingRange", 10};

// Renderer sampling params.
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocRendererAllocationSamplingMultiplier{
        &kGwpAsanMalloc, "RendererAllocationSamplingMultiplier", 800};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocRendererAllocationSamplingRange{
        &kGwpAsanMalloc, "RendererAllocationSamplingRange", 10};

// Renderer sampling params (for PartitionAlloc, not PA-E).
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanPartitionAllocRendererAllocationSamplingMultiplier{
        &kGwpAsanPartitionAlloc, "RendererAllocationSamplingMultiplier", 800};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanPartitionAllocRendererAllocationSamplingRange{
        &kGwpAsanPartitionAlloc, "RendererAllocationSamplingRange", 10};

// GPU reservation params.
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocGpuMaxAllocations{&kGwpAsanMalloc, "GpuMaxAllocations", 100};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocGpuMaxMetadata{&kGwpAsanMalloc, "GpuMaxMetadata", 420};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocGpuTotalPages{&kGwpAsanMalloc, "GpuTotalPages", 4096};

// GPU sampling params.
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocGpuAllocationSamplingMultiplier{
        &kGwpAsanMalloc, "GpuAllocationSamplingMultiplier", 1200};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocGpuAllocationSamplingRange{&kGwpAsanMalloc,
                                             "GpuAllocationSamplingRange", 10};

#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

// Browser reservation params.
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocBrowserMaxAllocations{&kGwpAsanMalloc,
                                        "BrowserMaxAllocations", 210};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocBrowserMaxMetadata{&kGwpAsanMalloc, "BrowserMaxMetadata",
                                     765};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocBrowserTotalPages{&kGwpAsanMalloc, "BrowserTotalPages", 6144};

// Browser sampling params.
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocBrowserAllocationSamplingMultiplier{
        &kGwpAsanMalloc, "BrowserAllocationSamplingMultiplier", 800};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocBrowserAllocationSamplingRange{
        &kGwpAsanMalloc, "BrowserAllocationSamplingRange", 10};

// Renderer sampling params.
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocRendererAllocationSamplingMultiplier{
        &kGwpAsanMalloc, "RendererAllocationSamplingMultiplier", 600};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocRendererAllocationSamplingRange{
        &kGwpAsanMalloc, "RendererAllocationSamplingRange", 10};

// Renderer sampling params (for PartitionAlloc, not PA-E).
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanPartitionAllocRendererAllocationSamplingMultiplier{
        &kGwpAsanPartitionAlloc, "RendererAllocationSamplingMultiplier", 600};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanPartitionAllocRendererAllocationSamplingRange{
        &kGwpAsanPartitionAlloc, "RendererAllocationSamplingRange", 10};

// GPU reservation params.
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocGpuMaxAllocations{&kGwpAsanMalloc, "GpuMaxAllocations", 140};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocGpuMaxMetadata{&kGwpAsanMalloc, "GpuMaxMetadata", 510};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocGpuTotalPages{&kGwpAsanMalloc, "GpuTotalPages", 4096};

// GPU sampling params.
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocGpuAllocationSamplingMultiplier{
        &kGwpAsanMalloc, "GpuAllocationSamplingMultiplier", 800};
GWP_ASAN_EXPORT extern const base::FeatureParam<int>
    kGwpAsanMallocGpuAllocationSamplingRange{&kGwpAsanMalloc,
                                             "GpuAllocationSamplingRange", 10};

#endif
// BUILDFLAG(IS_IOS) does not need process-specific parameters as it only has
// one chrome-controlled process (the browser process).

BASE_FEATURE(kExtremeLightweightUAFDetector, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kExtremeLightweightUAFDetectorSamplingFrequency{
    &kExtremeLightweightUAFDetector, "sampling_frequency",
    1000};  // Quarantine once per 1000 calls to `free`.
const base::FeatureParam<int>
    kExtremeLightweightUAFDetectorQuarantineCapacityForSmallObjectsInBytes{
        &kExtremeLightweightUAFDetector,
        "quarantine_capacity_for_small_objects_in_bytes",
        1 * 1024 * 1024 - 100 * 1024};  // 900 KiB for small objects.
const base::FeatureParam<int>
    kExtremeLightweightUAFDetectorQuarantineCapacityForLargeObjectsInBytes{
        &kExtremeLightweightUAFDetector,
        "quarantine_capacity_for_large_objects_in_bytes",
        100 * 1024};  // 100 KiB for large objects.
// Small objects: size <= 1 KiB
// Large objects: size > 1 KiB
const base::FeatureParam<int>
    kExtremeLightweightUAFDetectorObjectSizeThresholdInBytes{
        &kExtremeLightweightUAFDetector, "object_size_threshold_in_bytes",
        1 * 1024};
constexpr base::FeatureParam<ExtremeLightweightUAFDetectorTargetProcesses>::
    Option kExtremeLightweightUAFDetectorTargetProcessesOptions[] = {
        {ExtremeLightweightUAFDetectorTargetProcesses::kAllProcesses, "all"},
        {ExtremeLightweightUAFDetectorTargetProcesses::kBrowserProcessOnly,
         "browser_only"},
        {ExtremeLightweightUAFDetectorTargetProcesses::kNonRendererProcesses,
         "non_renderer"},
};
const base::FeatureParam<ExtremeLightweightUAFDetectorTargetProcesses>
    kExtremeLightweightUAFDetectorTargetProcesses{
        &kExtremeLightweightUAFDetector,
        "target_processes",
        ExtremeLightweightUAFDetectorTargetProcesses::kAllProcesses,
        &kExtremeLightweightUAFDetectorTargetProcessesOptions,
    };

}  // namespace gwp_asan::internal
