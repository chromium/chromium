// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/lightweight_detector/random_eviction_quarantine.h"

#include <type_traits>

#include "base/auto_reset.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gwp_asan::internal::lud {

using ::testing::_;
using ::testing::Return;

namespace {

// Exposes the static `Get` method that points to the current mock instance.
template <typename T>
class StaticWrapper {
 public:
  class Holder {
   public:
    Holder() : reset_(&StaticWrapper<T>::instance_, &instance_) {}
    Holder(Holder&) = delete;
    Holder& operator=(const Holder&) = delete;

   private:
    T instance_;
    base::AutoReset<T*> reset_;
  };

  static T* Get() { return instance_; }

 private:
  inline static T* instance_ = nullptr;

  friend class Holder;
};

// Forwards a static method to the current mock instance.
#define MOCK_STATIC_METHOD(ReturnType, MethodName, ...)      \
  MOCK_METHOD(ReturnType, MethodName, __VA_ARGS__);          \
  template <typename... Args>                                \
  static ReturnType MethodName(Args&&... args) {             \
    CHECK(Get());                                            \
    if constexpr (!std::is_same_v<ReturnType, void>) {       \
      return Get()->MethodName(std::forward<Args>(args)...); \
    } else {                                                 \
      Get()->MethodName(std::forward<Args>(args)...);        \
    }                                                        \
  }                                                          \
  struct __swallow_semicolon_##MethodName {}

class MockMetadataRecorder : public StaticWrapper<MockMetadataRecorder> {
 public:
  MOCK_METHOD(void, RecordAndZap, (void* ptr, size_t size));
};

class MockShimSupport : public StaticWrapper<MockShimSupport> {
 public:
  MOCK_STATIC_METHOD(size_t, NextGetSizeEstimate, (void* ptr));
  MOCK_STATIC_METHOD(void, NextFree, (void* ptr));
};

constexpr size_t kMaxAllocationCount = 1;
constexpr size_t kMaxTotalSize = 128;
constexpr size_t kTotalSizeHighWaterMark = kMaxTotalSize - 1;
constexpr size_t kTotalSizeLowWaterMark = kTotalSizeHighWaterMark - 1;
constexpr size_t kEvictionChunkSize = 1;
constexpr size_t kEvictionTaskIntervalMs = 1000;

}  // namespace

class RandomEvictionQuarantineTest : public testing::Test {
 public:
  using Quarantine =
      RandomEvictionQuarantineImpl<MockMetadataRecorder, MockShimSupport>;

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

  Quarantine quarantine_;
  MockMetadataRecorder::Holder recorder_holder_;
  MockShimSupport::Holder support_holder_;
};

TEST_F(RandomEvictionQuarantineTest, WrongSize) {
  void* const kAllocPtr = reinterpret_cast<void*>(1);

  EXPECT_CALL(*MockShimSupport::Get(), NextGetSizeEstimate(kAllocPtr))
      .WillOnce(Return(0))
      .WillOnce(Return(Quarantine::kMaxAllocationSize + 1));

  EXPECT_FALSE(quarantine_.Add(kAllocPtr));
  EXPECT_FALSE(quarantine_.Add(kAllocPtr));
}

TEST_F(RandomEvictionQuarantineTest, SingleAllocation) {
  void* const kAllocPtr = reinterpret_cast<void*>(1);
  constexpr size_t kAllocSize = 1;

  EXPECT_CALL(*MockShimSupport::Get(), NextGetSizeEstimate(kAllocPtr))
      .WillRepeatedly(Return(1));

  EXPECT_CALL(*MockMetadataRecorder::Get(),
              RecordAndZap(kAllocPtr, kAllocSize));

  EXPECT_TRUE(quarantine_.Add(kAllocPtr));
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

  EXPECT_CALL(*MockShimSupport::Get(), NextGetSizeEstimate)
      .WillRepeatedly(Return(1));

  EXPECT_CALL(*MockMetadataRecorder::Get(), RecordAndZap(_, kAllocSize))
      .Times(2);

  EXPECT_CALL(*MockShimSupport::Get(), NextFree(kAllocPtr1));

  EXPECT_TRUE(quarantine_.Add(kAllocPtr1));
  EXPECT_TRUE(quarantine_.HasAllocationForTesting(kAllocPtr1));

  EXPECT_TRUE(quarantine_.Add(kAllocPtr2));
  EXPECT_TRUE(quarantine_.HasAllocationForTesting(kAllocPtr2));
  EXPECT_FALSE(quarantine_.HasAllocationForTesting(kAllocPtr1));
}

TEST_F(RandomEvictionQuarantineTest, Trim) {
  void* const kAllocPtr = reinterpret_cast<void*>(1);
  constexpr size_t kAllocSize = kMaxTotalSize;

  EXPECT_CALL(*MockShimSupport::Get(), NextGetSizeEstimate(kAllocPtr))
      .WillOnce(Return(kAllocSize));

  EXPECT_CALL(*MockShimSupport::Get(), NextFree(kAllocPtr));

  EXPECT_CALL(*MockMetadataRecorder::Get(),
              RecordAndZap(kAllocPtr, kAllocSize));

  EXPECT_TRUE(quarantine_.Add(kAllocPtr));
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
