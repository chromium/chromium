// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/redirected_audio_connection.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "chromecast/media/audio/mixer_service/conversions.h"
#include "chromecast/media/audio/mixer_service/mixer_service.pb.h"
#include "chromecast/net/io_buffer_pool.h"

namespace chromecast {
namespace media {
namespace mixer_service {

namespace {

void FillPatterns(
    const RedirectedAudioConnection::StreamMatchPatterns& patterns,
    Generic* message) {
  RedirectedStreamPatterns* patterns_proto =
      message->mutable_redirected_stream_patterns();
  for (const auto& p : patterns) {
    auto* pattern = patterns_proto->add_patterns();
    pattern->set_content_type(ConvertContentType(p.first));
    pattern->set_device_id_pattern(p.second);
  }
}

}  // namespace

RedirectedAudioConnection::RedirectedAudioConnection(const Config& config,
                                                     Delegate* delegate)
    : config_(config), delegate_(delegate) {
  DCHECK(delegate_);
}

RedirectedAudioConnection::~RedirectedAudioConnection() = default;

void RedirectedAudioConnection::SetStreamMatchPatterns(
    StreamMatchPatterns patterns) {
  stream_match_patterns_ = std::move(patterns);
  if (socket_) {
    Generic message;
    FillPatterns(stream_match_patterns_, &message);
    socket_->SendProto(message);
  }
}

void RedirectedAudioConnection::Connect() {
  MixerConnection::Connect();
}

void RedirectedAudioConnection::OnConnected(
    std::unique_ptr<MixerSocket> socket) {
  sample_rate_ = 0;

  socket_ = std::move(socket);
  socket_->SetDelegate(this);

  Generic message;
  RedirectionRequest* request = message.mutable_redirection_request();
  request->set_order(config_.order);
  request->set_num_channels(config_.num_output_channels);
  request->set_apply_volume(config_.apply_volume);
  request->set_extra_delay_microseconds(config_.extra_delay_microseconds);

  if (!stream_match_patterns_.empty()) {
    FillPatterns(stream_match_patterns_, &message);
  }
  socket_->SendProto(message);
}

void RedirectedAudioConnection::OnConnectionError() {
  socket_.reset();
  MixerConnection::Connect();
}

bool RedirectedAudioConnection::HandleMetadata(const Generic& message) {
  if (message.has_stream_config()) {
    DCHECK_EQ(message.stream_config().sample_format(), SAMPLE_FORMAT_FLOAT_P);
    sample_rate_ = message.stream_config().sample_rate();
    DCHECK_EQ(message.stream_config().num_channels(),
              config_.num_output_channels);
  }
  return true;
}

bool RedirectedAudioConnection::HandleAudioData(char* data,
                                                int size,
                                                int64_t timestamp) {
  if (sample_rate_ != 0) {
    int frames = size / (sizeof(float) * config_.num_output_channels);
    delegate_->OnRedirectedAudio(timestamp, sample_rate_,
                                 reinterpret_cast<float*>(data), frames);
  }
  return true;
}

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast
