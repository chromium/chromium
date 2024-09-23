// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_COMMON_ALLOCATION_INFO_H_
#define COMPONENTS_GWP_ASAN_COMMON_ALLOCATION_INFO_H_

#include "base/containers/span.h"
#include "base/threading/platform_thread.h"

namespace gwp_asan::internal {

// Information saved for allocations and deallocations.
struct AllocationInfo {
  static size_t GetStackTrace(base::span<const void*> trace);
  static uint64_t GetCurrentTid();

  // (De)allocation thread id or base::kInvalidThreadId if no (de)allocation
  // occurred.
  uint64_t tid = base::kInvalidThreadId;
  // Length used to encode the packed stack trace.
  uint16_t trace_len = 0;
  // Whether a stack trace has been collected for this (de)allocation.
  bool trace_collected = false;
};

}  // namespace gwp_asan::internal

#endif  // COMPONENTS_GWP_ASAN_COMMON_ALLOCATION_INFO_H_
