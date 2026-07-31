// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/audio_segment_queue.h"

#include <atomic>
#include <memory>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "chrome/services/readaloud/decoded_audio_segment.h"
#include "chrome/services/readaloud/read_aloud_playback_controller.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

class AudioSegmentQueueTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  AudioSegmentQueue queue_;

  base::PassKey<AudioSegmentQueueTest> GetPassKey() const {
    return base::PassKey<AudioSegmentQueueTest>();
  }
};

class ClearThread : public base::SimpleThread {
 public:
  ClearThread(AudioSegmentQueue& queue,
              std::atomic<bool>& start,
              base::PassKey<AudioSegmentQueueTest> passkey)
      : base::SimpleThread("ClearThread"),
        queue_(queue),
        start_(start),
        passkey_(passkey) {}

  void Run() override {
    while (!start_->load(std::memory_order_relaxed)) {
    }
    queue_->ClearForTesting(passkey_);
  }

 private:
  const raw_ref<AudioSegmentQueue> queue_;
  const raw_ref<std::atomic<bool>> start_;
  const base::PassKey<AudioSegmentQueueTest> passkey_;
};

namespace {

class ProducerThread : public base::SimpleThread {
 public:
  ProducerThread(AudioSegmentQueue& queue, size_t count)
      : base::SimpleThread("ProducerThread"), queue_(queue), count_(count) {}

  void Run() override {
    for (size_t i = 0; i < count_; ++i) {
      auto segment =
          base::MakeRefCounted<DecodedAudioSegment>(base::Milliseconds(10));
      EXPECT_TRUE(queue_->Push(segment));
    }
  }

 private:
  const raw_ref<AudioSegmentQueue> queue_;
  size_t count_;
};

class ConsumerThread : public base::SimpleThread {
 public:
  ConsumerThread(AudioSegmentQueue& queue, size_t count)
      : base::SimpleThread("ConsumerThread"), queue_(queue), count_(count) {}

  void Run() override {
    size_t consumed = 0;
    while (consumed < count_) {
      scoped_refptr<DecodedAudioSegment> segment = queue_->Pop();
      if (segment) {
        consumed++;
      } else {
        base::PlatformThread::YieldCurrentThread();
      }
    }
  }

 private:
  const raw_ref<AudioSegmentQueue> queue_;
  size_t count_;
};

class PopThread : public base::SimpleThread {
 public:
  PopThread(AudioSegmentQueue& queue, std::atomic<bool>& start)
      : base::SimpleThread("PopThread"), queue_(queue), start_(start) {}

  void Run() override {
    while (!start_->load(std::memory_order_relaxed)) {
    }
    (void)queue_->Pop();
  }

 private:
  const raw_ref<AudioSegmentQueue> queue_;
  const raw_ref<std::atomic<bool>> start_;
};

}  // namespace

TEST_F(AudioSegmentQueueTest, PushAndPopBasic) {
  EXPECT_EQ(0u, queue_.size());
  EXPECT_EQ(base::TimeDelta(), queue_.GetBufferedDuration());

  auto segment1 = base::MakeRefCounted<DecodedAudioSegment>(
      media::AudioBus::Create(1, 1024), 44100, base::Milliseconds(1500));
  auto segment2 = base::MakeRefCounted<DecodedAudioSegment>(
      media::AudioBus::Create(2, 512), 22050, base::Milliseconds(500));

  EXPECT_TRUE(queue_.Push(segment1));
  EXPECT_EQ(1u, queue_.size());
  EXPECT_EQ(base::Milliseconds(1500), queue_.GetBufferedDuration());

  EXPECT_TRUE(queue_.Push(segment2));
  EXPECT_EQ(2u, queue_.size());
  EXPECT_EQ(base::Milliseconds(2000), queue_.GetBufferedDuration());

  scoped_refptr<DecodedAudioSegment> popped1 = queue_.Pop();
  ASSERT_TRUE(popped1);
  EXPECT_EQ(segment1, popped1);
  // Buffered duration drops immediately upon pop.
  EXPECT_EQ(base::Milliseconds(500), queue_.GetBufferedDuration());

  scoped_refptr<DecodedAudioSegment> popped2 = queue_.Pop();
  ASSERT_TRUE(popped2);
  EXPECT_EQ(segment2, popped2);
  EXPECT_EQ(base::TimeDelta(), queue_.GetBufferedDuration());

  // Deque itself still contains empty pointer slots until reclaimed.
  EXPECT_EQ(2u, queue_.size());

  // Push dummy to trigger reclaim.
  auto dummy =
      base::MakeRefCounted<DecodedAudioSegment>(base::Milliseconds(10));
  EXPECT_TRUE(queue_.Push(dummy));
  EXPECT_EQ(1u, queue_.size());
}

