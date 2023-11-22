// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_MALLOC_SHIMS_H_
#define COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_MALLOC_SHIMS_H_

#include <stddef.h>

#include "components/gwp_asan/client/export.h"
#include "components/gwp_asan/client/gwp_asan.h"

namespace gwp_asan::internal::lud {

template <typename T, typename U>
class RandomEvictionQuarantineImpl;

GWP_ASAN_EXPORT void InstallMallocHooks(size_t max_allocation_count,
                                        size_t max_total_size,
                                        size_t total_size_high_water_mark,
                                        size_t total_size_low_water_mark,
                                        size_t eviction_chunk_size,
                                        size_t eviction_task_interval_ms,
                                        size_t sampling_frequency);

class GWP_ASAN_EXPORT MallocShimSupport {
 private:
  // `RandomEvictionQuarantine` needs to defer calling the next hook
  // in the allocator shim chain. Not for general use.
  static void NextFree(void* ptr);
  static size_t NextGetSizeEstimate(void* ptr);

  template <typename T, typename U>
  friend class RandomEvictionQuarantineImpl;
};

}  // namespace gwp_asan::internal::lud

#endif  // COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_MALLOC_SHIMS_H_
