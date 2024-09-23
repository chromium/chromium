// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/receiver_config_conversions.h"

#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "components/cast_streaming/browser/public/receiver_config.h"
#include "media/base/audio_codecs.h"
#include "media/base/channel_layout.h"
#include "media/base/video_codecs.h"
#include "media/cast/openscreen/config_conversions.h"
#include "third_party/openscreen/src/cast/streaming/public/constants.h"
#include "ui/gfx/geometry/rect.h"

namespace cast_streaming {
namespace {

int GetMaxChannelCount(media::ChannelLayout channel_layout) {
  switch (channel_layout) {
    case media::CHANNEL_LAYOUT_MONO:
      return 1;
    case media::CHANNEL_LAYOUT_STEREO:
    case media::CHANNEL_LAYOUT_STEREO_DOWNMIX:
    case media::CHANNEL_LAYOUT_1_1:
      return 2;
    case media::CHANNEL_LAYOUT_2_1:
    case media::CHANNEL_LAYOUT_SURROUND:
    case media::CHANNEL_LAYOUT_2POINT1:
    case media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC:
      return 3;
    case media::CHANNEL_LAYOUT_4_0:
    case media::CHANNEL_LAYOUT_2_2:
    case media::CHANNEL_LAYOUT_QUAD:
    case media::CHANNEL_LAYOUT_3_1:
    case media::CHANNEL_LAYOUT_3_1_BACK:
      return 4;
    case media::CHANNEL_LAYOUT_5_0:
    case media::CHANNEL_LAYOUT_5_0_BACK:
    case media::CHANNEL_LAYOUT_4_1:
    case media::CHANNEL_LAYOUT_4_1_QUAD_SIDE:
      return 5;
    case media::CHANNEL_LAYOUT_5_1:
    case media::CHANNEL_LAYOUT_5_1_BACK:
    case media::CHANNEL_LAYOUT_6_0:
    case media::CHANNEL_LAYOUT_6_0_FRONT:
    case media::CHANNEL_LAYOUT_HEXAGONAL:
    case media::CHANNEL_LAYOUT_5_1_4_DOWNMIX:
      return 6;
    case media::CHANNEL_LAYOUT_7_0:
    case media::CHANNEL_LAYOUT_6_1:
    case media::CHANNEL_LAYOUT_6_1_BACK:
    case media::CHANNEL_LAYOUT_6_1_FRONT:
    case media::CHANNEL_LAYOUT_7_0_FRONT:
      return 7;
    case media::CHANNEL_LAYOUT_7_1:
    case media::CHANNEL_LAYOUT_7_1_WIDE:
    case media::CHANNEL_LAYOUT_7_1_WIDE_BACK:
    case media::CHANNEL_LAYOUT_OCTAGONAL:
      return 8;
    default:
      return 1;
  }
}

openscreen::cast::Dimensions ToOpenscreenType(const gfx::Rect& rect,
                                              int frame_rate) {
  return openscreen::cast::Dimensions{
      rect.width(), rect.height(), {frame_rate, 1}};
}

openscreen::cast::VideoLimits ToOpenscreenVideoLimitsType(
    const ReceiverConfig::VideoLimits& limits) {
  openscreen::cast::VideoLimits osp_limits;

  if (limits.codec) {
    osp_limits.applies_to_all_codecs = false;
    osp_limits.codec =
        media::cast::ToVideoCaptureConfigCodec(limits.codec.value());
  } else {
    osp_limits.applies_to_all_codecs = true;
  }

  osp_limits.max_dimensions = ToOpenscreenType(
      limits.max_dimensions,
      limits.max_frame_rate.value_or(openscreen::cast::kDefaultFrameRate));
  osp_limits.max_delay =
      std::chrono::milliseconds(limits.max_delay.InMilliseconds());
  osp_limits.max_pixels_per_second = limits.max_pixels_per_second;
  if (limits.min_bit_rate) {
    osp_limits.min_bit_rate = limits.min_bit_rate.value();
  }
  if (limits.max_bit_rate) {
    osp_limits.max_bit_rate = limits.max_bit_rate.value();
  }

  return osp_limits;
}

openscreen::cast::AudioLimits ToOpenscreenAudioLimitsType(
    const ReceiverConfig::AudioLimits& limits) {
  openscreen::cast::AudioLimits osp_limits;

  if (limits.codec) {
    osp_limits.applies_to_all_codecs = false;
    osp_limits.codec =
        media::cast::ToAudioCaptureConfigCodec(limits.codec.value());
  } else {
    osp_limits.applies_to_all_codecs = true;
  }

  osp_limits.max_channels = GetMaxChannelCount(limits.channel_layout);
  osp_limits.max_delay =
      std::chrono::milliseconds(limits.max_delay.InMilliseconds());
  if (limits.max_sample_rate) {
    osp_limits.max_sample_rate = limits.max_sample_rate.value();
  }
  if (limits.min_bit_rate) {
    osp_limits.min_bit_rate = limits.min_bit_rate.value();
  }
  if (limits.max_bit_rate) {
    osp_limits.max_bit_rate = limits.max_bit_rate.value();
  }

  return osp_limits;
}

openscreen::cast::Display ToOpenscreenDisplayType(
    const ReceiverConfig::Display& display) {
  openscreen::cast::Display osp_display;
  osp_display.dimensions = ToOpenscreenType(
      display.dimensions,
      display.max_frame_rate.value_or(openscreen::cast::kDefaultFrameRate));
  osp_display.can_scale_content = display.can_scale_content;
  return osp_display;
}

openscreen::cast::RemotingConstraints ToOpenscreenRemotingConstraintsType(
    const ReceiverConfig::RemotingConstraints& constraints) {
  openscreen::cast::RemotingConstraints osp_constraints;
  osp_constraints.supports_4k = constraints.supports_4k;
  osp_constraints.supports_chrome_audio_codecs =
      constraints.supports_mp3 || constraints.supports_ogg_vorbis ||
      constraints.supports_flac || constraints.supports_mpegh ||
      constraints.supports_pcm || constraints.supports_amr ||
      constraints.supports_gsm || constraints.supports_eac3 ||
      constraints.supports_alac || constraints.supports_ac3 ||
      constraints.supports_dts;
  return osp_constraints;
}

}  // namespace

openscreen::cast::ReceiverConstraints ToOpenscreenConstraints(
    const ReceiverConfig& config) {
  std::vector<openscreen::cast::AudioCodec> audio_codecs;
  audio_codecs.reserve(config.audio_codecs.size());
  base::ranges::transform(
      config.audio_codecs.begin(), config.audio_codecs.end(),
      std::back_inserter(audio_codecs), media::cast::ToAudioCaptureConfigCodec);

  std::vector<openscreen::cast::VideoCodec> video_codecs;
  video_codecs.reserve(config.video_codecs.size());
  base::ranges::transform(
      config.video_codecs.begin(), config.video_codecs.end(),
      std::back_inserter(video_codecs), media::cast::ToVideoCaptureConfigCodec);

  openscreen::cast::ReceiverConstraints constraints(std::move(video_codecs),
                                                    std::move(audio_codecs));

  constraints.audio_limits.reserve(config.audio_limits.size());
  base::ranges::transform(config.audio_limits,
                          std::back_inserter(constraints.audio_limits),
                          ToOpenscreenAudioLimitsType);

  constraints.video_limits.reserve(config.video_limits.size());
  base::ranges::transform(config.video_limits,
                          std::back_inserter(constraints.video_limits),
                          ToOpenscreenVideoLimitsType);

  if (config.display_description) {
    constraints.display_description =
        std::make_unique<openscreen::cast::Display>(
            ToOpenscreenDisplayType(config.display_description.value()));
  }

  if (config.remoting) {
    constraints.remoting =
        std::make_unique<openscreen::cast::RemotingConstraints>(
            ToOpenscreenRemotingConstraintsType(config.remoting.value()));
  }

  return constraints;
}

}  // namespace cast_streaming