TEST_F(AudioSegmentQueueTest, RejectInvalidSegments) {
  // Reject nullptr.
  EXPECT_FALSE(queue_.Push(nullptr));

  // Reject zero duration.
  auto zero_segment =
      base::MakeRefCounted<DecodedAudioSegment>(base::TimeDelta());
  EXPECT_FALSE(queue_.Push(zero_segment));

  // Reject negative duration.
  auto negative_segment =
      base::MakeRefCounted<DecodedAudioSegment>(base::Seconds(-1));
  EXPECT_FALSE(queue_.Push(negative_segment));

  // Reject infinite duration.
  auto max_segment =
      base::MakeRefCounted<DecodedAudioSegment>(base::TimeDelta::Max());
  EXPECT_FALSE(queue_.Push(max_segment));

  EXPECT_EQ(0u, queue_.size());
  EXPECT_EQ(base::TimeDelta(), queue_.GetBufferedDuration());
}

TEST_F(AudioSegmentQueueTest, BufferedDurationTrackingWithoutLimit) {
  auto segment = base::MakeRefCounted<DecodedAudioSegment>(base::Seconds(2));

  EXPECT_TRUE(queue_.Push(segment));
  EXPECT_EQ(base::Seconds(2), queue_.GetBufferedDuration());

  scoped_refptr<DecodedAudioSegment> popped = queue_.Pop();
  ASSERT_TRUE(popped);
  EXPECT_EQ(base::TimeDelta(), queue_.GetBufferedDuration());
}

TEST_F(AudioSegmentQueueTest, ClearEmptiesQueueAndResetsDuration) {
  auto segment1 = base::MakeRefCounted<DecodedAudioSegment>(base::Seconds(2));
  auto segment2 = base::MakeRefCounted<DecodedAudioSegment>(base::Seconds(3));

  EXPECT_TRUE(queue_.Push(segment1));
  EXPECT_TRUE(queue_.Push(segment2));

  EXPECT_EQ(2u, queue_.size());
  EXPECT_EQ(base::Seconds(5), queue_.GetBufferedDuration());

  queue_.ClearForTesting(GetPassKey());

  EXPECT_EQ(0u, queue_.size());
  EXPECT_EQ(base::TimeDelta(), queue_.GetBufferedDuration());
}

TEST_F(AudioSegmentQueueTest, MicrosecondPrecisionAccumulation) {
  // Push 3 segments with sub-millisecond durations (e.g. 1500 microseconds
  // = 1.5ms).
  auto segment1 =
      base::MakeRefCounted<DecodedAudioSegment>(base::Microseconds(1500));
  auto segment2 =
      base::MakeRefCounted<DecodedAudioSegment>(base::Microseconds(2500));
  auto segment3 =
      base::MakeRefCounted<DecodedAudioSegment>(base::Microseconds(1000));

  EXPECT_TRUE(queue_.Push(segment1));
  EXPECT_TRUE(queue_.Push(segment2));
  EXPECT_TRUE(queue_.Push(segment3));

  // Total should be exactly 5000 microseconds (5ms).
  EXPECT_EQ(base::Milliseconds(5), queue_.GetBufferedDuration());

  scoped_refptr<DecodedAudioSegment> popped = queue_.Pop();
  ASSERT_TRUE(popped);
  EXPECT_EQ(base::Microseconds(3500), queue_.GetBufferedDuration());
}

TEST_F(AudioSegmentQueueTest, MultiThreadedProducerConsumerStressTest) {
  constexpr size_t kItemsPerProducer = 500;

  ProducerThread producer(queue_, kItemsPerProducer);
  ConsumerThread consumer(queue_, kItemsPerProducer);

  producer.Start();
  consumer.Start();

  producer.Join();
  consumer.Join();

  EXPECT_EQ(base::TimeDelta(), queue_.GetBufferedDuration());

  // Push dummy to trigger reclaim.
  auto dummy =
      base::MakeRefCounted<DecodedAudioSegment>(base::Milliseconds(10));
  EXPECT_TRUE(queue_.Push(dummy));
  EXPECT_EQ(1u, queue_.size());
  EXPECT_EQ(base::Milliseconds(10), queue_.GetBufferedDuration());
}

