// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/audio_segment_queue.h"

#include <atomic>
#include <memory>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "chrome/services/readaloud/decoded_audio_segment.h"
#include "chrome/services/readaloud/read_aloud_playback_controller.h"
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

  auto buffer1 = media::AudioBuffer::CreateEmptyBuffer(
      media::CHANNEL_LAYOUT_MONO, 1, 44100, 66150, base::TimeDelta());
  auto segment1 = base::MakeRefCounted<DecodedAudioSegment>(std::move(buffer1));

  auto buffer2 = media::AudioBuffer::CreateEmptyBuffer(
      media::CHANNEL_LAYOUT_STEREO, 2, 22050, 11025, base::TimeDelta());
  auto segment2 = base::MakeRefCounted<DecodedAudioSegment>(std::move(buffer2));

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
  // Because of lag-by-one, it will reclaim popped1 (since popped_count_ was 2)
  // leaving popped2 in the queue along with the newly pushed dummy.
  auto dummy =
      base::MakeRefCounted<DecodedAudioSegment>(base::Milliseconds(10));
  EXPECT_TRUE(queue_.Push(dummy));
  // size is 2 (popped2 + dummy)
  EXPECT_EQ(queue_.size(), 2u);
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
  // The lag-by-one keeps the very last popped item in the queue.
  auto dummy =
      base::MakeRefCounted<DecodedAudioSegment>(base::Milliseconds(10));
  EXPECT_TRUE(queue_.Push(dummy));
  EXPECT_EQ(2u, queue_.size());
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

  // Size of queue_ should now be 10 - 4 (popped reclaimed) + 1 (dummy) = 7.
  EXPECT_EQ(7u, queue_.size());
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

  // Pop two elements. The lag-by-one fix means popping 1 element leaves it in
  // the queue, preventing Push() from succeeding. Popping 2 elements allows
  // ReclaimPoppedSlots() to reclaim the first one, freeing up 1 slot!
  scoped_refptr<DecodedAudioSegment> popped = queue_.Pop();
  ASSERT_TRUE(popped);
  scoped_refptr<DecodedAudioSegment> popped_two = queue_.Pop();
  ASSERT_TRUE(popped_two);

  // Since pops are deferred, queue_.size() is still at capacity.
  EXPECT_EQ(kCapacity, queue_.size());

  // Pushing now should succeed because Push() auto-reclaims the first popped
  // element.
  auto post_pop_segment =
      base::MakeRefCounted<DecodedAudioSegment>(base::Milliseconds(10));
  EXPECT_TRUE(queue_.Push(post_pop_segment));
  // The queue size should remain at capacity.
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

namespace {
class ThreadTrackingAudioSegment : public DecodedAudioSegment {
 public:
  ThreadTrackingAudioSegment(base::TimeDelta duration,
                             base::PlatformThreadId* destruction_thread_id)
      : DecodedAudioSegment(duration),
        destruction_thread_id_(destruction_thread_id) {}

 protected:
  ~ThreadTrackingAudioSegment() override {
    if (destruction_thread_id_) {
      *destruction_thread_id_ = base::PlatformThread::CurrentId();
    }
  }

 private:
  raw_ptr<base::PlatformThreadId> destruction_thread_id_;
};

class AudioThreadSimulator : public base::SimpleThread {
 public:
  explicit AudioThreadSimulator(AudioSegmentQueue& queue,
                                base::WaitableEvent* pop_event)
      : base::SimpleThread("AudioThreadSimulator"),
        queue_(queue),
        pop_event_(pop_event) {}

  void Run() override {
    audio_thread_id_ = base::PlatformThread::CurrentId();
    // Simulate audio thread pop and use.
    scoped_refptr<DecodedAudioSegment> segment = queue_.get().Pop();
    EXPECT_TRUE(segment);
    // Release our reference (as the audio thread would do after its Render
    // loop)
    segment.reset();
    pop_event_->Signal();
  }

  base::PlatformThreadId audio_thread_id() const { return audio_thread_id_; }

 private:
  const raw_ref<AudioSegmentQueue> queue_;
  raw_ptr<base::WaitableEvent> pop_event_;
  base::PlatformThreadId audio_thread_id_ = base::kInvalidThreadId;
};
}  // namespace

