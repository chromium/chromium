// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_candidate_temporal_tracker.h"

namespace viz {

OverlayCandidateTemporalTracker::OverlayCandidateTemporalTracker() = default;

void OverlayCandidateTemporalTracker::Reset() {
  ratio_rate_category = 0;
}

int OverlayCandidateTemporalTracker::GetModeledPowerGain(
    uint64_t curr_frame,
    const OverlayCandidateTemporalTracker::Config& config,
    int display_area) {
  // Model of proportional power gained by hw overlay promotion.
  return static_cast<int>((ratio_rate_category - config.damage_rate_threshold) *
                          display_area);
}

void OverlayCandidateTemporalTracker::CategorizeDamageRatioRate(
    uint64_t curr_frame,
    const OverlayCandidateTemporalTracker::Config& config) {
  float mean_ratio_rate = MeanFrameRatioRate(config);
  // Simple implementation of hysteresis. If the value is far enough away from
  // the stored value it will be updated.
  if (std::abs(mean_ratio_rate - ratio_rate_category) >=
      config.damage_rate_hysteresis_range) {
    ratio_rate_category = mean_ratio_rate;
  }
}

bool OverlayCandidateTemporalTracker::IsActivelyChanging(
    uint64_t curr_frame,
    const OverlayCandidateTemporalTracker::Config& config) const {
  return LastChangeFrameCount(curr_frame) < config.max_frames_inactive;
}

void OverlayCandidateTemporalTracker::AddRecord(
    uint64_t curr_frame,
    float damage_area_ratio,
    ResourceId resource_id,
    const OverlayCandidateTemporalTracker::Config& config,
    bool force_resource_update) {
  if ((prev_resource_id != resource_id || force_resource_update) &&
      frame_record[(next_index + kNumRecords - 1) % kNumRecords] !=
          curr_frame) {
    frame_record[next_index] = curr_frame;
    damage_record[next_index] = damage_area_ratio;
    next_index = (next_index + 1) % kNumRecords;
    prev_resource_id = resource_id;

    CategorizeDamageRatioRate(curr_frame, config);
  }
  absent = false;
}

uint64_t OverlayCandidateTemporalTracker::LastChangeFrameCount(
    uint64_t curr_frame) const {
  uint64_t diff_now_prev =
      (curr_frame -
       frame_record[((next_index - 1) + kNumRecords) % kNumRecords]);

  return diff_now_prev;
}

float OverlayCandidateTemporalTracker::MeanFrameRatioRate(
    const OverlayCandidateTemporalTracker::Config& config) const {
  float mean_ratio_rate = 0.f;
  int num_records = (kNumRecords - 1);
  // We are concerned with the steady state of damage ratio rate.
  // A specific interruption (paused video, stopped accelerated ink) is
  // categorized by 'IsActivelyChanging' and is intentionally excluded here by
  // |skip_single_interruption|.
  bool skip_single_interruption = true;
  for (int i = 0; i < kNumRecords; i++) {
    if (i != next_index) {
      uint64_t diff_frames =
          (frame_record[i] -
           frame_record[((i - 1) + kNumRecords) % kNumRecords]);
      if (skip_single_interruption &&
          diff_frames > config.max_frames_inactive) {
        skip_single_interruption = false;
        num_records--;
      } else if (diff_frames != 0) {
        float damage_ratio = damage_record[i];
        mean_ratio_rate += damage_ratio / diff_frames;
      }
    }
  }

  return mean_ratio_rate / num_records;
}

bool OverlayCandidateTemporalTracker::IsAbsent() {
  bool ret = absent;
  absent = true;
  return ret;
}

}  // namespace viz