TEST_F(AudioSegmentQueueTest, DeferredPopSpaceReclamation) {
  // Push 10 segments.
  for (size_t i = 0; i < 10; ++i) {
    auto segment =
        base::MakeRefCounted<DecodedAudioSegment>(base::Milliseconds(100));
    EXPECT_TRUE(queue_.Push(segment));
  }

  EXPECT_EQ(10u, queue_.size());

  // Pop 5 segments.
  for (size_t i = 0; i < 5; ++i) {
    scoped_refptr<DecodedAudioSegment> popped = queue_.Pop();
    ASSERT_TRUE(popped);
  }

  // Size of queue_ is still 10 because pops are deferred.
  EXPECT_EQ(10u, queue_.size());

  // Push dummy to trigger reclaim.
  auto dummy =
      base::MakeRefCounted<DecodedAudioSegment>(base::Milliseconds(10));
  EXPECT_TRUE(queue_.Push(dummy));

  // Size of queue_ should now be 10 - 5 (popped) + 1 (dummy) = 6.
  EXPECT_EQ(6u, queue_.size());
}

TEST_F(AudioSegmentQueueTest, PushQueueOverflowHandling) {
  constexpr size_t kCapacity = 512;
  for (size_t i = 0; i < kCapacity; ++i) {
    auto segment =
        base::MakeRefCounted<DecodedAudioSegment>(base::Milliseconds(10));
    EXPECT_TRUE(queue_.Push(segment));
  }
  EXPECT_EQ(kCapacity, queue_.size());

  // Overflow push should be rejected because queue_ size reaches capacity.
  auto overflow_segment =
      base::MakeRefCounted<DecodedAudioSegment>(base::Milliseconds(10));
  EXPECT_FALSE(queue_.Push(overflow_segment));
  EXPECT_EQ(kCapacity, queue_.size());

  // Pop one element.
  scoped_refptr<DecodedAudioSegment> popped = queue_.Pop();
  ASSERT_TRUE(popped);

  // Since pops are deferred, queue_.size() is still at capacity.
  EXPECT_EQ(kCapacity, queue_.size());

  // Pushing now should succeed because Push() auto-reclaims.
  auto post_pop_segment =
      base::MakeRefCounted<DecodedAudioSegment>(base::Milliseconds(10));
  EXPECT_TRUE(queue_.Push(post_pop_segment));
  EXPECT_EQ(kCapacity, queue_.size());
}

TEST_F(AudioSegmentQueueTest, ClearDuringPopDoesNotCorruptDuration) {
  constexpr size_t kIterations = 500;
  for (size_t i = 0; i < kIterations; ++i) {
    auto segment1 =
        base::MakeRefCounted<DecodedAudioSegment>(base::Milliseconds(100));
    auto segment2 =
        base::MakeRefCounted<DecodedAudioSegment>(base::Milliseconds(200));
    EXPECT_TRUE(queue_.Push(segment1));
    EXPECT_TRUE(queue_.Push(segment2));

    std::atomic<bool> start{false};

    PopThread pop_thread(queue_, start);
    ClearThread clear_thread(queue_, start, GetPassKey());

    pop_thread.Start();
    clear_thread.Start();
    start.store(true, std::memory_order_relaxed);

    pop_thread.Join();
    clear_thread.Join();

    // Duration must be 0 and non-negative after clear and pop complete.
    EXPECT_GE(queue_.GetBufferedDuration(), base::TimeDelta());
    EXPECT_EQ(base::TimeDelta(), queue_.GetBufferedDuration());
    EXPECT_EQ(0u, queue_.size());

    // Pushing a new segment must track duration precisely without corruption.
    auto new_segment =
        base::MakeRefCounted<DecodedAudioSegment>(base::Milliseconds(50));
    EXPECT_TRUE(queue_.Push(new_segment));
    EXPECT_EQ(base::Milliseconds(50), queue_.GetBufferedDuration());

    queue_.ClearForTesting(GetPassKey());
  }
}

}  // namespace readaloud
