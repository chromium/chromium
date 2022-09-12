// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/output_stream_connection.h"

#include <limits>
#include <utility>

#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "chromecast/media/audio/mixer_service/mixer_service_transport.pb.h"
#include "chromecast/media/audio/net/common.pb.h"
#include "chromecast/media/audio/net/conversions.h"
#include "chromecast/metrics/metrics_recorder.h"
#include "chromecast/net/io_buffer_pool.h"

namespace chromecast {
namespace media {
namespace mixer_service {

namespace {

int GetFrameSize(const OutputStreamParams& params) {
  return audio_service::GetSampleSizeBytes(params.sample_format()) *
         params.num_channels();
}

int GetFillSizeFrames(const OutputStreamParams& params) {
  if (params.has_fill_size_frames()) {
    return params.fill_size_frames();
  }
  // Use 10 ms by default.
  return params.sample_rate() / 100;
}

enum MessageTypes : int {
  kInitial = 1,
  kStartTimestamp,
  kPlaybackRate,
  kAudioClockRate,
  kStreamVolume,
  kPauseResume,
  kEndOfStream,
  kTimestampAdjustment,
};

}  // namespace

OutputStreamConnection::OutputStreamConnection(Delegate* delegate,
                                               const OutputStreamParams& params)
    : delegate_(delegate),
      params_(std::make_unique<OutputStreamParams>(params)),
      frame_size_(GetFrameSize(params)),
      fill_size_frames_(GetFillSizeFrames(params)),
      buffer_pool_(base::MakeRefCounted<IOBufferPool>(
          MixerSocket::kAudioMessageHeaderSize +
              fill_size_frames_ * frame_size_,
          std::numeric_limits<size_t>::max(),
          true /* threadsafe */)),
      audio_buffer_(buffer_pool_->GetBuffer()) {
  DCHECK(delegate_);
  DCHECK_GT(params_->sample_rate(), 0);
  DCHECK_GT(params_->num_channels(), 0);
  params_->set_fill_size_frames(fill_size_frames_);
}

OutputStreamConnection::~OutputStreamConnection() = default;

void OutputStreamConnection::Connect() {
  MixerConnection::Connect();
}

void OutputStreamConnection::SendNextBuffer(int filled_frames, int64_t pts) {
  SendAudioBuffer(std::move(audio_buffer_), filled_frames, pts);
  audio_buffer_ = buffer_pool_->GetBuffer();
}

void OutputStreamConnection::SendAudioBuffer(
    scoped_refptr<net::IOBuffer> audio_buffer,
    int filled_frames,
    int64_t pts) {
  if (!socket_) {
    LOG(INFO) << "Tried to send buffer without a connection";
    return;
  }
  if (sent_eos_) {
    // Don't send any more data after the EOS buffer.
    return;
  }

  if (filled_frames == 0) {
    // Send explicit end-of-stream message.
    sent_eos_ = true;
    Generic message;
    message.mutable_eos_played_out();
    socket_->SendProto(kEndOfStream, message);
    return;
  }
  if (socket_->SendAudioBuffer(std::move(audio_buffer),
                               filled_frames * frame_size_, pts)) {
    LOG_IF(INFO, dropping_audio_) << "Stopped dropping audio";
    dropping_audio_ = false;
  } else {
    LOG_IF(WARNING, !dropping_audio_) << "Dropping audio";
    dropping_audio_ = true;
  }
}

void OutputStreamConnection::SetVolumeMultiplier(float multiplier) {
  volume_multiplier_ = multiplier;
  if (socket_) {
    Generic message;
    message.mutable_set_stream_volume()->set_volume(multiplier);
    socket_->SendProto(kStreamVolume, message);
  }
}

void OutputStreamConnection::SetStartTimestamp(int64_t start_timestamp,
                                               int64_t pts) {
  start_timestamp_ = start_timestamp;
  start_pts_ = pts;

  if (socket_) {
    Generic message;
    message.mutable_set_start_timestamp()->set_start_timestamp(start_timestamp);
    message.mutable_set_start_timestamp()->set_start_pts(pts);
    socket_->SendProto(kStartTimestamp, message);
  }
}

void OutputStreamConnection::SetPlaybackRate(float playback_rate) {
  playback_rate_ = playback_rate;
  if (socket_) {
    Generic message;
    message.mutable_set_playback_rate()->set_playback_rate(playback_rate);
    socket_->SendProto(kPlaybackRate, message);
  }
}

void OutputStreamConnection::SetAudioClockRate(double rate) {
  audio_clock_rate_ = rate;
  if (socket_) {
    Generic message;
    message.mutable_set_audio_clock_rate()->set_rate(rate);
    socket_->SendProto(kAudioClockRate, message);
  }
}

void OutputStreamConnection::Pause() {
  paused_ = true;
  if (socket_) {
    Generic message;
    message.mutable_set_paused()->set_paused(true);
    socket_->SendProto(kPauseResume, message);
  }
}

void OutputStreamConnection::Resume() {
  paused_ = false;
  if (socket_) {
    Generic message;
    auto* pause_message = message.mutable_set_paused();
    pause_message->set_paused(false);
    socket_->SendProto(kPauseResume, message);
  }
}

void OutputStreamConnection::SendTimestampAdjustment(
    int64_t timestamp_adjustment) {
  if (socket_) {
    Generic message;
    auto* adjustment_message = message.mutable_timestamp_adjustment();
    adjustment_message->set_adjustment(timestamp_adjustment);
    socket_->SendProto(kTimestampAdjustment, message);
  }
}

void OutputStreamConnection::OnConnected(std::unique_ptr<MixerSocket> socket) {
  socket_ = std::move(socket);
  socket_->SetDelegate(this);

  Generic message;
  *(message.mutable_output_stream_params()) = *params_;
  if (start_timestamp_ != INT64_MIN) {
    message.mutable_set_start_timestamp()->set_start_timestamp(
        start_timestamp_);
    message.mutable_set_start_timestamp()->set_start_pts(start_pts_);
  }
  if (playback_rate_ != 1.0f) {
    message.mutable_set_playback_rate()->set_playback_rate(playback_rate_);
  }
  if (audio_clock_rate_ != 1.0) {
    message.mutable_set_audio_clock_rate()->set_rate(audio_clock_rate_);
  }
  if (volume_multiplier_ != 1.0f) {
    message.mutable_set_stream_volume()->set_volume(volume_multiplier_);
  }
  if (paused_) {
    message.mutable_set_paused()->set_paused(true);
  }
  socket_->SendProto(kInitial, message);
  delegate_->FillNextBuffer(
      audio_buffer_->data() + MixerSocket::kAudioMessageHeaderSize,
      fill_size_frames_, std::numeric_limits<int64_t>::min(), 0);
}

void OutputStreamConnection::OnConnectionError() {
  socket_.reset();
  if (sent_eos_) {
    delegate_->OnEosPlayed();
    return;
  }
  MixerConnection::Connect();
}

bool OutputStreamConnection::HandleMetadata(const Generic& message) {
  if (message.has_eos_played_out()) {
    delegate_->OnEosPlayed();
    return true;
  }

  if (message.has_push_result() && !sent_eos_) {
    delegate_->FillNextBuffer(
        audio_buffer_->data() + MixerSocket::kAudioMessageHeaderSize,
        fill_size_frames_, message.push_result().delay_timestamp(),
        message.push_result().delay());
  }

  if (message.has_ready_for_playback()) {
    delegate_->OnAudioReadyForPlayback(
        message.ready_for_playback().delay_microseconds());
  }

  if (message.has_error()) {
    delegate_->OnMixerError();
  }

  if (message.has_mixer_underrun()) {
    std::string metric_name =
        (message.mixer_underrun().type() == MixerUnderrun::INPUT_UNDERRUN
             ? "Platform.Audio.Mixer.StreamUnderrun"
             : "Platform.Audio.Mixer.OutputUnderrun");
    std::unique_ptr<CastEventBuilder> event = CreateCastEvent(metric_name);
    delegate_->ProcessCastEvent(event.get());
    RecordCastEvent(metric_name, std::move(event),
                    /* verbose_log_level = */ 0);
    delegate_->OnMixerUnderrun(static_cast<Delegate::MixerUnderrunType>(
        message.mixer_underrun().type()));
  }
  return true;
}

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast
