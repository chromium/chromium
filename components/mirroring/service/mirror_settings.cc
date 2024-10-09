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
using media::cast::RtpPayloadType;
using media::cast::VideoCodecParams;

namespace mirroring {

namespace {

constexpr int kAudioTimebase = 48000;
constexpr int kVideoTimebase = 90000;
constexpr int kAudioChannels = 2;
constexpr int kAudioFramerate = 100;  // 100 FPS for 10ms packets.
constexpr int kMinVideoBitrate = 300000;
constexpr int kMaxVideoBitrate = 5000000;
constexpr int kAudioBitrate = 0;   // 0 means automatic.
constexpr int kMaxFrameRate = 30;  // The maximum frame rate for captures.
constexpr int kMaxWidth = 1920;    // Maximum video width in pixels.
constexpr int kMaxHeight = 1080;   // Maximum video height in pixels.
constexpr int kMinWidth = 180;     // Minimum video frame width in pixels.
constexpr int kMinHeight = 180;    // Minimum video frame height in pixels.

base::TimeDelta GetPlayoutDelayImpl() {
  // Currently min, max, and animated playout delay are the same.
  constexpr char kPlayoutDelayVariable[] = "CHROME_MIRRORING_PLAYOUT_DELAY";

  auto environment = base::Environment::Create();
  if (!environment->HasVar(kPlayoutDelayVariable)) {
    return kDefaultPlayoutDelay;
  }

  std::string playout_delay_arg;
  if (!environment->GetVar(kPlayoutDelayVariable, &playout_delay_arg) ||
      playout_delay_arg.empty()) {
    return kDefaultPlayoutDelay;
  }

  int playout_delay;
  if (!base::StringToInt(playout_delay_arg, &playout_delay) ||
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

MirrorSettings::MirrorSettings()
    : min_width_(kMinWidth),
      min_height_(kMinHeight),
      max_width_(kMaxWidth),
      max_height_(kMaxHeight) {}

MirrorSettings::~MirrorSettings() = default;

// static
FrameSenderConfig MirrorSettings::GetDefaultAudioConfig(
    RtpPayloadType payload_type,
    media::AudioCodec codec) {
  FrameSenderConfig config;
  config.sender_ssrc = 1;
  config.receiver_ssrc = 2;
  const base::TimeDelta playout_delay = GetPlayoutDelay();
  config.min_playout_delay = playout_delay;
  config.max_playout_delay = playout_delay;
  config.rtp_payload_type = payload_type;
  config.rtp_timebase = (payload_type == RtpPayloadType::REMOTE_AUDIO)
                            ? media::cast::kRemotingRtpTimebase
                            : kAudioTimebase;
  config.channels = kAudioChannels;
  config.min_bitrate = config.max_bitrate = config.start_bitrate =
      kAudioBitrate;
  config.max_frame_rate = kAudioFramerate;  // 10 ms audio frames
  config.audio_codec_params = AudioCodecParams{.codec = codec};
  return config;
}

// static
FrameSenderConfig MirrorSettings::GetDefaultVideoConfig(
    RtpPayloadType payload_type,
    media::VideoCodec codec) {
  FrameSenderConfig config;
  config.sender_ssrc = 11;
  config.receiver_ssrc = 12;
  const base::TimeDelta playout_delay = GetPlayoutDelay();
  config.min_playout_delay = playout_delay;
  config.max_playout_delay = playout_delay;
  config.rtp_payload_type = payload_type;
  config.rtp_timebase = (payload_type == RtpPayloadType::REMOTE_VIDEO)
                            ? media::cast::kRemotingRtpTimebase
                            : kVideoTimebase;
  config.channels = 1;
  config.min_bitrate = kMinVideoBitrate;
  config.max_bitrate = kMaxVideoBitrate;
  config.start_bitrate = kMinVideoBitrate;
  config.max_frame_rate = kMaxFrameRate;
  config.video_codec_params = VideoCodecParams(codec);
  return config;
}

void MirrorSettings::SetResolutionConstraints(int max_width, int max_height) {
  max_width_ = std::max(max_width, min_width_);
  max_height_ = std::max(max_height, min_height_);
}

void MirrorSettings::SetSenderSideLetterboxingEnabled(bool enabled) {
  enable_sender_side_letterboxing_ = enabled;
}

media::VideoCaptureParams MirrorSettings::GetVideoCaptureParams() {
  media::VideoCaptureParams params;
  params.requested_format =
      media::VideoCaptureFormat(gfx::Size(max_width_, max_height_),
                                kMaxFrameRate, media::PIXEL_FORMAT_I420);
  if (max_height_ == min_height_ && max_width_ == min_width_) {
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

media::AudioParameters MirrorSettings::GetAudioCaptureParams() {
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(),
                                kAudioTimebase, kAudioTimebase / 100);
  DCHECK(params.IsValid());
  return params;
}

}  // namespace mirroring
