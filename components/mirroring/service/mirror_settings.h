// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_MIRROR_SETTINGS_H_
#define COMPONENTS_MIRRORING_SERVICE_MIRROR_SETTINGS_H_

#include "base/component_export.h"
#include "base/time/time.h"
#include "media/capture/video_capture_types.h"
#include "media/cast/cast_config.h"

namespace media {
class AudioParameters;
enum class AudioCodec;
enum class VideoCodec;
}  // namespace media

namespace mirroring {

// The interval since the last video frame was received from the video source,
// before requesting a refresh frame.
inline constexpr base::TimeDelta kFrameRefreshInterval = base::Milliseconds(50);

// Default end-to-end latency. Currently adaptive latency control is disabled
// because of audio playout regressions (b/32876644).
// TODO(openscreen/44): Re-enable in port to Open Screen.
inline constexpr base::TimeDelta kDefaultPlayoutDelay = base::Milliseconds(200);

// Holds the default settings for a mirroring session. This class provides the
// audio/video configs that this sender supports. And also provides the
// audio/video constraints used for capturing.
// TODO(issuetracker.google.com/158032164): as part of migration to libcast's
// OFFER/ANSWER exchange, expose constraints here from the OFFER message.
class COMPONENT_EXPORT(MIRRORING_SERVICE) MirrorSettings {
 public:
  MirrorSettings();

  MirrorSettings(const MirrorSettings&) = delete;
  MirrorSettings& operator=(const MirrorSettings&) = delete;

  ~MirrorSettings();

  // Get the audio/video config with given codec.
  static media::cast::FrameSenderConfig GetDefaultAudioConfig(
      media::cast::RtpPayloadType payload_type,
      media::AudioCodec codec);
  static media::cast::FrameSenderConfig GetDefaultVideoConfig(
      media::cast::RtpPayloadType payload_type,
      media::VideoCodec codec);

  // Call to override the default resolution settings.
  void SetResolutionConstraints(int max_width, int max_height);
  void SetSenderSideLetterboxingEnabled(bool enabled);

  // Get video capture constraints with the current settings.
  media::VideoCaptureParams GetVideoCaptureParams();

  // Get Audio capture constraints with the current settings.
  media::AudioParameters GetAudioCaptureParams();

  int max_width() const { return max_width_; }
  int max_height() const { return max_height_; }

  base::TimeDelta refresh_interval() const { return refresh_interval_; }
  void set_refresh_interval(base::TimeDelta refresh_interval) {
    refresh_interval_ = refresh_interval;
  }

 private:
  const int min_width_;
  const int min_height_;
  int max_width_;
  int max_height_;
  bool enable_sender_side_letterboxing_ = true;
  base::TimeDelta refresh_interval_{kFrameRefreshInterval};
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_MIRROR_SETTINGS_H_
