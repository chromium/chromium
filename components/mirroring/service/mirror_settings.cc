// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/mirror_settings.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/environment.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/mirroring/service/mirroring_features.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_parameters.h"
#include "media/base/video_codecs.h"

using media::ResolutionChangePolicy;
using media::cast::AudioCodecParams;
using media::cast::FrameSenderConfig;
using media::cast::VideoCodecParams;

namespace mirroring {

namespace {

// Default timebase for audio (48 kHz).
constexpr int kAudioTimebase = 48000;

// Default timebase for video (90 kHz).
constexpr int kVideoTimebase = 90000;

// Number of audio channels (stereo).
constexpr int kAudioChannels = 2;

// Audio frame rate (100 FPS for 10ms packets).
constexpr int kAudioFramerate = 100;

// Minimum video bitrate (300 kbps).
constexpr int kMinVideoBitrate = 300000;

// Maximum video bitrate (5 Mbps).
constexpr int kMaxVideoBitrate = 5000000;

// Default maximum frame rate for video (30 FPS).
constexpr int kDefaultMaxFrameRate = 30;

// Target maximum frame rate for 60FPS video streaming.
constexpr int kTargetMaxFrameRate60 = 60;

// Maximum resolution supported by default (1080p).
constexpr gfx::Size kMaxResolution{1920, 1080};

// Minimum resolution supported by default (180p).
constexpr gfx::Size kMinResolution{180, 180};

// Default maximum bitrate for audio (128 kbps).
// Bumped from legacy ~102 kbps formula to provide better quality.
constexpr int kDefaultAudioMaxBitrate = 128000;

int GetMaxFrameRate() {
  return base::FeatureList::IsEnabled(features::kCastStreaming60fps)
             ? kTargetMaxFrameRate60
             : kDefaultMaxFrameRate;
}

base::TimeDelta GetPlayoutDelayImpl() {
  // Currently min, max, and animated playout delay are the same.
  constexpr char kPlayoutDelayVariable[] = "CHROME_MIRRORING_PLAYOUT_DELAY";

  auto environment = base::Environment::Create();
  std::optional<std::string> playout_delay_arg =
      environment->GetVar(kPlayoutDelayVariable);
  if (!playout_delay_arg.has_value() || playout_delay_arg->empty()) {
    return kDefaultPlayoutDelay;
  }

  int playout_delay;
  if (!base::StringToInt(playout_delay_arg.value(), &playout_delay) ||
      playout_delay < 1 || playout_delay > 65535) {
    VLOG(1) << "Invalid custom mirroring playout delay passed, must be between "
               "1 and 65535 milliseconds. Using default value instead.";
    return kDefaultPlayoutDelay;
  }

  VLOG(1) << "Using custom mirroring playout delay value of: " << playout_delay
          << "ms...";
  return base::Milliseconds(playout_delay);
}

base::TimeDelta GetPlayoutDelay() {
  static base::TimeDelta playout_delay = GetPlayoutDelayImpl();
  return playout_delay;
}

}  // namespace

MirrorSettings::MirrorSettings(
    std::optional<base::TimeDelta> target_playout_delay)
    : target_playout_delay_(target_playout_delay),
      min_resolution_(kMinResolution),
      max_resolution_(kMaxResolution),
      max_frame_rate_(GetMaxFrameRate()) {}

MirrorSettings::~MirrorSettings() = default;

FrameSenderConfig MirrorSettings::GetVideoConfig(
    media::VideoCodec codec) const {
  FrameSenderConfig config;
  config.sender_ssrc = 11;
  config.receiver_ssrc = 12;
  const base::TimeDelta playout_delay =
      target_playout_delay_.value_or(GetPlayoutDelay());
  config.min_playout_delay = playout_delay;
  config.max_playout_delay = playout_delay;
  config.rtp_timebase = (codec == media::VideoCodec::kUnknown)
                            ? media::cast::kRemotingRtpTimebase
                            : kVideoTimebase;
  config.channels = 1;
  config.min_bitrate = kMinVideoBitrate;
  config.max_bitrate = kMaxVideoBitrate;
  config.start_bitrate = kMinVideoBitrate;
  config.max_frame_rate = max_frame_rate_;
  config.video_codec_params = VideoCodecParams(codec);

  return config;
}

FrameSenderConfig MirrorSettings::GetAudioConfig(
    media::AudioCodec codec) const {
  FrameSenderConfig config;
  config.sender_ssrc = 1;
  config.receiver_ssrc = 2;
  const base::TimeDelta playout_delay =
      target_playout_delay_.value_or(GetPlayoutDelay());
  config.min_playout_delay = playout_delay;
  config.max_playout_delay = playout_delay;
  config.rtp_timebase = (codec == media::AudioCodec::kUnknown)
                            ? media::cast::kRemotingRtpTimebase
                            : kAudioTimebase;
  config.channels = kAudioChannels;
  config.min_bitrate = 0;
  config.start_bitrate = kDefaultAudioMaxBitrate;
  config.max_bitrate = kDefaultAudioMaxBitrate;
  config.max_frame_rate = kAudioFramerate;  // 10 ms audio frames
  config.audio_codec_params = AudioCodecParams{.codec = codec};

  return config;
}

void MirrorSettings::SetMaxResolutionConstraints(gfx::Size max_resolution) {
  // Clamp the requested maximum resolution to ensure it does not fall below the
  // safe minimum resolution.
  max_resolution_ =
      gfx::Size(std::max(max_resolution.width(), min_resolution_.width()),
                std::max(max_resolution.height(), min_resolution_.height()));
}

void MirrorSettings::SetSenderSideLetterboxingEnabled(bool enabled) {
  enable_sender_side_letterboxing_ = enabled;
}

media::VideoCaptureParams MirrorSettings::GetVideoCaptureParams() const {
  media::VideoCaptureParams params;
  params.requested_format = media::VideoCaptureFormat(
      max_resolution_, max_frame_rate_, media::PIXEL_FORMAT_I420);
  if (max_resolution_ == min_resolution_) {
    params.resolution_change_policy = ResolutionChangePolicy::FIXED_RESOLUTION;
  } else if (enable_sender_side_letterboxing_) {
    params.resolution_change_policy =
        ResolutionChangePolicy::FIXED_ASPECT_RATIO;
  } else {
    params.resolution_change_policy = ResolutionChangePolicy::ANY_WITHIN_LIMIT;
  }
  params.is_high_dpi_enabled =
      base::FeatureList::IsEnabled(features::kCastEnableStreamingWithHiDPI);

  DCHECK(params.IsValid());
  return params;
}

media::AudioParameters MirrorSettings::GetAudioCaptureParams() const {
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(),
                                kAudioTimebase, kAudioTimebase / 100);
  DCHECK(params.IsValid());
  return params;
}

}  // namespace mirroring
