// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_TEMPORAL_TRACKER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_TEMPORAL_TRACKER_H_

#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {
// Overlay selection is extremely important for optimal power and performance.
// The |OverlayCandidateTemporalTracker| class provides a way to temporally
// track overlay candidate properties and to categorize them. This tracker
// operates on highly opaque input; it only understands resource id (changes)
// and damage ratios. The hysteresis in categorization is intentional and its
// purpose is to temporally stabilize the result.
class VIZ_SERVICE_EXPORT OverlayCandidateTemporalTracker {
 public:
  OverlayCandidateTemporalTracker();

  // The |Config| contains values that are derived as part of a heuristic. This
  // |Config| allows for the potential of platform specific variations or
  // experiments.
  class VIZ_SERVICE_EXPORT Config {
   public:
    // This is the only heuristic input constant to our power model. It
    // effectively determines the threshold for positive power gain.
    float damage_rate_threshold = 0.3f;

    // The hysteresis value for damage rate is kept constant within the range of
    // |damage_rate_hysteresis_range|
    float damage_rate_hysteresis_range = 0.15f;

    // |max_frames_inactive| is the frame count cutoff for when an unchanging
    // candidate is considered to be inactive. see 'IsActivelyChanging()'
    uint64_t max_frames_inactive = 6;
  };

  // This function returns an opaque but comparable value representing the
  // power improvement by promoting the tracked candidate to an overlay.
  // Negative values indicate that the model suggests a power degradation if the
  // candidate is promoted to overlay.
  int GetModeledPowerGain(uint64_t curr_frame,
                          const OverlayCandidateTemporalTracker::Config& config,
                          int display_area);

  // This function returns true when the time since the |resource_id| changed
  // exceeds a specific threshold.
  bool IsActivelyChanging(uint64_t curr_frame, const Config& config) const;

  void Reset();

  // This function adds a new record to the tracker if the |resource_id| has
  // changed since last update.
  // The |force_resource_update| flag has been added for the case when the
  // resource has been updated but the |resource_id| has not changed. The case
  // for when this occurs is a low latency surface (ink). Fortunately, we can
  // use surface damage to ascertain when these surfaces have changed despite
  // the |resource_id| remaining constant.
  void AddRecord(uint64_t curr_frame,
                 float damage_area_ratio,
                 ResourceId resource_id,
                 const Config& config,
                 bool force_resource_update = false);

  // This function returns true when this tracker's 'AddRecord' was not called
  // in the previous frame. We require this behavior in order to know when an
  // overlay candidate is no longer present since we are tracking across frames.
  bool IsAbsent();

  // The functions and data below are used internally but also can be used for
  // diagnosis and testing.
  float MeanFrameRatioRate(const Config& config) const;
  float GetDamageRatioRate() const { return ratio_rate_category; }
  uint64_t LastChangeFrameCount(uint64_t curr_frame) const;
  // Categorization can happen over a series of |KNumRecords| frames.
  // The more records the smoother the categorization but the worse the latency.
  static constexpr int kNumRecords = 6;

 private:
  void CategorizeDamageRatioRate(uint64_t curr_frame, const Config& config);
  ResourceId prev_resource_id = kInvalidResourceId;

  float ratio_rate_category = 0.0f;
  // Next empty slot index. Used for circular samples buffer.
  int next_index = 0;

  // The state of this absent bool is as follows:
  // In the normal flow 'IsAbsent()' is tested which sets |absent| = true. Then
  // the 'AddRecord() sets it false again in the same frame.
  // When this tracker no longer corresponds to an overlay candidate the
  // 'IsAbsent()' is tested which sets |absent| = true but on the next frame
  // 'IsAbsent()' returns true  because |absent| was never reset to false. This
  // indicating this tracker should be removed.
  bool absent = false;
  uint64_t frame_record[kNumRecords] = {};
  float damage_record[kNumRecords] = {};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_TEMPORAL_TRACKER_H_
