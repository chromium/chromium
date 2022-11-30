// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/control_connection.h"

#include <utility>

#include "chromecast/media/audio/mixer_service/mixer_service_transport.pb.h"
#include "chromecast/media/audio/net/common.pb.h"
#include "chromecast/media/audio/net/conversions.h"
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
    volume->set_content_type(audio_service::ConvertContentType(type));
    volume->set_volume_multiplier(volume_multiplier);
    if (!socket_->SendProto(0, message)) {
      OnSendFailed();
    }
  }
}

void ControlConnection::SetMuted(AudioContentType type, bool muted) {
  if (type == AudioContentType::kOther) {
    return;
  }

  muted_[type] = muted;
  if (socket_) {
    Generic message;
    auto* mute_message = message.mutable_set_device_muted();
    mute_message->set_content_type(audio_service::ConvertContentType(type));
    mute_message->set_muted(muted);
    if (!socket_->SendProto(0, message)) {
      OnSendFailed();
    }
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
    limit->set_content_type(audio_service::ConvertContentType(type));
    limit->set_max_volume_multiplier(max_volume_multiplier);
    if (!socket_->SendProto(0, message)) {
      OnSendFailed();
    }
  }
}

void ControlConnection::ListPostprocessors(
    ListPostprocessorsCallback callback) {
  list_postprocessors_callbacks_.push_back(std::move(callback));
  if (!socket_) {
    return;
  }
  Generic message;
  message.mutable_list_postprocessors();
  if (!socket_->SendProto(0, message)) {
    OnSendFailed();
  }
}

void ControlConnection::ConfigurePostprocessor(std::string postprocessor_name,
                                               std::string config) {
  postprocessor_config_.insert_or_assign(postprocessor_name, config);
  if (!SendPostprocessorMessageInternal(std::move(postprocessor_name),
                                        std::move(config))) {
    OnSendFailed();
  }
}

void ControlConnection::SendPostprocessorMessage(std::string postprocessor_name,
                                                 std::string message) {
  SendPostprocessorMessageInternal(std::move(postprocessor_name),
                                   std::move(message));
}

bool ControlConnection::SendPostprocessorMessageInternal(
    std::string postprocessor_name,
    std::string message) {
  if (!socket_) {
    return true;
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
  return socket_->SendProto(0, proto);
}

void ControlConnection::ReloadPostprocessors() {
  if (!socket_) {
    return;
  }
  Generic message;
  message.mutable_reload_postprocessors();
  socket_->SendProto(0, message);
}

void ControlConnection::SetStreamCountCallback(StreamCountCallback callback) {
  stream_count_callback_ = std::move(callback);
  if (socket_) {
    Generic message;
    message.mutable_request_stream_count()->set_subscribe(
        !stream_count_callback_.is_null());
    if (!socket_->SendProto(0, message)) {
      OnSendFailed();
    }
  }
}

void ControlConnection::SetNumOutputChannels(int num_channels) {
  num_output_channels_ = num_channels;
  if (socket_) {
    Generic message;
    message.mutable_set_num_output_channels()->set_channels(num_channels);
    if (!socket_->SendProto(0, message)) {
      OnSendFailed();
    }
  }
}

void ControlConnection::OnConnected(std::unique_ptr<MixerSocket> socket) {
  socket_ = std::move(socket);
  socket_->SetDelegate(this);

  for (const auto& item : volume_limit_) {
    Generic message;
    auto* limit = message.mutable_set_volume_limit();
    limit->set_content_type(audio_service::ConvertContentType(item.first));
    limit->set_max_volume_multiplier(item.second);
    if (!socket_->SendProto(0, message)) {
      return OnSendFailed();
    }
  }

  for (const auto& item : muted_) {
    Generic message;
    auto* muted = message.mutable_set_device_muted();
    muted->set_content_type(audio_service::ConvertContentType(item.first));
    muted->set_muted(item.second);
    if (!socket_->SendProto(0, message)) {
      return OnSendFailed();
    }
  }

  for (const auto& item : volume_) {
    Generic message;
    auto* volume = message.mutable_set_device_volume();
    volume->set_content_type(audio_service::ConvertContentType(item.first));
    volume->set_volume_multiplier(item.second);
    if (!socket_->SendProto(0, message)) {
      return OnSendFailed();
    }
  }

  if (stream_count_callback_) {
    Generic message;
    message.mutable_request_stream_count()->set_subscribe(true);
    if (!socket_->SendProto(0, message)) {
      return OnSendFailed();
    }
  }

  if (num_output_channels_) {
    Generic message;
    message.mutable_set_num_output_channels()->set_channels(
        num_output_channels_);
    if (!socket_->SendProto(0, message)) {
      return OnSendFailed();
    }
  }

  for (const auto& item : postprocessor_config_) {
    if (!SendPostprocessorMessageInternal(item.first, item.second)) {
      return OnSendFailed();
    }
  }

  if (!list_postprocessors_callbacks_.empty()) {
    Generic message;
    message.mutable_list_postprocessors();
    if (!socket_->SendProto(0, message)) {
      return OnSendFailed();
    }
  }

  if (connect_callback_) {
    connect_callback_.Run();
  }
}

void ControlConnection::OnSendFailed() {
  LOG(WARNING) << "Failed to send a control message";
  OnConnectionError();
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
  if (message.has_postprocessor_list()) {
    std::vector<std::string> post_processors;
    for (const auto& post_processor :
         message.postprocessor_list().postprocessors()) {
      post_processors.push_back(post_processor);
    }
    while (!list_postprocessors_callbacks_.empty()) {
      std::move(list_postprocessors_callbacks_.front()).Run(post_processors);
      list_postprocessors_callbacks_.pop_front();
    }
  }

  return true;
}

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast
