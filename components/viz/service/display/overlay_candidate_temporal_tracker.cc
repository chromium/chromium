// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_candidate_temporal_tracker.h"

#include <algorithm>

namespace viz {

OverlayCandidateTemporalTracker::OverlayCandidateTemporalTracker() = default;

void OverlayCandidateTemporalTracker::Reset() {
  num_samples_ = 0;
}

int OverlayCandidateTemporalTracker::GetModeledPowerGain(
    uint64_t curr_frame,
    const OverlayCandidateTemporalTracker::Config& config,
    int display_area,
    bool is_fullscreen) const {
  // Model of proportional power gained by hw overlay promotion.

  if (is_fullscreen) {
    // Fullscreen removes the primary plane and saves ~2x the power of normal
    // overlays and has no overhead as there is only one overlay present.
    return static_cast<int>(ratio_rate_category_ * display_area * 2.f);
  }

  return static_cast<int>(
      (ratio_rate_category_ - config.damage_rate_threshold) * display_area);
}

void OverlayCandidateTemporalTracker::CategorizeDamageRatioRate(
    uint64_t curr_frame,
    const OverlayCandidateTemporalTracker::Config& config) {
  float mean_ratio_rate = MeanFrameRatioRate(config);
  // Simple implementation of hysteresis. If the value is far enough away from
  // the stored value it will be updated.
  // Hysteresis range is derived from the maximum value impact of a single
  // sample on the total mean.
  const float damage_rate_hysteresis_range = 1.0f / config.max_num_frames_avg;
  if (std::abs(mean_ratio_rate - ratio_rate_category_) >=
      damage_rate_hysteresis_range) {
    ratio_rate_category_ = mean_ratio_rate;
  }
}

bool OverlayCandidateTemporalTracker::IsActivelyChanging(
    uint64_t curr_frame,
    const OverlayCandidateTemporalTracker::Config& config) const {
  return LastChangeFrameCount(curr_frame) <
         static_cast<uint64_t>(config.max_num_frames_avg);
}

float OverlayCandidateTemporalTracker::MeanFrameRatioRate(
    const OverlayCandidateTemporalTracker::Config& config) const {
  return damage_record_avg_;
}

void OverlayCandidateTemporalTracker::AddRecord(
    uint64_t curr_frame,
    float damage_area_ratio,
    ResourceId resource_id,
    const OverlayCandidateTemporalTracker::Config& config,
    bool force_resource_update) {
  if ((prev_resource_id_ != resource_id || force_resource_update) &&
      frame_record_prev_ != curr_frame) {
    float damage_area_ratio_rate =
        damage_area_ratio / (curr_frame - frame_record_prev_);

    frame_record_prev_ = curr_frame;
    prev_resource_id_ = resource_id;
    // Construct a new average from the previous average and this new sample.
    // This computation is mathematically equivalent to the mean formula however
    // we need store each of the previous |num_samples_|.
    damage_record_avg_ =
        ((damage_record_avg_ * num_samples_) + damage_area_ratio_rate) /
        (num_samples_ + 1);

    // After a fixed number of samples we cap the divisor and transform the
    // average into an exponential averaging function. Unlike the mean formula,
    // the exponential smoothing formula will always remain sensitive to recent
    // sample data.
    num_samples_ = std::min(config.max_num_frames_avg, num_samples_ + 1);

    CategorizeDamageRatioRate(curr_frame, config);
  }
  absent_ = false;
}

uint64_t OverlayCandidateTemporalTracker::LastChangeFrameCount(
    uint64_t curr_frame) const {
  return curr_frame - frame_record_prev_;
}

bool OverlayCandidateTemporalTracker::IsAbsent() {
  return std::exchange(absent_, true);
}

}  // namespace viz
