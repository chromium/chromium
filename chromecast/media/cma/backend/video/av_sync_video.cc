// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/video/av_sync_video.h"

#include <cmath>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chromecast/base/statistics/weighted_moving_linear_regression.h"
#include "chromecast/media/audio/rate_adjuster.h"
#include "chromecast/media/cma/backend/audio_decoder_for_mixer.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_for_mixer.h"
#include "chromecast/media/cma/backend/video_decoder_for_mixer.h"

namespace chromecast {
namespace media {

namespace {

constexpr base::TimeDelta kLinearRegressionWindow = base::Seconds(20);

// Time interval between AV sync upkeeps.
constexpr base::TimeDelta kAvSyncUpkeepInterval = base::Milliseconds(16);

// Threshold where the audio and video PTS are far enough apart such that we
// want to do a hard correction.
constexpr base::TimeDelta kMaxAptsError = base::Milliseconds(50);

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

// Maximum correction rate for absolute sync offset.
const double kMaxOffsetCorrection = 2.5e-4;

// Maximum A/V sync offset that we allow without correction. Note that we still
// correct the audio playback rate to match video (to prevent the offset from
// growing) even when the offset is lower than this value.
const int64_t kMaxIgnoredOffset = 500;

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
  audio_rate_adjuster_.reset();
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
    DCHECK(audio_rate_adjuster_);
    audio_rate_adjuster_->AddError(apts_timestamp_error, new_apts_timestamp);
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

double AvSyncVideo::ChangeAudioRate(double desired_clock_rate,
                                    double error_slope,
                                    double current_error) {
  double effective_new_rate =
      backend_->audio_decoder()->SetAvSyncPlaybackRate(desired_clock_rate);
  current_audio_clock_rate_ = effective_new_rate;
  LOG(INFO) << "Update audio clock rate to " << effective_new_rate
            << "; wanted " << desired_clock_rate
            << ", error slope = " << error_slope
            << ", smoothed error = " << current_error;

  double vpts_slope;
  double e;
  if (video_pts_->EstimateSlope(&vpts_slope, &e)) {
    LOG(INFO) << "VPTS slope = " << vpts_slope << "; playback rate =~ "
              << (1.0 / vpts_slope);
  }
  return effective_new_rate;
}

void AvSyncVideo::FlushAudioPts() {
  last_apts_timestamp_ = INT64_MIN;
  // Don't reset last_apts_value_, since we still want to ignore that value for
  // the new linear regression since it may be invalid.

  RateAdjuster::Config config;
  config.linear_regression_window = kLinearRegressionWindow;
  config.max_ignored_current_error = kMaxIgnoredOffset;
  config.max_current_error_correction = kMaxOffsetCorrection;
  // Only change the clock rate if the desired rate is > 30 ppm different.
  // Reasoning: 30 ppm means that leaving the clock rate unchanged will add at
  // most 30 microseconds of additional error before the next clock rate check.
  config.min_rate_change = 3.0e-5;
  audio_rate_adjuster_ = std::make_unique<RateAdjuster>(
      config,
      base::BindRepeating(&AvSyncVideo::ChangeAudioRate,
                          base::Unretained(this)),
      current_audio_clock_rate_);
}

void AvSyncVideo::FlushVideoPts() {
  video_pts_.reset();
  // Don't reset last_vpts_value_, since we still want to ignore that value for
  // the new linear regression since it may be invalid.
}

}  // namespace media
}  // namespace chromecast
