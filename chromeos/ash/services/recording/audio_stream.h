// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_RECORDING_AUDIO_STREAM_H_
#define CHROMEOS_ASH_SERVICES_RECORDING_AUDIO_STREAM_H_

#include <memory>
#include <string_view>

#include "base/containers/queue.h"
#include "base/time/time.h"

namespace media {
class AudioBus;
}  // namespace media

namespace recording {

// Defines a contiguous stream of audio buses that can be consumed and mixed
// with audio frames in a destination audio bus.
class AudioStream {
 public:
  explicit AudioStream(std::string_view name);
  AudioStream(const AudioStream&) = delete;
  AudioStream& operator=(const AudioStream&) = delete;
  ~AudioStream();

  std::string_view name() const { return name_; }
  int total_frames() const { return total_frames_; }
  bool empty() const { return total_frames_ == 0; }

  base::TimeTicks begin_timestamp() const { return begin_timestamp_; }
  base::TimeTicks end_timestamp() const { return end_timestamp_; }

  // Appends the given `audio_bus`, whose first frame's `timestamp` is given, to
  // `stream_fifo_`. `timestamp` is significant:
  // - If `timestamp` is less than `end_timestamp_`, it will be adjusted to be
  //   equal. This guarantees monotonically increasing timestamps of all sub-
  //   sequent audio buses (see http://b/277656502).
  // - If the stream is empty, `timestamp` will be used as the new
  //   `begin_timestamp_`, and `audio_bus` will be the very first bus in the
  //   stream.
  // - If `timestamp` is larger than `end_timestamp_` and the stream is not
  //   empty, meaning that the new to-be-appended `audio_bus` is not directly
  //   contiguous to the last audio bus in the `stream_fifo_`, a new zero-filled
  //   audio bus will be appended first to fill this gap, then `audio_bus` will
  //   be appended at the end.
  void AppendAudioBus(std::unique_ptr<media::AudioBus> audio_bus,
                      base::TimeTicks timestamp);

  // Consumes a number of frames from `stream_fifo_` that is equal to
  // `frames_to_consume` and adds them to the existing frames in the
  // `destination` audio bus starting at `destination_start_frame`. The consumed
  // frames are removed from the `stream_fifo_`, and `total_frames_`,
  // `begin_timestamp_`, and `end_timestamp_` are all updated accordingly.
  void ConsumeAndAccumulateTo(media::AudioBus* destination,
                              int destination_start_frame,
                              int frames_to_consume);

 private:
  // Appends a zero-filled bus that has the given number of `frames` at the end
  // of the stream.
  void AppendSilence(int64_t frames);

  void AppendAudioBusInternal(std::unique_ptr<media::AudioBus> audio_bus,
                              base::TimeTicks timestamp);

  // Updates the `end_timestamp_` based on the current `begin_timestamp_`,
  // `total_frames_`, and the default sampling rate which determines the
  // duration of each frame.
  void UpdateEndTimestamp();

  // The name of this stream. Used for debugging only.
  const std::string name_;

  base::queue<std::unique_ptr<media::AudioBus>> stream_fifo_;

  // The sum of all audio frames from all the audio buses currently in
  // `stream_fifo_`.
  int total_frames_ = 0;

  // The timestamp of the very first audio frame in the very first audio bus in
  // the front of the `stream_fifo_`.
  base::TimeTicks begin_timestamp_;

  // The timestamp of an audio frame that would be appended directly after the
  // very last audio frame at the back of the `stream_fifo_`.
  base::TimeTicks end_timestamp_;
};

}  // namespace recording

#endif  // CHROMEOS_ASH_SERVICES_RECORDING_AUDIO_STREAM_H_
