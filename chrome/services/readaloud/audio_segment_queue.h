// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_AUDIO_SEGMENT_QUEUE_H_
#define CHROME_SERVICES_READALOUD_AUDIO_SEGMENT_QUEUE_H_

#include <atomic>
#include <cstddef>

#include "base/containers/circular_deque.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "chrome/services/readaloud/decoded_audio_segment.h"

namespace readaloud {

class ReadAloudPlaybackController;
class AudioSegmentQueueTest;

// Thread-safe queue managing decoded audio segments between the producer
// (decoder pipeline) and consumer (real-time audio thread).
//
// Threading & Real-time Audio Guarantees:
// - Pop() is safe to call from the real-time audio thread. It retrieves
//   a segment pointer without executing C++ destructors or deallocating heap
//   memory on the audio thread.
// - ReclaimPoppedSlots() must be called periodically from a non-real-time
//   thread (e.g. browser main thread or background worker) to safely reclaim
//   popped pointer slots and keep queue memory bounded.
class AudioSegmentQueue final {
 public:
  AudioSegmentQueue();

  AudioSegmentQueue(const AudioSegmentQueue&) = delete;
  AudioSegmentQueue& operator=(const AudioSegmentQueue&) = delete;
  AudioSegmentQueue(AudioSegmentQueue&&) = delete;
  AudioSegmentQueue& operator=(AudioSegmentQueue&&) = delete;

  ~AudioSegmentQueue();

  // Pushes a segment into the queue.
  // Returns true on success, or false if the segment is invalid (nullptr,
  // non-positive, or max duration).
  //
  // Threading: MUST NOT be called from the real-time audio thread.
  // Called by the decoder pipeline (non-real-time producer thread).
  [[nodiscard]] bool Push(scoped_refptr<DecodedAudioSegment> segment);

  // Pops the next segment from the queue. Returns nullptr if empty.
  //
  // Threading: Safe to call from the real-time audio thread.
  // Called by the audio renderer (real-time consumer thread).
  [[nodiscard]] scoped_refptr<DecodedAudioSegment> Pop();

  // Returns the current size of the queue (including both popped and unpopped
  // elements).
  [[nodiscard]] size_t size() const {
    base::AutoLock auto_lock(lock_);
    return queue_.size();
  }

  // Returns total duration currently buffered in the play queue.
  //
  // Threading: Safe to call from any thread.
  [[nodiscard]] base::TimeDelta GetBufferedDuration() const;

  // Empties all queued segments safely.
  //
  // Threading: MUST NOT be called from the real-time audio thread.
  // Must be called from a non-time-sensitive background thread or main thread.
  void Clear(base::PassKey<ReadAloudPlaybackController>);

  // Test-only version of Clear.
  void ClearForTesting(base::PassKey<AudioSegmentQueueTest>);

 private:
  // Reclaims empty pointer slots from the front of the deque.
  //
  // Threading: MUST NOT be called from the real-time audio thread.
  // Must be called from a non-time-sensitive background thread or main thread.
  void ReclaimPoppedSlots();

  void ClearInternal();

  mutable base::Lock lock_;
  base::circular_deque<scoped_refptr<DecodedAudioSegment>> queue_
      GUARDED_BY(lock_);
  std::atomic<size_t> popped_count_{0};
  std::atomic<int64_t> buffered_duration_us_{0};
};

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_AUDIO_SEGMENT_QUEUE_H_
