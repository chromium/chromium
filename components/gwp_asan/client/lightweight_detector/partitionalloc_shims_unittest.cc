// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/lightweight_detector/partitionalloc_shims.h"

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "components/crash/core/common/crash_key.h"
#include "components/gwp_asan/client/lightweight_detector/poison_metadata_recorder.h"
#include "components/gwp_asan/common/lightweight_detector_state.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

// PartitionAlloc (and hence hooking) are disabled with sanitizers that replace
// allocation routines.
#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR) &&    \
    PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) && \
    PA_BUILDFLAG(USE_RAW_PTR_BACKUP_REF_IMPL)

// These tests install global PartitionAlloc hooks so they are not safe to run
// in multi-threaded contexts. Instead they're implemented as multi-process
// tests.

namespace gwp_asan::internal::lud {

namespace {

constexpr int kSuccess = 0;
constexpr int kFailure = 1;

constexpr partition_alloc::PartitionOptions kAllocatorOptions = []() {
  partition_alloc::PartitionOptions opts;
  opts.backup_ref_ptr = partition_alloc::PartitionOptions::kEnabled;
  return opts;
}();

static void HandleOOM(size_t) {
  LOG(FATAL) << "Out of memory.";
}

}  // namespace

class PartitionAllocShimsTest : public base::MultiProcessTest {
 protected:
  void runTest(const char* name) {
    base::Process process = SpawnChild(name);
    int exit_code = -1;
    ASSERT_TRUE(WaitForMultiprocessTestChildExit(
        process, TestTimeouts::action_max_timeout(), &exit_code));
    EXPECT_EQ(exit_code, kSuccess);
  }
};

MULTIPROCESS_TEST_MAIN(PartitionAllocShimsTest_Basic) {
  crash_reporter::InitializeCrashKeys();
  partition_alloc::PartitionAllocGlobalInit(HandleOOM);
  PoisonMetadataRecorder::Init(LightweightDetectorMode::kBrpQuarantine,
                               LightweightDetectorState::kMaxMetadata);
  InstallPartitionAllocHooks();

  partition_alloc::PartitionAllocator allocator;
  allocator.init(kAllocatorOptions);

  raw_ptr<void, base::RawPtrTraits::kMayDangle> ptr =
      allocator.root()->Alloc(1);
  allocator.root()->Free(ptr);

  if (PoisonMetadataRecorder::Get()->HasAllocationForTesting(
          reinterpret_cast<uintptr_t>(ptr.get()))) {
    return kSuccess;
  }

  return kFailure;
}

TEST_F(PartitionAllocShimsTest, Basic) {
  runTest("PartitionAllocShimsTest_Basic");
}

}  // namespace gwp_asan::internal::lud

#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR) &&
        // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) &&
        // PA_BUILDFLAG(USE_RAW_PTR_BACKUP_REF_IMPL)
