// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_PARTITIONALLOC_SHIMS_H_
#define COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_PARTITIONALLOC_SHIMS_H_

#include <stddef.h>  // for size_t

#include "components/gwp_asan/client/export.h"
#include "components/gwp_asan/client/guarded_page_allocator.h"
#include "components/gwp_asan/common/lightweight_detector_state.h"

namespace gwp_asan {
namespace internal {

GWP_ASAN_EXPORT void InstallPartitionAllocHooks(
    size_t max_allocated_pages,
    size_t num_metadata,
    size_t total_pages,
    size_t sampling_frequency,
    GuardedPageAllocator::OutOfMemoryCallback callback);

}  // namespace internal
}  // namespace gwp_asan

#endif  // COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_PARTITIONALLOC_SHIMS_H_
