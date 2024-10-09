// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/extreme_lightweight_detector_malloc_shims.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace gwp_asan::internal {

namespace {

constexpr size_t kSamplingFrequency = 10;
constexpr size_t kQuarantineCapacityForSmallObjectsInBytes = 4096;
constexpr size_t kQuarantineCapacityForLargeObjectsInBytes = 4096;
constexpr size_t kObjectSizeThresholdInBytes = 256;

// Number of loop iterations required to definitely hit a sampled allocation.
constexpr size_t kLoopIterations = kSamplingFrequency * 10;

constexpr int kSuccess = 0;
constexpr int kFailure = 1;

class ExtremeLightweightDetectorMallocShimsTest
    : public base::MultiProcessTest {
 public:
  static void MultiprocessTestSetup() {
    allocator_shim::ConfigurePartitions(
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
        allocator_shim::EnableBrp(true),
#else
        allocator_shim::EnableBrp(false),
#endif
        allocator_shim::EnableMemoryTagging(false),
        partition_alloc::TagViolationReportingMode::kDisabled,
        allocator_shim::BucketDistribution::kNeutral,
        allocator_shim::SchedulerLoopQuarantine(false),
        /*scheduler_loop_quarantine_capacity_in_bytes=*/0,
        allocator_shim::ZappingByFreeFlags(false),
        allocator_shim::UsePoolOffsetFreelists(true),
        allocator_shim::UseSmallSingleSlotSpans(true));
    InstallExtremeLightweightDetectorHooks(
        {.sampling_frequency = kSamplingFrequency,
         .quarantine_capacity_for_small_objects_in_bytes =
             kQuarantineCapacityForSmallObjectsInBytes,
         .quarantine_capacity_for_large_objects_in_bytes =
             kQuarantineCapacityForLargeObjectsInBytes,
         .object_size_threshold_in_bytes = kObjectSizeThresholdInBytes});
  }

 protected:
  void RunTest(const char* name) {
    base::Process process = SpawnChild(name);
    int exit_code = -1;
    ASSERT_TRUE(WaitForMultiprocessTestChildExit(
        process, TestTimeouts::action_max_timeout(), &exit_code));
    EXPECT_EQ(exit_code, kSuccess);
  }
};

MULTIPROCESS_TEST_MAIN_WITH_SETUP(
    Basic,
    ExtremeLightweightDetectorMallocShimsTest::MultiprocessTestSetup) {
  auto& small_quarantine = GetEludQuarantineBranchForSmallObjectsForTesting();
  auto& large_quarantine = GetEludQuarantineBranchForLargeObjectsForTesting();

  struct SmallObject {
    using TypeTag = SmallObject*;
    char c[42];
  };
  CHECK_LT(sizeof(SmallObject), kObjectSizeThresholdInBytes);

  struct LargeObject {
    using TypeTag = LargeObject*;
    char c[1234];
  };
  CHECK_GT(sizeof(LargeObject), kObjectSizeThresholdInBytes);

  auto try_to_quarantine =
      []<typename ObjectType>(
          partition_alloc::internal::LightweightQuarantineBranch&
              quarantine_branch,
          ObjectType* unused_type_tag) -> ObjectType* {
    for (size_t i = 0; i < kLoopIterations; ++i) {
      // macOS defers the actual deallocation when `free` is called (i.e. `free`
      // returns immediately without actually deallocating the memory pointed to
      // by the given pointer). It's not easy to predict when `Quarantine` is
      // called. However, `operator delete` doesn't defer the deallocation. It
      // calls `malloc_zone_free` in sync (As of Jan 2024). So, new/delete is
      // used instead of malloc/free here.
      ObjectType* ptr = new ObjectType();
      delete ptr;
      if (quarantine_branch.IsQuarantinedForTesting(ptr)) {
        return ptr;
      }
    }
    return nullptr;
  };

  SmallObject* small_object =
      try_to_quarantine(small_quarantine, SmallObject::TypeTag());
  EXPECT_TRUE(small_object);
  EXPECT_TRUE(small_quarantine.IsQuarantinedForTesting(small_object));
  EXPECT_FALSE(large_quarantine.IsQuarantinedForTesting(small_object));
  small_quarantine.Purge();

  LargeObject* large_object =
      try_to_quarantine(large_quarantine, LargeObject::TypeTag());
  EXPECT_TRUE(large_object);
  EXPECT_FALSE(small_quarantine.IsQuarantinedForTesting(large_object));
  EXPECT_TRUE(large_quarantine.IsQuarantinedForTesting(large_object));
  large_quarantine.Purge();

  return ::testing::Test::HasFailure() ? kFailure : kSuccess;
}

TEST_F(ExtremeLightweightDetectorMallocShimsTest, Basic) {
  RunTest("Basic");
}

}  // namespace

}  // namespace gwp_asan::internal

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
