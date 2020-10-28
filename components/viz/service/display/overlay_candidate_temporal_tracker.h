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
  // The |Config| contains values that are derived as part of a heuristic. This
  // |Config| allows for the potential of platform specific variations or
  // experiments.
  class VIZ_SERVICE_EXPORT Config {
   public:
    // Mean damage above |kDamageHighThreshold| is considered significant. An
    // example of this is a youtube video.
    float damage_high_threshold = 0.3f;
    // Mean damage below |kDamageLowThreshold| is considered insignificant. An
    // example of this is a blinking cursor.
    float damage_low_threshold = 0.1f;

    // |sMaxMsActive| is a millisecond the cutoff for when an unchanging
    // candidate is considered to be inactive. see 'IsActivelyChanging()'
    int64_t max_ms_active = 100;
  };

  enum FrameRateCategory { kFrameRate60fps, kFrameRate30fps, kFrameRateLow };
  enum DamageCategory { kDamageHigh, kDamageLow };

  // This function returns true when the time since the |resource_id| changed
  // exceeds a specific threshold.
  bool IsActivelyChanging(base::TimeTicks curr_tick, const Config& config);

  // This function adds a new record to the tracker if the |resource_id| has
  // changed since last update.
  void AddRecord(base::TimeTicks curr_tick,
                 float damage_area_ratio,
                 unsigned resource_id,
                 const Config& config);

  // This function returns true when this tracker's 'AddRecord' was not called
  // in the previous frame. We require this behavior in order to know when an
  // overlay candidate is no longer present since we are tracking across frames.
  bool IsAbsent();

  void Reset();

  FrameRateCategory GetFPSCategory() const { return fps_category; }
  bool HasSignificantDamage() { return damage_category == kDamageHigh; }

  // The functions and data below are used internally but also can be used for
  // diagnosis and testing.
  int64_t MeanFrameMs() const;
  float MeanDamageAreaRatio() const;
  int64_t LastChangeMs(base::TimeTicks curr_tick) const;

  // Categorization can happen over a series of |KNumRecords| frames.
  // The more records the smoother the categorization but the worse the latency.
  static constexpr int kNumRecords = 6;

 private:
  void CategorizeFrameRate(base::TimeTicks curr_tick);
  void CategorizeDamageRatio(const Config& config);

  unsigned prev_resource_id = kInvalidResourceId;
  FrameRateCategory fps_category = kFrameRateLow;
  DamageCategory damage_category = kDamageLow;
  // Next empty slot index
  int next_index = 0;

  // The state of this absent bool is as follows:
  // In the normal flow 'IsAbsent()' is tested which sets |absent| = true. Then
  // the 'AddRecord() sets it false again in the same frame.
  // When this tracker no longer corresponds to an overlay candidate the
  // 'IsAbsent()' is tested which sets |absent| = true but on the next frame
  // 'IsAbsent()' returns true  because |absent| was never reset to false. This
  // indicating this tracker should be removed.
  bool absent = false;
  base::TimeTicks tick_record[kNumRecords] = {};
  float damage_record[kNumRecords] = {};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_TEMPORAL_TRACKER_H_
