// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/recording/audio_stream.h"

#include <algorithm>
#include <memory>
#include <string_view>

#include "base/check.h"
#include "base/time/time.h"
#include "chromeos/ash/services/recording/audio_capture_util.h"
#include "chromeos/ash/services/recording/recording_service_constants.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"

namespace recording {

AudioStream::AudioStream(std::string_view name) : name_(name) {}

AudioStream::~AudioStream() = default;

void AudioStream::AppendAudioBus(std::unique_ptr<media::AudioBus> audio_bus,
                                 base::TimeTicks timestamp) {
  if (timestamp < end_timestamp_) {
    // Ensure that the timestamps are always monotonically increasing. Sometimes
    // a new bus is sent with a timestamp that's earlier than those of the ones
    // received before it. See http://b/277656502.
    timestamp = end_timestamp_;
  }

  if (empty()) {
    // If the stream is empty; i.e. this is the very first bus to ever be
    // appended, or all previously appended buses had already been consumed, we
    // can treat this new bus as the very first ever bus. there's no need to
    // append a zero-filled bus to cover the gap between the last bus that was
    // consumed and this newly-received `audio_bus`.
    begin_timestamp_ = timestamp;
    end_timestamp_ = timestamp;
    AppendAudioBusInternal(std::move(audio_bus), timestamp);
    return;
  }

  // In case the newly added bus has a timestamp that is not directly after the
  // last bus at the end of the stream, we append a zero-filled bus that lasts
  // for the duration of the gap. This allows us to treat the stream as a
  // contiguous bus when consuming it.
  const auto diff = timestamp - end_timestamp_;
  CHECK(!diff.is_negative());
  if (const auto gap_frames =
          audio_capture_util::NumberOfAudioFramesInDuration(diff);
      gap_frames > 0) {
    AppendSilence(gap_frames);
  }

  AppendAudioBusInternal(std::move(audio_bus), end_timestamp_);
}

void AudioStream::ConsumeAndAccumulateTo(media::AudioBus* destination,
                                         int destination_start_frame,
                                         int frames_to_consume) {
  CHECK_GT(frames_to_consume, 0);
  CHECK_LE(frames_to_consume, total_frames_);

  begin_timestamp_ += media::AudioTimestampHelper::FramesToTime(
      frames_to_consume, audio_capture_util::kAudioSampleRate);

  int remaining_frames_to_consume = frames_to_consume;
  while (remaining_frames_to_consume > 0) {
    // Do not pop the front bus yet, since it may get partially consumed, and in
    // that case, we will replace it with another bus that contains the leftover
    // frames. Only when it's fully consumed is when we pop it from the queue.
    auto& front = stream_fifo_.front();

    const int consumed = std::min(front->frames(), remaining_frames_to_consume);
    audio_capture_util::AccumulateBusTo(
        /*source=*/*front, /*destination=*/destination,
        /*source_start_frame=*/0,
        /*destination_start_frame=*/destination_start_frame,
        /*length=*/consumed);
    remaining_frames_to_consume -= consumed;
    destination_start_frame += consumed;

    const int left_over = front->frames() - consumed;
    if (left_over == 0) {
      // Bus was fully consumed, we can pop it out from the queue.
      stream_fifo_.pop();
    } else {
      // Replace the bus at the front with one that contains the leftover
      // frames.
      auto left_over_bus =
          media::AudioBus::Create(front->channels(), left_over);
      front->CopyPartialFramesTo(/*source_start_frame=*/consumed,
                                 /*frame_count=*/left_over,
                                 /*dest_start_frame=*/0,
                                 /*dest=*/left_over_bus.get());
      front = std::move(left_over_bus);
    }
  }

  total_frames_ -= frames_to_consume;

  UpdateEndTimestamp();
}

void AudioStream::AppendSilence(int64_t frames) {
  DCHECK_GT(frames, 0);
  AppendAudioBusInternal(
      audio_capture_util::CreateStereoZeroInitializedAudioBusForFrames(frames),
      end_timestamp_);
}

void AudioStream::AppendAudioBusInternal(
    std::unique_ptr<media::AudioBus> audio_bus,
    base::TimeTicks timestamp) {
  CHECK_EQ(end_timestamp_, timestamp);

  const int bus_frames = audio_bus->frames();
  stream_fifo_.push(std::move(audio_bus));
  total_frames_ += bus_frames;

  UpdateEndTimestamp();
}

void AudioStream::UpdateEndTimestamp() {
  end_timestamp_ = begin_timestamp_ +
                   media::AudioTimestampHelper::FramesToTime(
                       total_frames_, audio_capture_util::kAudioSampleRate);
}

}  // namespace recording
