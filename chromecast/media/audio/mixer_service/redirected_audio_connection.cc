// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/redirected_audio_connection.h"

#include <algorithm>
#include <limits>

#include "base/check_op.h"
#include "chromecast/media/audio/mixer_service/mixer_service_transport.pb.h"
#include "chromecast/media/audio/net/common.pb.h"
#include "chromecast/media/audio/net/conversions.h"
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
    pattern->set_content_type(audio_service::ConvertContentType(p.first));
    pattern->set_device_id_pattern(p.second);
  }
}

enum MessageTypes : int {
  kRedirectionRequest = 1,
  kStreamMatchPatterns,
};

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
    socket_->SendProto(kStreamMatchPatterns, message);
  }
}

void RedirectedAudioConnection::Connect() {
  MixerConnection::Connect();
}

void RedirectedAudioConnection::ConnectForTest(
    std::unique_ptr<MixerSocket> connected_socket_for_test) {
  DCHECK(connected_socket_for_test);
  OnConnected(std::move(connected_socket_for_test));
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
  if (config_.output_channel_layout != media::ChannelLayout::UNSUPPORTED &&
      config_.output_channel_layout != media::ChannelLayout::BITSTREAM) {
    request->set_channel_layout(
        audio_service::ConvertChannelLayout(config_.output_channel_layout));
  }
  request->set_apply_volume(config_.apply_volume);
  request->set_extra_delay_microseconds(config_.extra_delay_microseconds);

  if (!stream_match_patterns_.empty()) {
    FillPatterns(stream_match_patterns_, &message);
  }
  socket_->SendProto(kRedirectionRequest, message);
}

void RedirectedAudioConnection::OnConnectionError() {
  socket_.reset();
  MixerConnection::Connect();
}

bool RedirectedAudioConnection::HandleMetadata(const Generic& message) {
  if (message.has_stream_config()) {
    DCHECK_EQ(message.stream_config().sample_format(),
              audio_service::SAMPLE_FORMAT_FLOAT_P);
    sample_rate_ = message.stream_config().sample_rate();
    DCHECK_EQ(message.stream_config().num_channels(),
              config_.num_output_channels);

    delegate_->SetSampleRate(sample_rate_);
  }
  return true;
}

bool RedirectedAudioConnection::HandleAudioData(char* data,
                                                size_t size,
                                                int64_t timestamp) {
  if (sample_rate_ != 0) {
    int frames = size / (sizeof(float) * config_.num_output_channels);
    delegate_->OnRedirectedAudio(timestamp, reinterpret_cast<float*>(data),
                                 frames);
  }
  return true;
}

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast
