// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/control_connection.h"

#include <utility>

#include "base/logging.h"
#include "chromecast/media/audio/mixer_service/conversions.h"
#include "chromecast/media/audio/mixer_service/mixer_service.pb.h"
#include "net/socket/stream_socket.h"

namespace chromecast {
namespace media {
namespace mixer_service {

ControlConnection::ControlConnection() = default;

ControlConnection::~ControlConnection() = default;

void ControlConnection::Connect(ConnectedCallback callback) {
  connect_callback_ = std::move(callback);
  MixerConnection::Connect();
}

void ControlConnection::SetVolume(AudioContentType type,
                                  float volume_multiplier) {
  if (type == AudioContentType::kOther) {
    return;
  }

  volume_[type] = volume_multiplier;
  if (socket_) {
    Generic message;
    auto* volume = message.mutable_set_device_volume();
    volume->set_content_type(ConvertContentType(type));
    volume->set_volume_multiplier(volume_multiplier);
    socket_->SendProto(message);
  }
}

void ControlConnection::SetMuted(AudioContentType type, bool muted) {
  if (type == AudioContentType::kOther) {
    return;
  }

  muted_[type] = muted;
  if (socket_) {
    Generic message;
    auto* muted = message.mutable_set_device_muted();
    muted->set_content_type(ConvertContentType(type));
    muted->set_muted(muted);
    socket_->SendProto(message);
  }
}

void ControlConnection::SetVolumeLimit(AudioContentType type,
                                       float max_volume_multiplier) {
  if (type == AudioContentType::kOther) {
    return;
  }

  volume_limit_[type] = max_volume_multiplier;
  if (socket_) {
    Generic message;
    auto* limit = message.mutable_set_volume_limit();
    limit->set_content_type(ConvertContentType(type));
    limit->set_max_volume_multiplier(max_volume_multiplier);
    socket_->SendProto(message);
  }
}

void ControlConnection::ConfigurePostprocessor(std::string postprocessor_name,
                                               std::string config) {
  SendPostprocessorMessage(postprocessor_name, config);
  postprocessor_config_.insert_or_assign(std::move(postprocessor_name),
                                         std::move(config));
}

void ControlConnection::SendPostprocessorMessage(std::string postprocessor_name,
                                                 std::string message) {
  if (!socket_) {
    return;
  }

  // Erase any ? and subsequent substring from the name.
  auto q = postprocessor_name.find('?');
  if (q != std::string::npos) {
    postprocessor_name.erase(q);
  }

  Generic proto;
  auto* content = proto.mutable_configure_postprocessor();
  content->set_name(std::move(postprocessor_name));
  content->set_config(std::move(message));
  socket_->SendProto(proto);
}

void ControlConnection::ReloadPostprocessors() {
  if (!socket_) {
    return;
  }
  Generic message;
  message.mutable_reload_postprocessors();
  socket_->SendProto(message);
}

void ControlConnection::SetStreamCountCallback(StreamCountCallback callback) {
  stream_count_callback_ = std::move(callback);
  if (socket_) {
    Generic message;
    message.mutable_request_stream_count()->set_subscribe(!callback.is_null());
    socket_->SendProto(message);
  }
}

void ControlConnection::SetNumOutputChannels(int num_channels) {
  num_output_channels_ = num_channels;
  if (socket_) {
    Generic message;
    message.mutable_set_num_output_channels()->set_channels(num_channels);
    socket_->SendProto(message);
  }
}

void ControlConnection::OnConnected(std::unique_ptr<MixerSocket> socket) {
  socket_ = std::move(socket);
  socket_->SetDelegate(this);

  for (const auto& item : volume_limit_) {
    Generic message;
    auto* limit = message.mutable_set_volume_limit();
    limit->set_content_type(ConvertContentType(item.first));
    limit->set_max_volume_multiplier(item.second);
    socket_->SendProto(message);
  }

  for (const auto& item : muted_) {
    Generic message;
    auto* muted = message.mutable_set_device_muted();
    muted->set_content_type(ConvertContentType(item.first));
    muted->set_muted(item.second);
    socket_->SendProto(message);
  }

  for (const auto& item : volume_) {
    Generic message;
    auto* volume = message.mutable_set_device_volume();
    volume->set_content_type(ConvertContentType(item.first));
    volume->set_volume_multiplier(item.second);
    socket_->SendProto(message);
  }

  if (stream_count_callback_) {
    Generic message;
    message.mutable_request_stream_count()->set_subscribe(true);
    socket_->SendProto(message);
  }

  if (num_output_channels_) {
    Generic message;
    message.mutable_set_num_output_channels()->set_channels(
        num_output_channels_);
    socket_->SendProto(message);
  }

  for (const auto& item : postprocessor_config_) {
    SendPostprocessorMessage(item.first, item.second);
  }

  if (connect_callback_) {
    connect_callback_.Run();
  }
}

void ControlConnection::OnConnectionError() {
  socket_.reset();
  MixerConnection::Connect();
}

bool ControlConnection::HandleMetadata(const Generic& message) {
  if (stream_count_callback_ && message.has_stream_count()) {
    stream_count_callback_.Run(message.stream_count().primary(),
                               message.stream_count().sfx());
  }
  return true;
}

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast
