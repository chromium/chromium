// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_MALLOC_SHIMS_H_
#define COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_MALLOC_SHIMS_H_

#include <stddef.h>  // for size_t

#include "build/build_config.h"
#include "components/gwp_asan/client/export.h"
#include "components/gwp_asan/client/guarded_page_allocator.h"
#include "components/gwp_asan/client/gwp_asan.h"

namespace gwp_asan {
namespace internal {

// Initialize the guarded allocator with the given parameters, and install the
// sampling malloc shims with the provided sampling frequency.
GWP_ASAN_EXPORT void InstallMallocHooks(
    const AllocatorSettings& settings,
    GuardedPageAllocator::OutOfMemoryCallback callback);

}  // namespace internal

// Checks if the |ptr| is a GWP-ASan allocation. (This is exposed for use by
// Zombies on macOS.)
GWP_ASAN_EXPORT bool IsGwpAsanMallocAllocation(const void* ptr);

}  // namespace gwp_asan

#endif  // COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_MALLOC_SHIMS_H_
