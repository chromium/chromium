// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "components/gwp_asan/client/lightweight_detector/partitionalloc_shims.h"
#include "components/gwp_asan/client/lightweight_detector/poison_metadata_recorder.h"
#include "components/gwp_asan/client/lightweight_detector/random_eviction_quarantine.h"
#include "components/gwp_asan/common/lightweight_detector_state.h"
#include "partition_alloc/shim/allocator_shim.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

// These tests install global malloc hooks so they are not safe to run
// in multi-threaded contexts. Instead they're implemented as multi-process
// tests.

namespace gwp_asan::internal::lud {

namespace {

constexpr int kSuccess = 0;
constexpr int kFailure = 1;

// Leave enough space to catch the test allocation, considering potential
// implicit allocations.
constexpr size_t kMaxAllocationCount = 1024;
constexpr size_t kMaxTotalSize = 65536;
constexpr size_t kTotalSizeHighWaterMark = kMaxTotalSize - 1;
constexpr size_t kTotalSizeLowWaterMark = kTotalSizeHighWaterMark - 1;
constexpr size_t kEvictionChunkSize = 1;
constexpr size_t kEvictionTaskIntervalMs = 1000;

// Sample every allocation.
constexpr size_t kSamplingFrequency = 1;

}  // namespace

class MallocShimsTest : public base::MultiProcessTest {
 protected:
  void runTest(const char* name) {
    base::Process process = SpawnChild(name);
    int exit_code = -1;
    ASSERT_TRUE(WaitForMultiprocessTestChildExit(
        process, TestTimeouts::action_max_timeout(), &exit_code));
    EXPECT_EQ(exit_code, kSuccess);
  }
};

MULTIPROCESS_TEST_MAIN(MallocShimsTest_Basic) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
  };

#if BUILDFLAG(IS_APPLE)
  allocator_shim::InitializeAllocatorShim();
#endif  // BUILDFLAG(IS_APPLE)

  crash_reporter::InitializeCrashKeys();
  PoisonMetadataRecorder::Init(LightweightDetectorMode::kRandom,
                               kMaxAllocationCount);
  InstallMallocHooks(kMaxAllocationCount, kMaxTotalSize,
                     kTotalSizeHighWaterMark, kTotalSizeLowWaterMark,
                     kEvictionChunkSize, kEvictionTaskIntervalMs,
                     kSamplingFrequency);

  char* ptr = new char;
  delete ptr;

  if (PoisonMetadataRecorder::Get()->HasAllocationForTesting(
          reinterpret_cast<uintptr_t>(ptr))) {
    return kSuccess;
  }

  return kFailure;
}

TEST_F(MallocShimsTest, Basic) {
  runTest("MallocShimsTest_Basic");
}

}  // namespace gwp_asan::internal::lud
