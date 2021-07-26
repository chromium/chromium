// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/streaming_receiver_session_client.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chromecast/shared/platform_info_serializer.h"
#include "components/cast/message_port/cast_core/create_message_port_core.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast_streaming/public/cast_streaming_url.h"
#include "components/cast_streaming/public/mojom/cast_streaming_session.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/openscreen/src/cast/streaming/constants.h"

namespace chromecast {
namespace {

openscreen::cast::VideoCodec ToOpenscreenCodec(media::VideoCodec codec) {
  switch (codec) {
    case media::VideoCodec::kCodecH264:
      return openscreen::cast::VideoCodec::kH264;
    case media::VideoCodec::kCodecVP8:
      return openscreen::cast::VideoCodec::kVp8;
    case media::VideoCodec::kCodecHEVC:
      return openscreen::cast::VideoCodec::kHevc;
    case media::VideoCodec::kCodecVP9:
      return openscreen::cast::VideoCodec::kVp9;
    default:
      break;
  }

  return openscreen::cast::VideoCodec::kNotSpecified;
}

openscreen::cast::AudioCodec ToOpenscreenCodec(media::AudioCodec codec) {
  switch (codec) {
    case media::AudioCodec::kCodecAAC:
      return openscreen::cast::AudioCodec::kAac;
    case media::AudioCodec::kCodecOpus:
      return openscreen::cast::AudioCodec::kOpus;
    default:
      break;
  }

  return openscreen::cast::AudioCodec::kNotSpecified;
}

cast_streaming::ReceiverSession::AVConstraints CreateConstraints(
    const PlatformInfoSerializer& deserializer) {
  cast_streaming::ReceiverSession::AVConstraints constraints;

  auto display_description =
      std::make_unique<openscreen::cast::ReceiverSession::Display>();
  bool use_display_description = false;
  auto width = deserializer.MaxWidth();
  if (width) {
    use_display_description = true;
    display_description->dimensions.width = width.value();
  }

  auto height = deserializer.MaxHeight();
  if (height) {
    use_display_description = true;
    display_description->dimensions.height = height.value();
  }

  auto frame_rate = deserializer.MaxFrameRate();
  if (frame_rate) {
    use_display_description = true;
    display_description->dimensions.frame_rate = frame_rate.value();
  }

  if (use_display_description) {
    constraints.display_description = std::move(display_description);
  }

  auto audio_codec_infos = deserializer.SupportedAudioCodecs();
  std::vector<openscreen::cast::AudioCodec> audio_codecs;
  std::vector<openscreen::cast::ReceiverSession::AudioLimits> audio_limits;
  if (audio_codec_infos) {
    for (auto& info : audio_codec_infos.value()) {
      auto converted_codec = ToOpenscreenCodec(info.codec);
      if (converted_codec == openscreen::cast::AudioCodec::kNotSpecified) {
        continue;
      }

      if (std::find(audio_codecs.begin(), audio_codecs.end(),
                    converted_codec) == audio_codecs.end()) {
        audio_codecs.push_back(converted_codec);

        audio_limits.emplace_back();
        auto& limit = audio_limits.back();
        limit.codec = converted_codec;
        limit.max_sample_rate = info.max_samples_per_second;
        limit.max_channels = info.max_audio_channels;
        continue;
      }

      auto it = std::find_if(
          audio_limits.begin(), audio_limits.end(),
          [converted_codec](
              const openscreen::cast::ReceiverSession::AudioLimits& limit) {
            return limit.codec == converted_codec;
          });
      DCHECK(it != audio_limits.end());
      it->max_sample_rate =
          std::max(it->max_sample_rate, info.max_samples_per_second);
      it->max_channels = std::max(it->max_channels, info.max_audio_channels);
    }
  }

  if (!audio_codecs.empty()) {
    DCHECK(!audio_limits.empty());
    constraints.audio_codecs = std::move(audio_codecs);
    constraints.audio_limits = std::move(audio_limits);
  } else {
    auto max_channels = deserializer.MaxChannels();
    if (max_channels) {
      constraints.audio_limits.emplace_back();
      constraints.audio_limits.back().applies_to_all_codecs = true;
      constraints.audio_limits.back().max_channels = max_channels.value();
    }
  }

  auto video_codec_infos = deserializer.SupportedVideoCodecs();
  if (video_codec_infos && video_codec_infos.value().size()) {
    std::vector<openscreen::cast::VideoCodec> video_codecs;
    for (auto& info : video_codec_infos.value()) {
      auto converted_codec = ToOpenscreenCodec(info.codec);
      if (converted_codec != openscreen::cast::VideoCodec::kNotSpecified &&
          std::find(video_codecs.begin(), video_codecs.end(),
                    converted_codec) == video_codecs.end()) {
        video_codecs.push_back(converted_codec);
      }
    }

    if (!video_codecs.empty()) {
      constraints.video_codecs = std::move(video_codecs);
    }
  }

  return constraints;
}

std::unique_ptr<cast_streaming::ReceiverSession> CreateReceiverSession(
    cast_streaming::ReceiverSession::MessagePortProvider message_port_provider,
    cast_streaming::ReceiverSession::AVConstraints constraints) {
  return cast_streaming::ReceiverSession::Create(
      std::make_unique<cast_streaming::ReceiverSession::AVConstraints>(
          std::move(constraints)),
      std::move(message_port_provider));
}

}  // namespace

StreamingReceiverSessionClient::StreamingReceiverSessionClient(
    cast_streaming::NetworkContextGetter network_context_getter,
    cast_streaming::ReceiverSession::MessagePortProvider message_port_provider,
    Handler* handler)
    : StreamingReceiverSessionClient(
          std::move(network_context_getter),
          base::BindOnce(&CreateReceiverSession,
                         std::move(message_port_provider)),
          handler) {}

StreamingReceiverSessionClient::StreamingReceiverSessionClient(
    cast_streaming::NetworkContextGetter network_context_getter,
    ReceiverSessionFactory receiver_session_factory,
    Handler* handler)
    : handler_(handler),
      receiver_session_factory_(std::move(receiver_session_factory)) {
  DCHECK(handler_);
  DCHECK(receiver_session_factory_);
  DCHECK(!network_context_getter.is_null());

  cast_streaming::SetNetworkContextGetter(std::move(network_context_getter));

  std::unique_ptr<cast_api_bindings::MessagePort> server;
  cast_api_bindings::CreateMessagePortCorePair(&message_port_, &server);

  DCHECK(message_port_);
  message_port_->SetReceiver(this);

  handler_->StartAvSettingsQuery(std::move(server));
}

StreamingReceiverSessionClient::~StreamingReceiverSessionClient() {
  cast_streaming::SetNetworkContextGetter({});
}

StreamingReceiverSessionClient::Handler::~Handler() = default;

void StreamingReceiverSessionClient::LaunchStreamingReceiver(
    CastWebContents* cast_web_contents) {
  DCHECK(av_constraints_);
  DCHECK(receiver_session_factory_);

  receiver_session_ =
      std::move(receiver_session_factory_).Run(av_constraints_.value());
  Observe(cast_web_contents);
}

void StreamingReceiverSessionClient::MainFrameReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(navigation_handle);