TEST_F(AudioSegmentQueueTest, SegmentsHeldByAudioThreadAreNotDiscarded) {
  auto segment1 =
      base::MakeRefCounted<DecodedAudioSegment>(base::Milliseconds(10));
  auto segment2 =
      base::MakeRefCounted<DecodedAudioSegment>(base::Milliseconds(10));
  auto segment3 =
      base::MakeRefCounted<DecodedAudioSegment>(base::Milliseconds(10));

  EXPECT_TRUE(queue_.Push(segment1));
  EXPECT_EQ(1u, queue_.size());

  // Simulate audio thread popping the first segment.
  // The queue should now have popped_count_ = 1.
  scoped_refptr<DecodedAudioSegment> popped1 = queue_.Pop();
  EXPECT_TRUE(popped1);
  EXPECT_EQ(1u, queue_.size());

  // Push the second segment. This triggers ReclaimPoppedSlots().
  // Because of the lag-by-one fix, popped_count_ = 1 will not cause a
  // reclamation. The queue should NOT discard segment1 while the audio thread
  // might still be using it.
  EXPECT_TRUE(queue_.Push(segment2));

  // The queue size should be 2, containing both segment1 and segment2.
  // (If it had incorrectly reclaimed segment1, the size would be 1).
  EXPECT_EQ(2u, queue_.size());

  // Simulate audio thread popping the second segment.
  // popped_count_ becomes 2.
  scoped_refptr<DecodedAudioSegment> popped2 = queue_.Pop();
  EXPECT_TRUE(popped2);
  EXPECT_EQ(2u, queue_.size());

  // Push the third segment. This triggers ReclaimPoppedSlots().
  // popped_count_ = 2, so it will now reclaim 1 element (segment1).
  EXPECT_TRUE(queue_.Push(segment3));

  // The queue size should remain 2 (segment2 and segment3 are in the queue,
  // segment1 is finally gone).
  EXPECT_EQ(2u, queue_.size());
}

TEST_F(AudioSegmentQueueTest, DeallocationDoesNotHappenInAudioThread) {
  base::PlatformThreadId destruction_thread_id = base::kInvalidThreadId;
  base::WaitableEvent pop_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  auto segment = base::MakeRefCounted<ThreadTrackingAudioSegment>(
      base::Milliseconds(10), &destruction_thread_id);

  EXPECT_TRUE(queue_.Push(segment));
  segment.reset();  // main thread drops reference

  AudioThreadSimulator audio_thread(queue_, &pop_event);
  audio_thread.Start();
  pop_event.Wait();

  // Audio thread has popped the segment and dropped its reference.
  // Thanks to the lag-by-one fix, the queue should still hold a reference.
  // Therefore, destruction_thread_id should still be kInvalidThreadId.
  EXPECT_EQ(base::kInvalidThreadId, destruction_thread_id);

  // Push a dummy segment. popped_count_ = 1, so it reclaims nothing.
  auto dummy1 =
      base::MakeRefCounted<DecodedAudioSegment>(base::Milliseconds(10));
  EXPECT_TRUE(queue_.Push(dummy1));

  // The segment should still be alive in the queue.
  EXPECT_EQ(base::kInvalidThreadId, destruction_thread_id);

  // Pop the dummy segment to increment popped_count_ to 2.
  // This simulates the audio thread moving on to the next segment.
  (void)queue_.Pop();

  // Push a second dummy segment. This triggers ReclaimPoppedSlots() with
  // popped_count_ = 2. It will reclaim 1 element (the original segment).
  // Because no one else holds it, it will be destroyed right here on the main
  // thread!
  auto dummy2 =
      base::MakeRefCounted<DecodedAudioSegment>(base::Milliseconds(10));
  EXPECT_TRUE(queue_.Push(dummy2));

  audio_thread.Join();

  // Destruction should have occurred naturally on the main thread during
  // Push().
  EXPECT_EQ(base::PlatformThread::CurrentId(), destruction_thread_id);
  EXPECT_NE(audio_thread.audio_thread_id(), destruction_thread_id);
}

}  // namespace readaloud
