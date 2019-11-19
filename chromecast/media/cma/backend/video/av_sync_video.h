// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_AV_SYNC_VIDEO_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_AV_SYNC_VIDEO_H_

#include <cstdint>
#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/timer/timer.h"
#include "chromecast/media/cma/backend/av_sync.h"

namespace chromecast {
class WeightedMovingLinearRegression;

namespace media {
class MediaPipelineBackendForMixer;

class AvSyncVideo : public AvSync {
 public:
  explicit AvSyncVideo(MediaPipelineBackendForMixer* const backend);
  ~AvSyncVideo() override;

  // AvSync implementation:
  void NotifyStart(int64_t timestamp, int64_t pts) override;
  void NotifyStop() override;
  void NotifyPause() override;
  void NotifyResume() override;
  void NotifyPlaybackRateChange(float rate) override;

 private:
  void StartAvSync();
  void StopAvSync();

  void UpkeepAvSync();
  bool VptsUpkeep();
  int GetVideoFrameRate();

  void HardCorrection(int64_t apts, int64_t desired_apts_timestamp);
  void AudioRateUpkeep(int64_t error, int64_t timestamp);

  void FlushAudioPts();
  void FlushVideoPts();

  MediaPipelineBackendForMixer* const backend_;

  base::RepeatingTimer upkeep_av_sync_timer_;

  std::unique_ptr<WeightedMovingLinearRegression> apts_error_;
  std::unique_ptr<WeightedMovingLinearRegression> video_pts_;

  // This is the audio playback rate propagated from SetPlaybackRate, which is
  // exposed to the user to speed up or slow down their playback.
  double current_media_playback_rate_ = 1.0;

  // This is the small playback rate change done to maintain AV sync.
  double current_audio_clock_rate_ = 1.0;

  int64_t playback_start_timestamp_ = INT64_MAX;

  int64_t last_vpts_value_ = INT64_MIN;

  int64_t last_apts_value_ = INT64_MIN;
  int64_t last_apts_timestamp_ = INT64_MIN;

  int64_t clock_rate_start_timestamp_ = INT64_MIN;
  int64_t clock_rate_error_base_ = INT64_MIN;
  int64_t apts_error_start_timestamp_ = INT64_MIN;

  DISALLOW_COPY_AND_ASSIGN(AvSyncVideo);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_AV_SYNC_VIDEO_H_
