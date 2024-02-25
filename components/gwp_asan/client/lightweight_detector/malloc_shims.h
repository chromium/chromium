// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_MALLOC_SHIMS_H_
#define COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_MALLOC_SHIMS_H_

#include <stddef.h>

#include "base/memory/raw_ptr_exclusion.h"
#include "build/build_config.h"
#include "components/gwp_asan/client/export.h"
#include "components/gwp_asan/client/gwp_asan.h"

namespace gwp_asan::internal::lud {

// `free()`-like functions that we support. Each value should have a
// corresponding branch in `ResumeFree()`.
enum class FreeFunctionKind : uint8_t {
  kUnknown,
  kFree,
  kFreeDefiniteSize,
  kTryFreeDefault,
  kAlignedFree,
};

struct AllocationInfo {
  bool operator==(const AllocationInfo& rhs) const {
    return address == rhs.address && size == rhs.size &&
           free_fn_kind == rhs.free_fn_kind
#if BUILDFLAG(IS_APPLE)
           && context == rhs.context
#endif  // BUILDFLAG(IS_APPLE)
        ;
  }

  // RAW_PTR_EXCLUSION: By the time `address` arrives here, it has already been
  // passed to a `free()`-like function, and assigning a dangling `T*` to a
  // `raw_ptr<T>` is forbidden.
  RAW_PTR_EXCLUSION void* address = nullptr;
  uint32_t size = 0;  // Intentionally not `size_t` to save space.
  FreeFunctionKind free_fn_kind = FreeFunctionKind::kUnknown;

#if BUILDFLAG(IS_APPLE)  // Not used on other platforms.
  // RAW_PTR_EXCLUSION: On macOS and iOS, this is a pointer to an OS-internal
  // `malloc_zone_t`, which can't be protected by raw_ptr<T>.
  RAW_PTR_EXCLUSION void* context = nullptr;
#endif  // BUILDFLAG(IS_APPLE)
};

GWP_ASAN_EXPORT void InstallMallocHooks(size_t max_allocation_count,
                                        size_t max_total_size,
                                        size_t total_size_high_water_mark,
                                        size_t total_size_low_water_mark,
                                        size_t eviction_chunk_size,
                                        size_t eviction_task_interval_ms,
                                        size_t sampling_frequency);

// `RandomEvictionQuarantine` needs to defer calling the next hook
// in the allocator shim chain. Not for general use.
GWP_ASAN_EXPORT void FinishFree(const AllocationInfo&);

}  // namespace gwp_asan::internal::lud

#endif  // COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_MALLOC_SHIMS_H_
