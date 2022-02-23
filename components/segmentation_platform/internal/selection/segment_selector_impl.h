// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SELECTOR_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SELECTOR_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "components/segmentation_platform/internal/selection/segment_selector.h"

#include "base/callback_helpers.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/public/segment_selection_result.h"

namespace base {
class Clock;
}  // namespace base

namespace segmentation_platform {

struct Config;
class ModelExecutionScheduler;
class SegmentationResultPrefs;
class SignalStorageConfig;

class SegmentSelectorImpl : public SegmentSelector {
 public:
  SegmentSelectorImpl(SegmentInfoDatabase* segment_database,
                      SignalStorageConfig* signal_storage_config,
                      SegmentationResultPrefs* result_prefs,
                      const Config* config,
                      base::Clock* clock,
                      const PlatformOptions& platform_options);

  ~SegmentSelectorImpl() override;

  // SegmentSelector overrides.
  void GetSelectedSegment(SegmentSelectionCallback callback) override;
  SegmentSelectionResult GetCachedSegmentResult() override;

  // Helper function to update the selected segment in the prefs. Auto-extends
  // the selection if the new result is unknown.
  virtual void UpdateSelectedSegment(OptimizationTarget new_selection);

  // ModelExecutionScheduler::Observer overrides.

  // Called whenever a model eval completes. Runs segment selection to find the
  // best segment, and writes it to the pref.
  void OnModelExecutionCompleted(OptimizationTarget segment_id) override;

 private:
  // For testing.
  friend class SegmentSelectorTest;

  // Helper method to run segment selection if possible.
  void RunSegmentSelection(
      std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> all_segments);

  // Determines whether segment selection can be run based on whether all
  // segments have met signal collection requirement, have valid results, and
  // segment selection TTL has expired.
  bool CanComputeSegmentSelection(
      const SegmentInfoDatabase::SegmentInfoList& all_segments);

  // Loops through all segments, performs discrete mapping, honors finch
  // supplied tie-breakers, TTL, inertia etc, and finds the highest score.
  // Ignores the segments that have no results.
  OptimizationTarget FindBestSegment(
      const SegmentInfoDatabase::SegmentInfoList& all_segments);

  // The database storing metadata and results.
  raw_ptr<SegmentInfoDatabase> segment_database_;

  // The database to determine whether the signal storage requirements are met.
  raw_ptr<SignalStorageConfig> signal_storage_config_;

  // Helper class to read/write results to the prefs.
  raw_ptr<SegmentationResultPrefs> result_prefs_;

  // The config for providing configuration params.
  raw_ptr<const Config> config_;

  // The time provider.
  raw_ptr<base::Clock> clock_;

  const PlatformOptions platform_options_;

  // Segment selection result is read from prefs on init and used for serving
  // the clients in the current session.
  SegmentSelectionResult selected_segment_last_session_;

  base::WeakPtrFactory<SegmentSelectorImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SELECTOR_IMPL_H_
