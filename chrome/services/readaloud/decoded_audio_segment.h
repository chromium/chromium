// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_DECODED_AUDIO_SEGMENT_H_
#define CHROME_SERVICES_READALOUD_DECODED_AUDIO_SEGMENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "media/base/audio_buffer.h"

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

  // Constructs a dummy audio segment with specified duration but no audio
  // buffer.
  explicit DecodedAudioSegment(base::TimeDelta duration);

  // Constructs a complete audio segment with audio data and word timings.
  explicit DecodedAudioSegment(scoped_refptr<media::AudioBuffer> audio_buffer,
                               std::vector<WordTiming> word_timings = {});

  // DecodedAudioSegment is ref-counted and its ownership is shared. To prevent
  // accidental expensive copies of the underlying audio buffer, it is not
  // copyable or assignable.
  DecodedAudioSegment(const DecodedAudioSegment&) = delete;
  DecodedAudioSegment& operator=(const DecodedAudioSegment&) = delete;

  // Returns the underlying AudioBuffer, or nullptr if none.
  [[nodiscard]] scoped_refptr<media::AudioBuffer> audio_buffer() const {
    return audio_buffer_;
  }

  // Returns sample rate in Hz.
  [[nodiscard]] int sample_rate() const {
    return audio_buffer_ ? audio_buffer_->sample_rate() : sample_rate_;
  }

  // Returns total duration of this audio segment.
  [[nodiscard]] base::TimeDelta duration() const {
    return audio_buffer_ ? audio_buffer_->duration() : duration_;
  }

  // Returns list of word timing markers for sync highlighting.
  [[nodiscard]] const std::vector<WordTiming>& word_timings() const {
    return word_timings_;
  }

 protected:
  friend class base::RefCountedThreadSafe<DecodedAudioSegment>;
  virtual ~DecodedAudioSegment();

 private:
  scoped_refptr<media::AudioBuffer> audio_buffer_;
  int sample_rate_ = 0;
  base::TimeDelta duration_;
  std::vector<WordTiming> word_timings_;
};

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_DECODED_AUDIO_SEGMENT_H_
