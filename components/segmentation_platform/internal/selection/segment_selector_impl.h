// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SELECTOR_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SELECTOR_IMPL_H_

#include "components/segmentation_platform/internal/selection/segment_selector.h"

#include "base/callback_helpers.h"
#include "components/segmentation_platform/public/segment_selection_result.h"

namespace segmentation_platform {

struct Config;
class ModelExecutionScheduler;
class SegmentationResultPrefs;
class SegmentInfoDatabase;

namespace proto {
class SegmentInfo;
class SegmentationModelMetadata;
}  // namespace proto

class SegmentSelectorImpl : public SegmentSelector {
 public:
  SegmentSelectorImpl(SegmentInfoDatabase* segment_database,
                      SegmentationResultPrefs* result_prefs,
                      Config* config);

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

  // Loops through all segments, performs discrete mapping, honors finch
  // supplied tie-breakers, TTL, inertia etc, and finds the highest score.
  // Ignores the segments that have no results.
  void FindBestSegment(
      std::vector<std::pair<OptimizationTarget, proto::SegmentInfo>>
          all_segments);

  // Helper function to update the selected segment in the prefs, if the new
  // selection passes the criteria for segment selection TTL, and segment
  // selection inertia.
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

  // Helper class to read/write results to the prefs.
  SegmentationResultPrefs* result_prefs_;

  // The config for providing configuration params.
  Config* config_;

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
