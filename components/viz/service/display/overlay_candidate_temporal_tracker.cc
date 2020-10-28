// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_candidate_temporal_tracker.h"

namespace viz {

void OverlayCandidateTemporalTracker::Reset() {
  fps_category = kFrameRateLow;
  damage_category = kDamageLow;
  for (int i = 0; i < kNumRecords; i++) {
    damage_record[i] = 0.0f;
    tick_record[i] = base::TimeTicks();
  }
}

void OverlayCandidateTemporalTracker::CategorizeDamageRatio(
    const OverlayCandidateTemporalTracker::Config& config) {
  // This function uses member state to provide a hysteresis effect.
  if (damage_category == DamageCategory::kDamageHigh) {
    if (MeanDamageAreaRatio() < config.damage_low_threshold) {
      damage_category = DamageCategory::kDamageLow;
    }
  } else {
    if (MeanDamageAreaRatio() > config.damage_high_threshold) {
      damage_category = DamageCategory::kDamageHigh;
    }
  }
}

void OverlayCandidateTemporalTracker::CategorizeFrameRate(
    base::TimeTicks curr_tick) {
  // This function uses member state to provide a hysteresis effect.
  static constexpr int64_t kEpsilonMs = 2;
  static constexpr int64_t k60FPSMs = 16;
  static constexpr int64_t k30FPSMs = 33;
  static constexpr int64_t k15FPSMs = 66;
  // Visual depiction of the hysteresis of this function:
  //   * - Go into kFrameRateLow state.
  //   # - Go into kFrameRate30fps state.
  //   & - Go into kFrameRate60fps state.
  //
  //   Current     fps_category:
  //
  //              kFrameRateLow --&&&&&&&&#################********
  //            kFrameRate30fps --&&&&&&&&####################*****
  //            kFrameRate60fps --&&&&&&&&&&&&&&&&&&&&&&######*****
  // mean_frame_time              |^^^^^^^^^|^^^^^^^^^|^^^ ... ^^^|
  //                              10        20        30          60
  //
  auto mean_frame_time = MeanFrameMs();
  if (fps_category == FrameRateCategory::kFrameRate60fps) {
    if (mean_frame_time > (k15FPSMs - kEpsilonMs)) {
      fps_category = FrameRateCategory::kFrameRateLow;
    } else if (mean_frame_time > (k30FPSMs - kEpsilonMs)) {
      fps_category = FrameRateCategory::kFrameRate30fps;
    }
  } else if (fps_category == FrameRateCategory::kFrameRate30fps) {
    if (mean_frame_time < (k60FPSMs + kEpsilonMs)) {
      fps_category = FrameRateCategory::kFrameRate60fps;
    } else if (mean_frame_time > (k15FPSMs - kEpsilonMs)) {
      fps_category = FrameRateCategory::kFrameRateLow;
    }
  } else {
    if (mean_frame_time < (k60FPSMs + kEpsilonMs)) {
      fps_category = FrameRateCategory::kFrameRate60fps;
    } else if (mean_frame_time < (k30FPSMs + kEpsilonMs)) {
      fps_category = FrameRateCategory::kFrameRate30fps;
    }
  }
}

bool OverlayCandidateTemporalTracker::IsActivelyChanging(
    base::TimeTicks curr_tick,
    const OverlayCandidateTemporalTracker::Config& config) {
  return LastChangeMs(curr_tick) < config.max_ms_active;
}

void OverlayCandidateTemporalTracker::AddRecord(
    base::TimeTicks curr_tick,
    float damage_area_ratio,
    unsigned resource_id,
    const OverlayCandidateTemporalTracker::Config& config) {
  if (prev_resource_id != resource_id &&
      tick_record[(next_index + kNumRecords - 1) % kNumRecords] != curr_tick) {
    tick_record[next_index] = curr_tick;
    damage_record[next_index] = damage_area_ratio;
    next_index = (next_index + 1) % kNumRecords;
    prev_resource_id = resource_id;

    CategorizeFrameRate(curr_tick);
    CategorizeDamageRatio(config);
  }
  absent = false;
}

float OverlayCandidateTemporalTracker::MeanDamageAreaRatio() const {
  float sum = 0.0f;
  for (auto& damage : damage_record) {
    sum += damage;
  }
  return sum / kNumRecords;
}

int64_t OverlayCandidateTemporalTracker::LastChangeMs(
    base::TimeTicks curr_tick) const {
  int64_t diff_now_prev =
      (curr_tick - tick_record[((next_index - 1) + kNumRecords) % kNumRecords])
          .InMilliseconds();

  return diff_now_prev;
}

int64_t OverlayCandidateTemporalTracker::MeanFrameMs() const {
  int64_t mean_time = 0;
  for (int i = 0; i < kNumRecords; i++) {
    if (i != next_index) {
      mean_time +=
          (tick_record[i] - tick_record[((i - 1) + kNumRecords) % kNumRecords])
              .InMilliseconds();
    }
  }
  return mean_time / (kNumRecords - 1);
}

bool OverlayCandidateTemporalTracker::IsAbsent() {
  bool ret = absent;
  absent = true;
  return ret;
}

}  // namespace viz