  if (has_streaming_started_ || !receiver_session_ ||
      !cast_streaming::IsCastStreamingMediaSourceUrl(
          navigation_handle->GetURL())) {
    return;
  }

  mojo::AssociatedRemote<::mojom::CastStreamingReceiver>
      cast_streaming_receiver;
  navigation_handle->GetRenderFrameHost()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&cast_streaming_receiver);
  receiver_session_->SetCastStreamingReceiver(
      std::move(cast_streaming_receiver));

  has_streaming_started_ = true;
  handler_->OnStreamingSessionStarted();
}

bool StreamingReceiverSessionClient::OnMessage(
    base::StringPiece message,
    std::vector<std::unique_ptr<cast_api_bindings::MessagePort>> ports) {
  DCHECK(ports.empty());

  absl::optional<PlatformInfoSerializer> deserializer =
      PlatformInfoSerializer::TryParse(message);
  if (!deserializer) {
    handler_->OnError();
    LOG(ERROR) << "AV Settings with invalid JSON received: " << message;
    return false;
  }

  auto constraints = CreateConstraints(deserializer.value());
  if (!has_streaming_started_) {
    av_constraints_ = std::move(constraints);
    return true;
  }

  DCHECK(av_constraints_);
  if (!constraints.IsSupersetOf(av_constraints_.value())) {
    handler_->OnError();
    LOG(WARNING) << "Device no longer supports capabilities used for "
                 << "cast streaming session negotiation: " << message;
    return false;
  }

  return true;
}

void StreamingReceiverSessionClient::OnPipeError() {
  DLOG(WARNING) << "Pipe disconnected.";
  handler_->OnError();
}

}  // namespace chromecast
