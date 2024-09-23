// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_HELPERS_H_
#define COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_HELPERS_H_

#include <stddef.h>

#include <optional>
#include <string_view>

#include "components/gwp_asan/client/export.h"
#include "components/gwp_asan/client/guarded_page_allocator.h"

namespace gwp_asan::internal {

GWP_ASAN_EXPORT GuardedPageAllocator::OutOfMemoryCallback CreateOomCallback(
    std::string_view allocator_name,
    std::string_view process_type,
    size_t sampling_frequency);

// Emits to `Security.GwpAsan.Activated...`.
GWP_ASAN_EXPORT void ReportGwpAsanActivated(std::string_view allocator_name,
                                            std::string_view process_type,
                                            bool activated);

// Returns the (GWP-ASan-internal) string representation of
// `process_type`, fed from the command-line of this process.
GWP_ASAN_EXPORT std::optional<std::string_view> ProcessString(
    std::string_view process_type);

}  // namespace gwp_asan::internal

#endif  // COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_HELPERS_H_
