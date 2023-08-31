// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_COMMON_CRASH_KEY_NAME_H_
#define COMPONENTS_GWP_ASAN_COMMON_CRASH_KEY_NAME_H_

namespace gwp_asan {

// The name of the crash key used to convey the address of the AllocatorState
// for the malloc/PartitionAlloc hooks to the crash handler.
const char kMallocCrashKey[] = "gwp-asan-malloc";
const char kPartitionAllocCrashKey[] = "gwp-asan-partitionalloc";
const char kLightweightDetectorCrashKey[] = "lightweight-uaf-detector";

}  // namespace gwp_asan

#endif  // COMPONENTS_GWP_ASAN_COMMON_CRASH_KEY_NAME_H_
