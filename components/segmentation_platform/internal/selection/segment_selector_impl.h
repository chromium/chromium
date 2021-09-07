// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SELECTOR_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SELECTOR_IMPL_H_

#include "components/segmentation_platform/internal/selection/segment_selector.h"

#include "base/callback_helpers.h"
#include "components/segmentation_platform/public/segment_selection_result.h"

namespace base {
class Clock;
}  // namespace base

namespace segmentation_platform {

struct Config;
class ModelExecutionScheduler;
class SegmentationResultPrefs;
class SegmentInfoDatabase;
class SignalStorageConfig;

namespace proto {
class SegmentInfo;
class SegmentationModelMetadata;
}  // namespace proto

class SegmentSelectorImpl : public SegmentSelector {
 public:
  SegmentSelectorImpl(SegmentInfoDatabase* segment_database,
                      SignalStorageConfig* signal_storage_config,
                      SegmentationResultPrefs* result_prefs,
                      Config* config,
                      base::Clock* clock);

  ~SegmentSelectorImpl() override;

  // SegmentSelector overrides.
  void Initialize(base::OnceClosure callback) override;
  void GetSelectedSegment(SegmentSelectionCallback callback) override;
  void GetSegmentScore(OptimizationTarget segment_id,
                       SingleSegmentResultCallback callback) override;
  void OnSegmentUsed(OptimizationTarget segment_id) override;

  // ModelExecutionScheduler::Observer overrides.

  // Called whenever a model eval completes. Runs segment selection to find the
  // best segment, and writes it to the pref.
  void OnModelExecutionCompleted(OptimizationTarget segment_id) override;

 private:
  // For testing.
  friend class SegmentSelectorTest;

  // Helper method to run segment selection if possible.
  void RunSegmentSelection(
      std::vector<std::pair<OptimizationTarget, proto::SegmentInfo>>
          all_segments);

  // Determines whether segment selection can be run based on whether all
  // segments have met signal collection requirement, have valid results, and
  // segment selection TTL has expired.
  bool CanComputeSegmentSelection(
      const std::vector<std::pair<OptimizationTarget, proto::SegmentInfo>>&
          all_segments);

  // Loops through all segments, performs discrete mapping, honors finch
  // supplied tie-breakers, TTL, inertia etc, and finds the highest score.
  // Ignores the segments that have no results.
  OptimizationTarget FindBestSegment(
      const std::vector<std::pair<OptimizationTarget, proto::SegmentInfo>>&
          all_segments);

  // Helper function to update the selected segment in the prefs. Auto-extends
  // the selection if the new result is unknown.
  void UpdateSelectedSegment(OptimizationTarget new_selection);

  // Callback method used during initialization to read the model results into
  // memory.
  void ReadScoresFromLastSession(
      base::OnceClosure callback,
      std::vector<std::pair<OptimizationTarget, proto::SegmentInfo>>
          all_segments);

  // Helper method to convert continuous to discrete score.
  int ConvertToDiscreteScore(OptimizationTarget segment_id,
                             const std::string& mapping_key,
                             float score,
                             const proto::SegmentationModelMetadata& metadata);

  // The database storing metadata and results.
  SegmentInfoDatabase* segment_database_;

  // The database to determine whether the signal storage requirements are met.
  SignalStorageConfig* signal_storage_config_;

  // Helper class to read/write results to the prefs.
  SegmentationResultPrefs* result_prefs_;

  // The config for providing configuration params.
  Config* config_;

  // The time provider.
  base::Clock* clock_;

  // These values are read from prefs or db on init and used for serving the
  // clients in the current session.
  SegmentSelectionResult selected_segment_last_session_;
  std::map<OptimizationTarget, float> segment_score_last_session_;

  // Whether the initialization is complete through an Initialize call.
  bool initialized_;

  base::WeakPtrFactory<SegmentSelectorImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SELECTOR_IMPL_H_
