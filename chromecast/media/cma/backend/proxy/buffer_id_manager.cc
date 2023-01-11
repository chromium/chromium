// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/buffer_id_manager.h"

#include <cmath>
#include <limits>

#include "base/functional/callback.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "chromecast/media/api/monotonic_clock.h"
#include "chromecast/media/cma/backend/proxy/cma_proxy_handler.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_device_params.h"

namespace chromecast {
namespace media {
namespace {

// Mask to apply to a uint64_t to get a BufferId.
constexpr uint64_t kBufferIdMask = (uint64_t{1} << 63) - 1;

// A "large" time difference is a difference in audio synchronization which may
// be perceived by the human  ear.
constexpr int64_t kLargeTimeDifference =
    base::Time::kMicrosecondsPerMillisecond;

// Calculates whether the difference between two microsecond-based timestamps is
// large.
bool IsLargeTimeDifference(int64_t timestamp1, int64_t timestamp2) {
  return std::abs(timestamp1 - timestamp2) >= kLargeTimeDifference;
}

}  // namespace

BufferIdManager::BufferIdQueue::BufferIdQueue() {
  next_id_ = 0;
}

BufferIdManager::BufferId BufferIdManager::BufferIdQueue::Front() const {
  return static_cast<int64_t>(next_id_ & kBufferIdMask);
}

BufferIdManager::BufferId BufferIdManager::BufferIdQueue::Pop() {
  BufferIdManager::BufferId id = Front();
  next_id_++;
  return id;
}

BufferIdManager::BufferInfo::BufferInfo(BufferId buffer_id, int64_t pts)
    : id(buffer_id), pts_timestamp_micros(pts) {}

BufferIdManager::Client::~Client() = default;

BufferIdManager::BufferIdManager(CmaBackend::AudioDecoder* audio_decoder,
                                 Client* client,
                                 std::unique_ptr<MonotonicClock> clock)
    : clock_(std::move(clock)),
      client_(client),
      audio_decoder_(audio_decoder),
      weak_factory_(this) {
  DCHECK(clock_);
  DCHECK(client_);
  DCHECK(audio_decoder_);
}

BufferIdManager::BufferIdManager(CmaBackend::AudioDecoder* audio_decoder,
                                 Client* client)
    : BufferIdManager(audio_decoder, client, MonotonicClock::Create()) {}

BufferIdManager::~BufferIdManager() = default;

BufferIdManager::TargetBufferInfo
BufferIdManager::GetCurrentlyProcessingBufferInfo() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If no data has finished playing, just return the next ID to play.
  if (!most_recently_played_buffer_.has_value()) {
    BufferId id = buffer_infos_.empty() ? buffer_id_queue_.Front()
                                        : buffer_infos_.front().id;
    return TargetBufferInfo{id, clock_->Now()};
  }

  // Else, return the most recently played buffer.
  return TargetBufferInfo{
      most_recently_played_buffer_.value().id,
      most_recently_played_buffer_.value().expected_playout_timestamp_micros};
}

BufferIdManager::BufferId BufferIdManager::AssignBufferId(
    const CastDecoderBuffer& buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UpdateAndGetCurrentlyProcessingBufferInfo();
  int64_t additional_playback_time = 0;
  if (!buffer_infos_.empty()) {
    additional_playback_time =
        buffer.timestamp() - buffer_infos_.back().pts_timestamp_micros;
  } else if (most_recently_played_buffer_.has_value()) {
    additional_playback_time =
        buffer.timestamp() -
        most_recently_played_buffer_.value().pts_timestamp_micros;
  }

  buffer_infos_.emplace(buffer_id_queue_.Front(), buffer.timestamp());
  pending_playback_time_in_microseconds_ += additional_playback_time;
  return buffer_id_queue_.Pop();
}

BufferIdManager::TargetBufferInfo
BufferIdManager::UpdateAndGetCurrentlyProcessingBufferInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto rendering_delay = audio_decoder_->GetRenderingDelay();

  // Special case meaning that the Rendering Delay is unavailable.
  if (rendering_delay.timestamp_microseconds ==
      std::numeric_limits<int64_t>::min()) {
    return GetCurrentlyProcessingBufferInfo();
  }

  // Pop buffer infos from the queue until only not-yet-played-out frames
  // remain.
  const int64_t delay_timestamp = rendering_delay.timestamp_microseconds;
  const int64_t delay_in_microseconds = rendering_delay.delay_microseconds;
  DCHECK_GE(delay_in_microseconds, 0);

  // When no buffers have played out yet, store the currently playing buffer's
  // info.
  if (!most_recently_played_buffer_.has_value() && !buffer_infos_.empty()) {
    // Use a large expected playout time to guarantee an OnTimestampUpdateNeeded
    // call occurs.
    most_recently_played_buffer_ = BufferPlayoutInfo{
        buffer_infos_.front().id, buffer_infos_.front().pts_timestamp_micros,
        std::numeric_limits<int64_t>::max()};
  }

  for (; delay_in_microseconds <= pending_playback_time_in_microseconds_ &&
         !buffer_infos_.empty();
       buffer_infos_.pop()) {
    const BufferInfo& queue_front = buffer_infos_.front();
    DCHECK_GE(queue_front.id, 0);
    DCHECK_GE(queue_front.pts_timestamp_micros, 0);

    // Update |pending_playback_time_in_microseconds_|.
    BufferPlayoutInfo& most_recently_played_buffer =
        most_recently_played_buffer_.value();
    DCHECK_GE(most_recently_played_buffer.id, 0);
    DCHECK_GE(most_recently_played_buffer.pts_timestamp_micros, 0);
    DCHECK_GE(most_recently_played_buffer.expected_playout_timestamp_micros, 0);

    int64_t change_in_playback_time =
        queue_front.pts_timestamp_micros -
        most_recently_played_buffer.pts_timestamp_micros;
    DCHECK_GE(change_in_playback_time, 0);
    pending_playback_time_in_microseconds_ -= change_in_playback_time;

    // Update |most_recently_played_buffer_|.
    most_recently_played_buffer.id = queue_front.id;
    most_recently_played_buffer.pts_timestamp_micros =
        queue_front.pts_timestamp_micros;
    most_recently_played_buffer.expected_playout_timestamp_micros +=
        change_in_playback_time;
  }

  // Check if the currently playing buffer has a playback time different from
  // what is expected, and update the client if the difference is too large.
  const int64_t last_playout_timestamp = delay_timestamp +
                                         delay_in_microseconds -
                                         pending_playback_time_in_microseconds_;
  if (most_recently_played_buffer_.has_value() &&
      IsLargeTimeDifference(most_recently_played_buffer_.value()
                                .expected_playout_timestamp_micros,
                            last_playout_timestamp)) {
    most_recently_played_buffer_.value().expected_playout_timestamp_micros =
        last_playout_timestamp;
    client_->OnTimestampUpdateNeeded(
        {most_recently_played_buffer_.value().id, last_playout_timestamp});
  }

  return GetCurrentlyProcessingBufferInfo();
}

}  // namespace media
}  // namespace chromecast
