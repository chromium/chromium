// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/mixer_loopback_connection.h"

#include <utility>

#include "base/logging.h"
#include "chromecast/media/audio/mixer_service/conversions.h"
#include "chromecast/media/audio/mixer_service/mixer_service.pb.h"
#include "chromecast/net/io_buffer_pool.h"

namespace chromecast {
namespace media {

MixerLoopbackConnection::MixerLoopbackConnection(
    std::unique_ptr<mixer_service::MixerSocket> socket)
    : socket_(std::move(socket)) {
  DCHECK(socket_);
  socket_->SetDelegate(this);
}

MixerLoopbackConnection::~MixerLoopbackConnection() = default;

void MixerLoopbackConnection::SetErrorCallback(base::OnceClosure callback) {
  error_callback_ = std::move(callback);
}

void MixerLoopbackConnection::SetStreamConfig(SampleFormat sample_format,
                                              int sample_rate,
                                              int num_channels,
                                              int data_size) {
  mixer_service::Generic message;
  mixer_service::StreamConfig* config = message.mutable_stream_config();
  config->set_sample_format(mixer_service::ConvertSampleFormat(sample_format));
  config->set_sample_rate(sample_rate);
  config->set_num_channels(num_channels);
  config->set_data_size(data_size);
  socket_->SendProto(message);

  sent_stream_config_ = true;
}

void MixerLoopbackConnection::SendAudio(
    scoped_refptr<net::IOBuffer> audio_buffer,
    int data_size_bytes,
    int64_t timestamp) {
  DCHECK(sent_stream_config_);
  socket_->SendAudioBuffer(std::move(audio_buffer), data_size_bytes, timestamp);
}

bool MixerLoopbackConnection::HandleMetadata(
    const mixer_service::Generic& message) {
  return true;
}

bool MixerLoopbackConnection::HandleAudioData(char* data,
                                              int size,
                                              int64_t timestamp) {
  return true;
}

void MixerLoopbackConnection::OnConnectionError() {
  if (error_callback_) {
    std::move(error_callback_).Run();
  }
}

}  // namespace media
}  // namespace chromecast
