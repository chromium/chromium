// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/lightweight_detector/random_eviction_quarantine.h"

#include <type_traits>

#include "base/auto_reset.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gwp_asan::internal::lud {

using ::testing::_;
using ::testing::Return;

namespace {

class TestRandomEvictionQuarantine final : public RandomEvictionQuarantineBase {
 public:
  TestRandomEvictionQuarantine(size_t max_allocation_count,
                               size_t max_total_size,
                               size_t total_size_high_water_mark,
                               size_t total_size_low_water_mark,
                               size_t eviction_chunk_size,
                               size_t eviction_task_interval_ms)
      : RandomEvictionQuarantineBase(max_allocation_count,
                                     max_total_size,
                                     total_size_high_water_mark,
                                     total_size_low_water_mark,
                                     eviction_chunk_size,
                                     eviction_task_interval_ms) {}
  MOCK_METHOD(void, RecordAndZap, (void* ptr, size_t size));
  MOCK_METHOD(void, FinishFree, (const AllocationInfo& info));
};

AllocationInfo MakeAllocationInfo(
    void* address,
    uint32_t size,
    FreeFunctionKind kind = FreeFunctionKind::kFree,
    void* context = nullptr) {
  AllocationInfo info;
  info.address = address;
  info.size = size;
  info.free_fn_kind = kind;
#if BUILDFLAG(IS_APPLE)
  info.context = context;
#endif  // BUILDFLAG(IS_APPLE)
  return info;
}

constexpr size_t kMaxAllocationCount = 1;
constexpr size_t kMaxTotalSize = 128;
constexpr size_t kTotalSizeHighWaterMark = kMaxTotalSize - 1;
constexpr size_t kTotalSizeLowWaterMark = kTotalSizeHighWaterMark - 1;
constexpr size_t kEvictionChunkSize = 1;
constexpr size_t kEvictionTaskIntervalMs = 1000;

}  // namespace

class RandomEvictionQuarantineTest : public testing::Test {
 public:
  RandomEvictionQuarantineTest()
      : quarantine_(kMaxAllocationCount,
                    kMaxTotalSize,
                    kTotalSizeHighWaterMark,
                    kTotalSizeLowWaterMark,
                    kEvictionChunkSize,
                    kEvictionTaskIntervalMs) {}

  ~RandomEvictionQuarantineTest() override {
    // In production, we never destroy the quarantine. In unit tests,
    // we do so we need to safely stop the trimming timer first.
    quarantine_.task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&base::RepeatingTimer::Stop,
                       base::Unretained(&quarantine_.timer_)),
        task_environment_.QuitClosure());
    task_environment_.RunUntilQuit();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
  };

  TestRandomEvictionQuarantine quarantine_;
};

TEST_F(RandomEvictionQuarantineTest, WrongSize) {
  void* const kAllocPtr = reinterpret_cast<void*>(1);
  constexpr size_t kWrongSize1 = 0;
  constexpr size_t kWrongSize2 =
      TestRandomEvictionQuarantine::kMaxAllocationSize + 1;

  EXPECT_FALSE(quarantine_.Add(MakeAllocationInfo(kAllocPtr, kWrongSize1)));
  EXPECT_FALSE(quarantine_.Add(MakeAllocationInfo(kAllocPtr, kWrongSize2)));
}

TEST_F(RandomEvictionQuarantineTest, SingleAllocation) {
  void* const kAllocPtr = reinterpret_cast<void*>(1);
  constexpr size_t kAllocSize = 1;

  EXPECT_CALL(quarantine_, RecordAndZap(kAllocPtr, kAllocSize));

  EXPECT_TRUE(quarantine_.Add(MakeAllocationInfo(kAllocPtr, kAllocSize)));
  EXPECT_TRUE(quarantine_.HasAllocationForTesting(kAllocPtr));

  {
    base::AutoLock lock(quarantine_.lock_);
    EXPECT_EQ(quarantine_.total_size_, kAllocSize);
  }
}

TEST_F(RandomEvictionQuarantineTest, SlotReuse) {
  void* const kAllocPtr1 = reinterpret_cast<void*>(1);
  void* const kAllocPtr2 = reinterpret_cast<void*>(2);
  constexpr size_t kAllocSize = 1;

  auto allocation1 = MakeAllocationInfo(kAllocPtr1, kAllocSize);
  auto allocation2 = MakeAllocationInfo(kAllocPtr2, kAllocSize);

  EXPECT_CALL(quarantine_, RecordAndZap(_, kAllocSize)).Times(2);

  EXPECT_CALL(quarantine_, FinishFree(allocation1));

  EXPECT_TRUE(quarantine_.Add(allocation1));
  EXPECT_TRUE(quarantine_.HasAllocationForTesting(kAllocPtr1));

  EXPECT_TRUE(quarantine_.Add(allocation2));
  EXPECT_TRUE(quarantine_.HasAllocationForTesting(kAllocPtr2));
  EXPECT_FALSE(quarantine_.HasAllocationForTesting(kAllocPtr1));
}

TEST_F(RandomEvictionQuarantineTest, Trim) {
  void* const kAllocPtr = reinterpret_cast<void*>(1);
  constexpr size_t kAllocSize = kMaxTotalSize;

  auto allocation = MakeAllocationInfo(kAllocPtr, kAllocSize);

  EXPECT_CALL(quarantine_, FinishFree(allocation));
  EXPECT_CALL(quarantine_, RecordAndZap(kAllocPtr, kAllocSize));

  EXPECT_TRUE(quarantine_.Add(allocation));
  {
    base::AutoLock lock(quarantine_.lock_);
    EXPECT_EQ(quarantine_.total_size_, kAllocSize);
  }

  task_environment_.FastForwardBy(quarantine_.eviction_task_interval_);
  {
    base::AutoLock lock(quarantine_.lock_);
    EXPECT_EQ(quarantine_.total_size_, 0u);
  }
}

}  // namespace gwp_asan::internal::lud
