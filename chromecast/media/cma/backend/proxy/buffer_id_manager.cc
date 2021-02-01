// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/buffer_id_manager.h"

#include <cmath>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_device_params.h"

namespace chromecast {
namespace media {
namespace {

// Mask to apply to a uint64_t to get a BufferId.
constexpr uint64_t kBufferIdMask = (uint64_t{1} << 63) - 1;

}  // namespace

BufferIdManager::BufferIdQueue::BufferIdQueue() {
  next_id_ = base::RandUint64();
}

BufferIdManager::BufferId BufferIdManager::BufferIdQueue::Front() {
  return static_cast<int64_t>(next_id_ & kBufferIdMask);
}

BufferIdManager::BufferId BufferIdManager::BufferIdQueue::Pop() {
  BufferIdManager::BufferId id = Front();
  next_id_++;
  return id;
}

BufferIdManager::BufferInfo::BufferInfo(
    BufferId buffer_id,
    double buffer_playback_time_in_microseconds)
    : id(buffer_id),
      playback_time_in_microseconds(buffer_playback_time_in_microseconds) {}

BufferIdManager::BufferIdManager(CmaBackend::AudioDecoder* audio_decoder)
    : audio_decoder_(audio_decoder), weak_factory_(this) {
  DCHECK(audio_decoder_);
}

BufferIdManager::~BufferIdManager() = default;

void BufferIdManager::SetAudioConfig(const AudioConfig& config) {
  bytes_per_microsecond_ = config.bytes_per_channel * config.channel_number *
                           (static_cast<double>(config.samples_per_second) /
                            base::Time::kMicrosecondsPerSecond);
}

BufferIdManager::BufferId BufferIdManager::AssignBufferId(
    size_t buffer_size_in_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(bytes_per_microsecond_);

  PruneBufferInfos();

  const double additional_playback_time =
      static_cast<double>(buffer_size_in_bytes) / bytes_per_microsecond_;
  buffer_infos_.emplace(buffer_id_queue_.Front(), additional_playback_time);
  pending_playback_time_in_microseconds_ += additional_playback_time;
  return buffer_id_queue_.Pop();
}

BufferIdManager::BufferId BufferIdManager::GetCurrentlyProcessingBuffer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  PruneBufferInfos();
  if (buffer_infos_.empty()) {
    return buffer_id_queue_.Front() == 0 ? 0 : buffer_id_queue_.Front() - 1;
  }

  return buffer_infos_.front().id;
}

void BufferIdManager::PruneBufferInfos() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!isnan(pending_playback_time_in_microseconds_));

  const auto rendering_delay = audio_decoder_->GetRenderingDelay();

  // Special case meaning that the Rendering Delay is unavailable.
  if (rendering_delay.timestamp_microseconds ==
      std::numeric_limits<int64_t>::min()) {
    return;
  }

  const double delay_in_microseconds = rendering_delay.delay_microseconds;
  DCHECK_GE(delay_in_microseconds, 0);
  while (delay_in_microseconds < pending_playback_time_in_microseconds_ &&
         !buffer_infos_.empty()) {
    const BufferInfo& oldest_buffer = buffer_infos_.front();
    DCHECK_NE(oldest_buffer.id, -1);
    DCHECK_NE(oldest_buffer.playback_time_in_microseconds, -1);
    pending_playback_time_in_microseconds_ -=
        oldest_buffer.playback_time_in_microseconds;
    buffer_infos_.pop();
  }
}

}  // namespace media
}  // namespace chromecast
