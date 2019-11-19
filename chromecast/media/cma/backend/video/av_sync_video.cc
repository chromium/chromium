// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/video/av_sync_video.h"

#include <cmath>

#include "base/logging.h"
#include "base/numerics/ranges.h"
#include "base/time/time.h"
#include "chromecast/base/statistics/weighted_moving_linear_regression.h"
#include "chromecast/media/cma/backend/audio_decoder_for_mixer.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_for_mixer.h"
#include "chromecast/media/cma/backend/video_decoder_for_mixer.h"

namespace chromecast {
namespace media {

namespace {

constexpr base::TimeDelta kLinearRegressionWindow =
    base::TimeDelta::FromSeconds(20);

// Time interval between AV sync upkeeps.
constexpr base::TimeDelta kAvSyncUpkeepInterval =
    base::TimeDelta::FromMilliseconds(16);

// Threshold where the audio and video PTS are far enough apart such that we
// want to do a hard correction.
constexpr base::TimeDelta kMaxAptsError = base::TimeDelta::FromMilliseconds(50);

// Minimum samples of video PTS before we start doing A/V sync.
const int kMinVideoPtsSamples = 60;

// This is the threshold for which we consider the rate of playback variation
// to be valid. If we measure a rate of playback variation worse than this, we
// consider the linear regression measurement invalid, we flush the linear
// regression and let AvSync collect samples all over again.
const double kExpectedSlopeVariance = 0.005;

// We don't AV sync content with frame rate less than this. This low framerate
// indicates that the content happens to be audio-centric, with a dummy video
// stream.
const int kAvSyncFpsThreshold = 10;

// How often we change the audio clock rate for AV sync.
constexpr base::TimeDelta kRateChangeInterval = base::TimeDelta::FromSeconds(1);

// Maximum correction rate for absolute sync offset.
const double kMaxOffsetCorrection = 2.5e-4;

// Maximum A/V sync offset that we allow without correction. Note that we still
// correct the audio playback rate to match video (to prevent the offset from
// growing) even when the offset is lower than this value.
const int64_t kMaxIgnoredOffset = 1000;

}  // namespace

std::unique_ptr<AvSync> AvSync::Create(
    MediaPipelineBackendForMixer* const backend) {
  return std::make_unique<AvSyncVideo>(backend);
}

AvSyncVideo::AvSyncVideo(MediaPipelineBackendForMixer* const backend)
    : backend_(backend) {
  DCHECK(backend_);
}

AvSyncVideo::~AvSyncVideo() = default;

void AvSyncVideo::NotifyStart(int64_t timestamp, int64_t pts) {
  LOG(INFO) << __func__;
  playback_start_timestamp_ = backend_->MonotonicClockNow();
  current_media_playback_rate_ = 1.0;
  current_audio_clock_rate_ = 1.0;
  backend_->video_decoder()->SetPlaybackRate(current_media_playback_rate_);

  StartAvSync();
}

void AvSyncVideo::NotifyStop() {
  LOG(INFO) << __func__;
  StopAvSync();
}

void AvSyncVideo::NotifyPause() {
  LOG(INFO) << __func__;
  StopAvSync();
  playback_start_timestamp_ = INT64_MAX;
}

void AvSyncVideo::NotifyResume() {
  LOG(INFO) << __func__;
  playback_start_timestamp_ = backend_->MonotonicClockNow();
  StartAvSync();
}

void AvSyncVideo::NotifyPlaybackRateChange(float rate) {
  DCHECK(backend_->video_decoder());
  DCHECK(backend_->audio_decoder());

  current_media_playback_rate_ = rate;
  backend_->video_decoder()->SetPlaybackRate(current_media_playback_rate_);

  FlushAudioPts();
  FlushVideoPts();

  LOG(INFO) << __func__
            << " current_media_playback_rate_=" << current_media_playback_rate_
            << " current_audio_clock_rate_=" << current_audio_clock_rate_;
}

void AvSyncVideo::StartAvSync() {
  FlushAudioPts();
  FlushVideoPts();

  upkeep_av_sync_timer_.Start(FROM_HERE, kAvSyncUpkeepInterval, this,
                              &AvSyncVideo::UpkeepAvSync);
}

void AvSyncVideo::StopAvSync() {
  upkeep_av_sync_timer_.Stop();
}

void AvSyncVideo::UpkeepAvSync() {
  if (!backend_->video_decoder() || !backend_->audio_decoder()) {
    return;
  }

  if (!VptsUpkeep()) {
    return;
  }

  int64_t new_raw_apts = 0;
  int64_t new_apts_timestamp = 0;
  if (!backend_->audio_decoder()->GetTimestampedPts(&new_apts_timestamp,
                                                    &new_raw_apts) ||
      new_apts_timestamp <= playback_start_timestamp_ ||
      new_raw_apts == last_apts_value_) {
    return;
  }
  last_apts_value_ = new_raw_apts;

  DCHECK(video_pts_);

  int64_t desired_apts_timestamp;
  double vpts_slope;
  double error;
  if (video_pts_->EstimateSlope(&vpts_slope, &error) &&
      std::abs(vpts_slope - 1.0 / current_media_playback_rate_) >
          kExpectedSlopeVariance) {
    // VPTS slope is bad. This can be because the video is actually playing out
    // at the wrong rate (eg when video playback can't keep up and is too slow),
    // or could be due to bad VPTS data (eg after resume, from old timestamps
    // before pause). We assume the most recent VPTS sample is OK (so far this
    // has always been true) and check if we need to do a hard correction to
    // account for cases where the video is actually playing at the wrong rate
    // before flushing the VPTS regression.
    LOG(ERROR) << "Calculated bad vpts_slope " << vpts_slope
               << " corresponding to playback rate =~ " << (1.0 / vpts_slope)
               << ". Expected playback rate = " << current_media_playback_rate_;

    int64_t last_vpts = video_pts_->samples().back().x;
    int64_t last_vpts_timestamp = video_pts_->samples().back().y;
    desired_apts_timestamp =
        last_vpts_timestamp +
        (new_raw_apts - last_vpts) / current_media_playback_rate_;
    FlushVideoPts();
  } else if (!video_pts_->EstimateY(new_raw_apts, &desired_apts_timestamp,
                                    &error)) {
    LOG(INFO) << "Failed to estimate desired APTS timestamp";
    return;
  }

  // If error is positive, the audio is playing later than it should be.
  int64_t apts_timestamp_error = new_apts_timestamp - desired_apts_timestamp;
  if (std::abs(apts_timestamp_error) > kMaxAptsError.InMicroseconds() ||
      new_apts_timestamp < last_apts_timestamp_) {
    if (new_apts_timestamp < last_apts_timestamp_) {
      LOG(INFO) << "Audio timestamp moved backward";
    }
    LOG(INFO) << "Hard correction; APTS = " << new_raw_apts
              << ", ts = " << new_apts_timestamp
              << ", desired = " << desired_apts_timestamp
              << ", error = " << apts_timestamp_error;
    HardCorrection(new_raw_apts, desired_apts_timestamp);
    return;
  }
  if (video_pts_) {
    // Only do audio rate upkeep if the VPTS data was OK (ie, no bad slope).
    AudioRateUpkeep(apts_timestamp_error, new_apts_timestamp);
  }
}

bool AvSyncVideo::VptsUpkeep() {
  if (!video_pts_) {
    video_pts_ = std::make_unique<WeightedMovingLinearRegression>(
        kLinearRegressionWindow.InMicroseconds());
  }

  int64_t new_raw_vpts = 0;
  int64_t new_vpts_timestamp = 0;
  if (backend_->video_decoder()->GetCurrentPts(&new_vpts_timestamp,
                                               &new_raw_vpts) &&
      new_vpts_timestamp > playback_start_timestamp_ &&
      new_raw_vpts != last_vpts_value_) {
    video_pts_->AddSample(new_raw_vpts, new_vpts_timestamp, 1.0);
    last_vpts_value_ = new_raw_vpts;
  }

  if (video_pts_->num_samples() < kMinVideoPtsSamples) {
    return false;
  }

  return (GetVideoFrameRate() >= kAvSyncFpsThreshold);
}

int AvSyncVideo::GetVideoFrameRate() {
  DCHECK(video_pts_);
  DCHECK_GE(video_pts_->num_samples(), 2u);
  int64_t duration =
      video_pts_->samples().back().x - video_pts_->samples().front().x;
  return std::round(static_cast<double>(video_pts_->num_samples()) * 1000000 /
                    duration);
}

void AvSyncVideo::HardCorrection(int64_t apts, int64_t desired_apts_timestamp) {
  backend_->audio_decoder()->RestartPlaybackAt(apts, desired_apts_timestamp);
  FlushAudioPts();
}

void AvSyncVideo::AudioRateUpkeep(int64_t error, int64_t timestamp) {
  if (!apts_error_) {
    apts_error_ = std::make_unique<WeightedMovingLinearRegression>(
        kLinearRegressionWindow.InMicroseconds());
    clock_rate_start_timestamp_ = timestamp;
    clock_rate_error_base_ = 0;
    apts_error_start_timestamp_ = timestamp;
  }

  int64_t x = timestamp - apts_error_start_timestamp_;

  // Error is positive if audio is playing out too late.
  // We play out |current_audio_clock_rate_| seconds of audio per second of
  // actual time. In the last N seconds, if the clock rate was 1.0 we would have
  // played (1.0 - clock_rate) * N more seconds of audio, so the current
  // buffer would have played out that much sooner (reducing its error by that
  // amount). We also need to take into account any existing error when we
  // last changed the clock rate.
  int64_t time_at_current_clock_rate = timestamp - clock_rate_start_timestamp_;
  int64_t correction =
      clock_rate_error_base_ -
      (1.0 - current_audio_clock_rate_) * time_at_current_clock_rate;
  int64_t corrected_error = error + correction;
  apts_error_->AddSample(x, corrected_error, 1.0);

  if (time_at_current_clock_rate < kRateChangeInterval.InMicroseconds()) {
    // Don't change clock rate too frequently.
    return;
  }

  int64_t offset;
  double slope;
  double e;
  if (!apts_error_->EstimateY(x, &offset, &e) ||
      !apts_error_->EstimateSlope(&slope, &e)) {
    return;
  }

  int64_t smoothed_error = error + (offset - corrected_error);

  // If slope is positive, a clock rate of 1.0 is too slow (audio is playing
  // progressively later than desired). We wanted to play slope*N seconds more
  // audio during N seconds that would have been played at clock rate 1.0.
  // Therefore the actual clock rate should be (1.0 + slope).
  // However, we also want to correct for any existing offset. We correct so
  // that the error should reduce to 0 by the next rate change interval;
  // however the rate change is capped to prevent very fast slewing.
  double offset_correction = 0.0;
  if (std::abs(smoothed_error) >= kMaxIgnoredOffset) {
    offset_correction = static_cast<double>(smoothed_error) /
                        kRateChangeInterval.InMicroseconds();
    offset_correction = base::ClampToRange(
        offset_correction, -kMaxOffsetCorrection, kMaxOffsetCorrection);
  }
  double new_rate = (1.0 + slope) + offset_correction;

  double effective_new_rate =
      backend_->audio_decoder()->SetAvSyncPlaybackRate(new_rate);
  if (effective_new_rate != current_audio_clock_rate_) {
    current_audio_clock_rate_ = effective_new_rate;
    LOG(INFO) << "Update audio clock rate to " << effective_new_rate
              << "; wanted " << new_rate << ", error slope = " << slope;
    LOG(INFO) << "Offset = " << offset
              << ", smoothed error = " << smoothed_error
              << ", base rate = " << (1.0 + slope)
              << ", offset correction = " << offset_correction;

    double vpts_slope;
    if (video_pts_->EstimateSlope(&vpts_slope, &e)) {
      LOG(INFO) << "VPTS slope = " << vpts_slope << "; playback rate =~ "
                << (1.0 / vpts_slope);
    }
  }

  clock_rate_start_timestamp_ = timestamp;
  clock_rate_error_base_ = correction;
}

void AvSyncVideo::FlushAudioPts() {
  apts_error_.reset();
  last_apts_timestamp_ = INT64_MIN;
  // Don't reset last_apts_value_, since we still want to ignore that value for
  // the new linear regression since it may be invalid.
}

void AvSyncVideo::FlushVideoPts() {
  video_pts_.reset();
  // Don't reset last_vpts_value_, since we still want to ignore that value for
  // the new linear regression since it may be invalid.
}

}  // namespace media
}  // namespace chromecast
