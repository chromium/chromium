// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_GWP_ASAN_H_
#define COMPONENTS_GWP_ASAN_CLIENT_GWP_ASAN_H_

#include <stddef.h>  // for size_t

#include <string_view>

#include "components/gwp_asan/client/export.h"
#include "components/gwp_asan/common/lightweight_detector_state.h"

namespace gwp_asan {

namespace internal {

struct AllocatorSettings {
  size_t max_allocated_pages;
  size_t num_metadata;
  size_t total_pages;
  size_t sampling_frequency;
};

}  // namespace internal

// Enable GWP-ASan for the current process for the given allocator. This should
// only be called once per process. This can not be disabled once it has been
// enabled. The |boost_sampling| parameter is used to indicate if the caller
// wants to increase the sampling for this process. The |process_type| parameter
// should be equal to the string passed to --type=, it is used for reporting
// metrics broken out per-process.

GWP_ASAN_EXPORT void EnableForMalloc(bool boost_sampling,
                                     std::string_view process_type);
GWP_ASAN_EXPORT void EnableForPartitionAlloc(bool boost_sampling,
                                             std::string_view process_type);
GWP_ASAN_EXPORT void MaybeEnableLightweightDetector(bool boost_sampling,
                                                    const char* process_type);
GWP_ASAN_EXPORT void MaybeEnableExtremeLightweightDetector(
    bool boost_sampling,
    std::string_view process_type);

}  // namespace gwp_asan

#endif  // COMPONENTS_GWP_ASAN_CLIENT_GWP_ASAN_H_
