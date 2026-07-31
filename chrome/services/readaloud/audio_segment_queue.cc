// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/audio_segment_queue.h"

#include <algorithm>
#include <utility>

#include "base/time/time.h"

namespace readaloud {
namespace {
constexpr size_t kDefaultQueueCapacity = 512;
}  // namespace

AudioSegmentQueue::AudioSegmentQueue() {
  queue_.reserve(kDefaultQueueCapacity);
}

AudioSegmentQueue::~AudioSegmentQueue() {
  ClearInternal();
}

bool AudioSegmentQueue::Push(scoped_refptr<DecodedAudioSegment> segment) {
  if (!segment || segment->duration() <= base::TimeDelta() ||
      segment->duration().is_max()) {
    return false;
  }
  const int64_t duration_us = segment->duration().InMicroseconds();
  ReclaimPoppedSlots();
  base::AutoLock auto_lock(lock_);
  if (queue_.size() >= kDefaultQueueCapacity) {
    return false;
  }
  queue_.push_back(std::move(segment));
  buffered_duration_us_.fetch_add(duration_us, std::memory_order_relaxed);
  return true;
}

scoped_refptr<DecodedAudioSegment> AudioSegmentQueue::Pop() {
  base::AutoLock auto_lock(lock_);
  // We must hold the lock because `queue_.size()` is not thread-safe to read.
  // Since the lock is held, `popped_count_` cannot be modified by other threads
  // during this method's execution. Thus, we only need to perform the atomic
  // load once and store it in a local variable.
  const size_t current_popped = popped_count_.load(std::memory_order_relaxed);
  if (current_popped >= queue_.size()) {
    return nullptr;
  }
  // We move the segment pointer out of the deque instead of calling
  // pop_front() to prevent invoking the deque's internal reclamation logic,
  // which could trigger free() calls on the real-time thread.
  scoped_refptr<DecodedAudioSegment> segment =
      std::move(queue_[current_popped]);
  if (segment) {
    buffered_duration_us_.fetch_sub(segment->duration().InMicroseconds(),
                                    std::memory_order_relaxed);
  }
  popped_count_.fetch_add(1, std::memory_order_relaxed);
  return segment;
}

void AudioSegmentQueue::ReclaimPoppedSlots() {
  const size_t count = popped_count_.load();
  if (count == 0) {
    return;
  }
  base::AutoLock auto_lock(lock_);
  // Remove the null-ed out slots from the front of the queue. Since we are
  // on a non-real-time thread, deallocations here are safe. We only reclaim
  // up to `count` elements that were popped prior to lock acquisition.
  for (size_t i = 0; i < count; ++i) {
    queue_.pop_front();
  }
  popped_count_.fetch_sub(count, std::memory_order_relaxed);
}

base::TimeDelta AudioSegmentQueue::GetBufferedDuration() const {
  const int64_t buffered_duration_us =
      buffered_duration_us_.load(std::memory_order_relaxed);
  return base::Microseconds(std::max(int64_t{0}, buffered_duration_us));
}

void AudioSegmentQueue::Clear(base::PassKey<ReadAloudPlaybackController>) {
  ClearInternal();
}

void AudioSegmentQueue::ClearForTesting(base::PassKey<AudioSegmentQueueTest>) {
  ClearInternal();
}

void AudioSegmentQueue::ClearInternal() {
  base::circular_deque<scoped_refptr<DecodedAudioSegment>> local_queue;
  local_queue.reserve(kDefaultQueueCapacity);
  {
    base::AutoLock auto_lock(lock_);
    // Swap the queue to pre-allocated local containers under lock. This
    // delegates the actual deallocation of the old elements and their internal
    // storage buffers to the local variable, which will execute outside the
    // lock when it goes out of scope.
    std::swap(queue_, local_queue);
    popped_count_.store(0, std::memory_order_relaxed);
    buffered_duration_us_.store(0, std::memory_order_relaxed);
  }
}

}  // namespace readaloud
