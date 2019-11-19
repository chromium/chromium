// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_HELPERS_H_
#define COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_HELPERS_H_

#include "components/gwp_asan/client/export.h"
#include "components/gwp_asan/client/guarded_page_allocator.h"

namespace gwp_asan {
namespace internal {

// Create a callback for GuardedPageAllocator that reports allocator OOM.
GWP_ASAN_EXPORT GuardedPageAllocator::OutOfMemoryCallback CreateOomCallback(
    const char* allocator_name,
    const char* process_type,
    size_t sampling_frequency);

}  // namespace internal
}  // namespace gwp_asan

#endif  // COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_HELPERS_H_
