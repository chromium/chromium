// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_DECODED_AUDIO_SEGMENT_H_
#define CHROME_SERVICES_READALOUD_DECODED_AUDIO_SEGMENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"

namespace media {
class AudioBus;
}

namespace readaloud {

// Represents a decoded audio segment and associated timing metadata.
// Reference counting guarantees that memory stays valid across thread
// boundaries (e.g., passing from decoder to real-time audio thread).
class DecodedAudioSegment
    : public base::RefCountedThreadSafe<DecodedAudioSegment> {
 public:
  // Struct tracking timing metadata for a single word within this audio
  // segment. Used to synchronize visual text highlighting in the UI with the
  // spoken words during playback.
  struct WordTiming {
    // Text string corresponding to the spoken word.
    std::string text;
    // Start time offset relative to the segment start.
    base::TimeDelta start_time;
    // End time offset relative to the segment start.
    base::TimeDelta end_time;
  };

  // Constructs an empty audio segment with zero duration.
  DecodedAudioSegment();

  // Constructs a dummy audio segment with specified duration but no audio bus.
  explicit DecodedAudioSegment(base::TimeDelta duration);

  // Constructs a complete audio segment with audio data, sample rate, duration,
  // and word timings.
  DecodedAudioSegment(std::unique_ptr<media::AudioBus> audio_bus,
                      int sample_rate,
                      base::TimeDelta duration,
                      std::vector<WordTiming> word_timings = {});

  // DecodedAudioSegment is ref-counted and its ownership is shared. To prevent
  // accidental expensive copies of the underlying audio buffer, it is not
  // copyable or assignable.
  DecodedAudioSegment(const DecodedAudioSegment&) = delete;
  DecodedAudioSegment& operator=(const DecodedAudioSegment&) = delete;

  // Returns pointer to the underlying AudioBus, or nullptr if none.
  [[nodiscard]] const media::AudioBus* audio_bus() const {
    return audio_bus_.get();
  }

  // Returns sample rate in Hz.
  [[nodiscard]] int sample_rate() const { return sample_rate_; }

  // Returns total duration of this audio segment.
  [[nodiscard]] base::TimeDelta duration() const { return duration_; }

  // Returns list of word timing markers for sync highlighting.
  [[nodiscard]] const std::vector<WordTiming>& word_timings() const {
    return word_timings_;
  }

 protected:
  friend class base::RefCountedThreadSafe<DecodedAudioSegment>;
  virtual ~DecodedAudioSegment();

 private:
  std::unique_ptr<media::AudioBus> audio_bus_;
  int sample_rate_ = 0;
  base::TimeDelta duration_;
  std::vector<WordTiming> word_timings_;
};

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_DECODED_AUDIO_SEGMENT_H_
