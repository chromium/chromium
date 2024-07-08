// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/core_streaming_config_manager.h"

#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/shared/platform_info_serializer.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast_receiver/browser/public/message_port_service.h"
#include "components/cast_receiver/common/public/status.h"
#include "media/base/audio_codecs.h"
#include "media/base/channel_layout.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"

namespace chromecast {
namespace {

constexpr char kMediaCapabilitiesBindingName[] =
    "cast.__platform__.canDisplayType";

::media::VideoCodec ToChromiumCodec(media::VideoCodec codec) {
  switch (codec) {
    case media::VideoCodec::kCodecH264:
      return ::media::VideoCodec::kH264;
    case media::VideoCodec::kCodecVP8:
      return ::media::VideoCodec::kVP8;
    case media::VideoCodec::kCodecHEVC:
      return ::media::VideoCodec::kHEVC;
    case media::VideoCodec::kCodecVP9:
      return ::media::VideoCodec::kVP9;
    case media::VideoCodec::kCodecAV1:
      return ::media::VideoCodec::kAV1;
    default:
      break;
  }

  return ::media::VideoCodec::kUnknown;
}

::media::AudioCodec ToChromiumCodec(media::AudioCodec codec) {
  switch (codec) {
    case media::AudioCodec::kCodecAAC:
      return ::media::AudioCodec::kAAC;
    case media::AudioCodec::kCodecOpus:
      return ::media::AudioCodec::kOpus;
    default:
      break;
  }

  return ::media::AudioCodec::kUnknown;
}

cast_streaming::ReceiverConfig CreateConfig(
    const PlatformInfoSerializer& deserializer) {
  cast_streaming::ReceiverConfig constraints;

  const std::optional<int> width = deserializer.MaxWidth();
  const std::optional<int> height = deserializer.MaxHeight();
  const std::optional<int> frame_rate = deserializer.MaxFrameRate();
  if (width && *width && height && *height && frame_rate && *frame_rate) {
    cast_streaming::ReceiverConfig::Display display;
    display.dimensions = gfx::Rect{*width, *height};
    display.max_frame_rate = *frame_rate;
    constraints.display_description = std::move(display);
  } else {
    DLOG(WARNING)
        << "Some Display properties missing. Using default values for "
        << "MaxWidth, MaxHeight, and MaxFrameRate";

#if DCHECK_IS_ON()
    if (!height) {
      LOG(INFO) << "MaxHeight value not present in received AV Settings.";
    } else if (!*height) {
      LOG(WARNING) << "Invalid MaxHeight of 0 parsed from AV Settings.";
    } else if (height) {
      LOG(WARNING)
          << "MaxHeight value ignored due to missing display properties.";
    }

    if (!width) {
      LOG(INFO) << "MaxWidth value not present in received AV Settings.";
    } else if (!*width) {
      LOG(WARNING) << "Invalid MaxWidth of 0 parsed from AV Settings.";
    } else if (width) {
      LOG(WARNING)
          << "MaxWidth value ignored due to missing display properties.";
    }

    if (!frame_rate) {
      LOG(INFO) << "MaxFrameRate value not present in received AV Settings.";
    } else if (!*frame_rate) {
      LOG(WARNING) << "Invalid MaxFrameRate of 0 parsed from AV Settings.";
    } else if (frame_rate) {
      LOG(WARNING)
          << "MaxFrameRate value ignored due to missing display properties.";
    }
#endif  // DCHECK_IS_ON()
  }

  auto audio_codec_infos = deserializer.SupportedAudioCodecs();
  std::vector<::media::AudioCodec> audio_codecs;
  std::vector<cast_streaming::ReceiverConfig::AudioLimits> audio_limits;
  if (!audio_codec_infos || (*audio_codec_infos).empty()) {
    DLOG(WARNING) << "No AudioCodecInfos in received AV Settings.";
  } else {
    for (auto& info : *audio_codec_infos) {
      const auto converted_codec = ToChromiumCodec(info.codec);
      if (converted_codec == ::media::AudioCodec::kUnknown) {
        DLOG(INFO) << "Skipping processing of unknown audio codec...";
        continue;
      }

      if (!base::Contains(audio_codecs, converted_codec)) {
        audio_codecs.push_back(converted_codec);

        audio_limits.emplace_back();
        auto& limit = audio_limits.back();
        limit.codec = converted_codec;
        limit.max_sample_rate = info.max_samples_per_second;
        limit.channel_layout =
            ::media::GuessChannelLayout(info.max_audio_channels);
        continue;
      }

      auto it = base::ranges::find(
          audio_limits, converted_codec,
          &cast_streaming::ReceiverConfig::AudioLimits::codec);
      CHECK(it != audio_limits.end(), base::NotFatalUntil::M130);
      if (it->max_sample_rate) {
        it->max_sample_rate =
            std::max(it->max_sample_rate.value(), info.max_samples_per_second);
      } else {
        it->max_sample_rate = info.max_samples_per_second;
      }
      it->channel_layout = ::media::GuessChannelLayout(info.max_audio_channels);
    }
  }

  if (!audio_codecs.empty()) {
    DCHECK(!audio_limits.empty());
    constraints.audio_codecs = std::move(audio_codecs);
    constraints.audio_limits = std::move(audio_limits);
  } else {
    auto max_channels = deserializer.MaxChannels();
    if (max_channels && *max_channels) {
      constraints.audio_limits.emplace_back();
      constraints.audio_limits.back().channel_layout =
          ::media::GuessChannelLayout(*max_channels);
    }
  }

  auto video_codec_infos = deserializer.SupportedVideoCodecs();
  if (!video_codec_infos || (*video_codec_infos).empty()) {
    DLOG(WARNING) << "No VideoCodecInfos in received AV Settings.";
  } else {
    std::vector<::media::VideoCodec> video_codecs;
    for (auto& info : *video_codec_infos) {
      const auto converted_codec = ToChromiumCodec(info.codec);
      if (converted_codec == ::media::VideoCodec::kUnknown) {
        DLOG(INFO) << "Skipping processing of unknown video codec...";
        continue;
      }

      if (!base::Contains(video_codecs, converted_codec)) {
        video_codecs.push_back(converted_codec);
      }
    }

    if (!video_codecs.empty()) {
      constraints.video_codecs = std::move(video_codecs);
    }
  }

  return constraints;
}

}  // namespace

CoreStreamingConfigManager::CoreStreamingConfigManager(
    cast_receiver::MessagePortService& message_port_service,
    cast_receiver::RuntimeApplication::StatusCallback error_cb)
    : CoreStreamingConfigManager(std::move(error_cb)) {
  std::unique_ptr<cast_api_bindings::MessagePort> server;
  cast_api_bindings::CreatePlatformMessagePortPair(&message_port_, &server);
  message_port_->SetReceiver(this);

  message_port_service.ConnectToPortAsync(kMediaCapabilitiesBindingName,
                                          std::move(server));
}

CoreStreamingConfigManager::CoreStreamingConfigManager(
    cast_receiver::RuntimeApplication::StatusCallback error_cb)
    : error_callback_(std::move(error_cb)) {}

CoreStreamingConfigManager::~CoreStreamingConfigManager() = default;

bool CoreStreamingConfigManager::OnMessage(
    std::string_view message,
    std::vector<std::unique_ptr<cast_api_bindings::MessagePort>> ports) {
  DLOG(INFO) << "AV Settings Response Received: " << message;

  DCHECK(ports.empty());

  std::optional<PlatformInfoSerializer> deserializer =
      PlatformInfoSerializer::Deserialize(message);
  if (!deserializer) {
    LOG(ERROR) << "AV Settings with invalid protobuf received: " << message;
    if (error_callback_) {
      std::move(error_callback_)
          .Run(cast_receiver::Status(
              cast_receiver::StatusCode::kInvalidArgument,
              "AV Settings with invalid protobuf received"));
    }
    return false;
  }

  OnStreamingConfigSet(CreateConfig(*deserializer));
  return true;
}

void CoreStreamingConfigManager::OnPipeError() {
  DLOG(WARNING) << "Pipe disconnected.";
  if (error_callback_) {
    std::move(error_callback_)
        .Run(cast_receiver::Status(cast_receiver::StatusCode::kInternal,
                                   "MessagePort pipe disconnected"));
  }
}

}  // namespace chromecast
