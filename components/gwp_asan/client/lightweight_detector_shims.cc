// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/lightweight_detector_shims.h"

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "components/crash/core/common/crash_key.h"
#include "components/gwp_asan/client/lightweight_detector.h"
#include "components/gwp_asan/common/crash_key_name.h"

namespace gwp_asan::internal {

namespace {

// The global detector singleton used by the shims. Implemented as a global
// pointer instead of a function-local static to avoid initialization checks
// for every access.
LightweightDetector* detector = nullptr;

void QuarantineHook(void* address, size_t size) {
  detector->RecordLightweightDeallocation(address, size);
}

}  // namespace

GWP_ASAN_EXPORT LightweightDetector& GetLightweightDetectorForTesting() {
  return *detector;
}

GWP_ASAN_EXPORT void InstallLightweightDetectorHooks(
    LightweightDetectorMode mode,
    size_t num_metadata) {
  detector = new LightweightDetector(mode, num_metadata);

  static crash_reporter::CrashKeyString<24> crash_key(
      kLightweightDetectorCrashKey);
  crash_key.Set(detector->GetCrashKey());

  if (mode == LightweightDetectorMode::kBrpQuarantine) {
    partition_alloc::PartitionAllocHooks::SetQuarantineOverrideHook(
        &QuarantineHook);
  }
}

}  // namespace gwp_asan::internal
