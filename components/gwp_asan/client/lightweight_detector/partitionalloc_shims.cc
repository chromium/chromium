// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/lightweight_detector/partitionalloc_shims.h"

#include "components/gwp_asan/client/lightweight_detector/poison_metadata_recorder.h"
#include "partition_alloc/partition_alloc.h"

namespace gwp_asan::internal::lud {

namespace {
void QuarantineHook(void* address, size_t size) {
  PoisonMetadataRecorder::Get()->RecordAndZap(address, size);
}
}  // namespace

void InstallPartitionAllocHooks() {
  partition_alloc::PartitionAllocHooks::SetQuarantineOverrideHook(
      &QuarantineHook);
}

}  // namespace gwp_asan::internal::lud
