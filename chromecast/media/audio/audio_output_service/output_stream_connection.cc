// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/audio_output_service/output_stream_connection.h"

#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "net/base/io_buffer.h"

namespace chromecast {
namespace media {
namespace audio_output_service {

namespace {

constexpr base::TimeDelta kHeartbeatTimeout = base::Seconds(2);

enum MessageTypes : int {
  kInitial = 1,
  kStartTimestamp,
  kPlaybackRate,
  kStreamVolume,
  kEndOfStream,
  kUpdateAudioConfig,
  kStop,
  kHeartbeat,
};

}  // namespace

OutputStreamConnection::OutputStreamConnection(
    Delegate* delegate,
    CmaBackendParams params,
    mojo::PendingRemote<mojom::AudioSocketBroker> pending_socket_broker)
    : OutputConnection(std::move(pending_socket_broker)),
      delegate_(delegate),
      params_(std::move(params)) {
  DCHECK(delegate_);
}

OutputStreamConnection::~OutputStreamConnection() = default;

void OutputStreamConnection::SendAudioBuffer(
    scoped_refptr<net::IOBuffer> audio_buffer,
    int buffer_size_bytes,
    int64_t pts) {
  if (!socket_) {
    LOG(INFO) << "Tried to send buffer without a connection.";
    return;
  }
  if (sent_eos_) {
    // Should not send any more data after the EOS buffer.
    return;
  }

  if (buffer_size_bytes == 0) {
    sent_eos_ = true;
    Generic message;
    message.mutable_eos_played_out();
    socket_->SendProto(kEndOfStream, message);
    return;
  }
  if (socket_->SendAudioBuffer(std::move(audio_buffer), buffer_size_bytes,
                               pts)) {
    LOG_IF(INFO, dropping_audio_) << "Stopped dropping audio.";
    dropping_audio_ = false;
  } else {
    LOG_IF(WARNING, !dropping_audio_) << "Dropping audio.";
    dropping_audio_ = true;
  }
  heartbeat_timer_.Reset();
}

void OutputStreamConnection::StartPlayingFrom(int64_t start_pts) {
  if (!socket_) {
    return;
  }
  Generic message;
  message.mutable_set_start_timestamp()->set_start_pts(start_pts);
  socket_->SendProto(kStartTimestamp, message);
  heartbeat_timer_.Reset();
}

void OutputStreamConnection::StopPlayback() {
  if (!socket_) {
    return;
  }
  Generic message;
  message.mutable_stop_playback();
  socket_->SendProto(kStop, message);
  heartbeat_timer_.Reset();
}

void OutputStreamConnection::SetPlaybackRate(float playback_rate) {
  playback_rate_ = playback_rate;
  if (!socket_) {
    return;
  }
  Generic message;
  message.mutable_set_playback_rate()->set_playback_rate(playback_rate_);
  socket_->SendProto(kPlaybackRate, message);
  heartbeat_timer_.Reset();
}

void OutputStreamConnection::SetVolume(float volume) {
  volume_ = volume;
  if (!socket_) {
    return;
  }
  Generic message;
  message.mutable_set_stream_volume()->set_volume(volume_);
  socket_->SendProto(kStreamVolume, message);
  heartbeat_timer_.Reset();
}

void OutputStreamConnection::UpdateAudioConfig(const CmaBackendParams& params) {
  params_.MergeFrom(params);
  if (!socket_) {
    return;
  }
  Generic message;
  *(message.mutable_backend_params()) = params_;
  socket_->SendProto(kUpdateAudioConfig, message);
  heartbeat_timer_.Reset();
}

void OutputStreamConnection::Connect() {
  if (socket_) {
    // Don't reconnect if the connection is already established.
    return;
  }
  OutputConnection::Connect();
}

void OutputStreamConnection::OnConnected(std::unique_ptr<OutputSocket> socket) {
  DCHECK(socket);

  socket_ = std::move(socket);
  socket_->SetDelegate(this);

  Generic message;
  *(message.mutable_backend_params()) = params_;
  if (playback_rate_ != 1.0f) {
    message.mutable_set_playback_rate()->set_playback_rate(playback_rate_);
  }
  if (volume_ != 1.0f) {
    message.mutable_set_stream_volume()->set_volume(volume_);
  }
  socket_->SendProto(kInitial, message);
  heartbeat_timer_.Start(FROM_HERE, kHeartbeatTimeout, this,
                         &OutputStreamConnection::SendHeartbeat);
}

void OutputStreamConnection::OnConnectionFailed() {
  BackendInitializationStatus status;
  status.set_status(BackendInitializationStatus::ERROR);
  delegate_->OnBackendInitialized(status);
}

void OutputStreamConnection::OnConnectionError() {
  heartbeat_timer_.Stop();
  socket_.reset();
  OutputConnection::Connect();
}

void OutputStreamConnection::SendHeartbeat() {
  if (!socket_) {
    LOG(ERROR) << "No active connection. Skip heartbeat.";
    return;
  }

  Generic message;
  message.mutable_heartbeat();
  socket_->SendProto(kHeartbeat, message);
  heartbeat_timer_.Start(FROM_HERE, kHeartbeatTimeout, this,
                         &OutputStreamConnection::SendHeartbeat);
}

bool OutputStreamConnection::HandleMetadata(const Generic& message) {
  if (message.has_backend_initialization_status()) {
    delegate_->OnBackendInitialized(message.backend_initialization_status());
  }

  if (message.has_current_media_timestamp()) {
    delegate_->OnNextBuffer(
        message.current_media_timestamp().media_timestamp_microseconds(),
        message.current_media_timestamp().reference_timestamp_microseconds(),
        message.current_media_timestamp().delay_microseconds(),
        message.current_media_timestamp().delay_timestamp_microseconds());
  }
  return true;
}

}  // namespace audio_output_service
}  // namespace media
}  // namespace chromecast
