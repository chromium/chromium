// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "base/memory/raw_ptr.h"
#include "base/threading/platform_thread.h"
#include "components/viz/service/debugger/rwlock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// Shared global variables
int writer_count = 0;
static const unsigned kNumCounts = 1000000;

class RWLockTest : public testing::Test {
 private:
 public:
  static rwlock::RWLock test_rwlock;
};

rwlock::RWLock RWLockTest::test_rwlock;

// The writer thread reads through the entire vector at a certain
// state left by the readers, sums up all the existing integer
// values, stores it, and resets the vector to provide a blank
// slate for the reader threads to continue working on.
class WriterThread : public base::PlatformThread::Delegate {
 private:
  raw_ptr<std::vector<int>> array_;
  int size_;
  volatile int delay_counter = 0;
  static const unsigned kNumWriterTries = 100;
  static const unsigned kNumTimeDelay = 500000;

 public:
  WriterThread() = default;

  void ThreadMain() override {
    for (uint32_t writer_try = 0; writer_try < kNumWriterTries; ++writer_try) {
      RWLockTest::test_rwlock.WriteLock();

      for (int i = 0; i < size_; ++i) {
        writer_count += (*array_)[i];
        (*array_)[i] = 0;
      }
      // Reset the shared vector.
      (*array_).clear();
      (*array_).resize(size_);

      RWLockTest::test_rwlock.WriteUnLock();

      // Time delay so that the writer thread does not
      // "burn out" too fast.
      for (uint32_t _ = 0; _ < kNumTimeDelay; ++_) {
        delay_counter = delay_counter + 1;
      }
    }
  }

  void Init(std::vector<int>* array, int size) {
    array_ = array;
    size_ = size;
  }
};

// Each reader thread has a corresponding slot within the shared
// vector. Within each slot, each reader thread increments the slot's
// integer value by 1 for k number (kNumCounts) of iterations.
// Within each iteration, there is a small time buffer/delay (kNumTimeDelay)
// to allow the writer thread to squeeze and take the lock.
class ReaderThread : public base::PlatformThread::Delegate {
 private:
  static const unsigned kNumTimeDelay = 100;
  volatile int delay_counter = 0;
  raw_ptr<std::vector<int>> array_;
  int array_index;

 public:
  ReaderThread() = default;

  ReaderThread(const ReaderThread&) = delete;
  ReaderThread& operator=(const ReaderThread&) = delete;

  void ThreadMain() override {
    for (uint32_t count = 0; count < kNumCounts; ++count) {
      RWLockTest::test_rwlock.ReadLock();
      ++(*array_)[array_index];
      RWLockTest::test_rwlock.ReadUnlock();

      // Time buffer/delay element for writer to squeeze in
      for (uint32_t _ = 0; _ < kNumTimeDelay; ++_) {
        delay_counter = delay_counter + 1;
      }
    }
  }

  void Init(std::vector<int>* array, int index) {
    array_ = array;
    array_index = index;
  }
};

TEST_F(RWLockTest, ReadWrite) {
  static const unsigned kNumReaders = 4;
  std::vector<int> arr(kNumReaders);  // Must outlive `writer` and `readers`.
  WriterThread writer;
  ReaderThread readers[kNumReaders];
  base::PlatformThreadHandle handles[kNumReaders];

  // Initialize and start each reader thread.
  for (uint32_t i = 0; i < kNumReaders; ++i) {
    readers[i].Init(&arr, i);
  }
  for (uint32_t i = 0; i < kNumReaders; ++i) {
    ASSERT_TRUE(base::PlatformThread::Create(0, &readers[i], &handles[i]));
  }

  // Initialize and start writer thread.
  writer.Init(&arr, kNumReaders);
  base::PlatformThreadHandle writer_handle;
  ASSERT_TRUE(base::PlatformThread::Create(0, &writer, &writer_handle));

  // Collect all reader threads.
  for (auto& handle : handles) {
    base::PlatformThread::Join(handle);
  }

  // Collect writer thread.
  base::PlatformThread::Join(writer_handle);

  int total_sum = 0;

  // Add number counted by the remainder of reader threads.
  for (uint32_t i = 0; i < kNumReaders; ++i) {
    total_sum += arr[i];
  }

  // Add number counted by writer thread.
  total_sum += writer_count;

  EXPECT_EQ(static_cast<uint32_t>(total_sum), kNumReaders * kNumCounts);
}
}  // namespace
