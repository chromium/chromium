// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/mixer_service_receiver.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "chromecast/media/audio/mixer_service/conversions.h"
#include "chromecast/media/audio/mixer_service/mixer_service.pb.h"
#include "chromecast/media/audio/mixer_service/mixer_socket.h"
#include "chromecast/media/cma/backend/mixer/audio_output_redirector.h"
#include "chromecast/media/cma/backend/mixer/loopback_handler.h"
#include "chromecast/media/cma/backend/mixer/mixer_input_connection.h"
#include "chromecast/media/cma/backend/mixer/mixer_loopback_connection.h"
#include "chromecast/media/cma/backend/mixer/stream_mixer.h"

namespace chromecast {
namespace media {

class MixerServiceReceiver::ControlConnection
    : public mixer_service::MixerSocket::Delegate {
 public:
  ControlConnection(StreamMixer* mixer,
                    MixerServiceReceiver* receiver,
                    std::unique_ptr<mixer_service::MixerSocket> socket)
      : mixer_(mixer), receiver_(receiver), socket_(std::move(socket)) {
    DCHECK(mixer_);
    DCHECK(receiver_);
    DCHECK(socket_);

    socket_->SetDelegate(this);
  }

  ~ControlConnection() override = default;

  void OnStreamCountChanged() {
    if (!send_stream_count_) {
      return;
    }
    mixer_service::Generic message;
    auto* counts = message.mutable_stream_count();
    counts->set_primary(receiver_->primary_stream_count_);
    counts->set_sfx(receiver_->sfx_stream_count_);
    socket_->SendProto(message);
  }

 private:
  friend class MixerServiceReceiver;

  // mixer_service::MixerSocket::Delegate implementation:
  bool HandleMetadata(const mixer_service::Generic& message) override {
    if (message.has_set_volume_limit()) {
      mixer_->SetOutputLimit(
          mixer_service::ConvertContentType(
              message.set_volume_limit().content_type()),
          message.set_volume_limit().max_volume_multiplier());
    }
    if (message.has_set_device_muted()) {
      mixer_->SetMuted(mixer_service::ConvertContentType(
                           message.set_device_muted().content_type()),
                       message.set_device_muted().muted());
    }
    if (message.has_set_device_volume()) {
      mixer_->SetVolume(mixer_service::ConvertContentType(
                            message.set_device_volume().content_type()),
                        message.set_device_volume().volume_multiplier());
    }
    if (message.has_configure_postprocessor()) {
      mixer_->SetPostProcessorConfig(
          message.configure_postprocessor().name(),
          message.configure_postprocessor().config());
    }
    if (message.has_reload_postprocessors()) {
      mixer_->ResetPostProcessors([](bool, const std::string&) {});
    }
    if (message.has_request_stream_count()) {
      send_stream_count_ = message.request_stream_count().subscribe();
      OnStreamCountChanged();
    }
    if (message.has_set_num_output_channels()) {
      mixer_->SetNumOutputChannels(
          message.set_num_output_channels().channels());
    }

    return true;
  }

  bool HandleAudioData(char* data, int size, int64_t timestamp) override {
    return true;
  }

  bool HandleAudioBuffer(scoped_refptr<net::IOBuffer> buffer,
                         char* data,
                         int size,
                         int64_t timestamp) override {
    return true;
  }

  void OnConnectionError() override {
    receiver_->RemoveControlConnection(this);
  }

  StreamMixer* const mixer_;
  MixerServiceReceiver* const receiver_;
  const std::unique_ptr<mixer_service::MixerSocket> socket_;

  bool send_stream_count_ = false;

  DISALLOW_COPY_AND_ASSIGN(ControlConnection);
};

MixerServiceReceiver::MixerServiceReceiver(StreamMixer* mixer,
                                           LoopbackHandler* loopback_handler)
    : mixer_(mixer), loopback_handler_(loopback_handler) {
  DCHECK(mixer_);
  DCHECK(loopback_handler_);
}

MixerServiceReceiver::~MixerServiceReceiver() = default;

void MixerServiceReceiver::OnStreamCountChanged(int primary, int sfx) {
  primary_stream_count_ = primary;
  sfx_stream_count_ = sfx;

  for (const auto& control : control_connections_) {
    control.second->OnStreamCountChanged();
  }
}

void MixerServiceReceiver::CreateOutputStream(
    std::unique_ptr<mixer_service::MixerSocket> socket,
    const mixer_service::Generic& message) {
  DCHECK(message.has_output_stream_params());
  // MixerInputConnection manages its own lifetime.
  auto* connection = new MixerInputConnection(mixer_, std::move(socket),
                                              message.output_stream_params());
  connection->HandleMetadata(message);
}

void MixerServiceReceiver::CreateLoopbackConnection(
    std::unique_ptr<mixer_service::MixerSocket> socket,
    const mixer_service::Generic& message) {
  auto connection =
      std::make_unique<MixerLoopbackConnection>(std::move(socket));
  loopback_handler_->AddConnection(std::move(connection));
}

void MixerServiceReceiver::CreateAudioRedirection(
    std::unique_ptr<mixer_service::MixerSocket> socket,
    const mixer_service::Generic& message) {
  if (message.redirection_request().has_num_channels() &&
      message.redirection_request().num_channels() <= 0) {
    LOG(INFO) << "Bad redirection request";
    return;
  }
  mixer_->AddAudioOutputRedirector(std::make_unique<AudioOutputRedirector>(
      mixer_, std::move(socket), message));
}

void MixerServiceReceiver::CreateControlConnection(
    std::unique_ptr<mixer_service::MixerSocket> socket,
    const mixer_service::Generic& message) {
  auto connection =
      std::make_unique<ControlConnection>(mixer_, this, std::move(socket));
  ControlConnection* ptr = connection.get();
  control_connections_[ptr] = std::move(connection);
  ptr->HandleMetadata(message);
}

void MixerServiceReceiver::RemoveControlConnection(ControlConnection* ptr) {
  control_connections_.erase(ptr);
}

}  // namespace media
}  // namespace chromecast
