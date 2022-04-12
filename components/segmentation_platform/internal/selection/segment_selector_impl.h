// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SELECTOR_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SELECTOR_IMPL_H_

#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/internal/selection/segment_result_provider.h"
#include "components/segmentation_platform/internal/selection/segment_selector.h"
#include "components/segmentation_platform/public/segment_selection_result.h"

class PrefService;

namespace base {
class Clock;
}  // namespace base

namespace segmentation_platform {

struct Config;
class ExecutionService;
class DefaultModelManager;
class SegmentationResultPrefs;
class SignalStorageConfig;

class SegmentSelectorImpl : public SegmentSelector {
 public:
  SegmentSelectorImpl(SegmentInfoDatabase* segment_database,
                      SignalStorageConfig* signal_storage_config,
                      PrefService* pref_service,
                      const Config* config,
                      base::Clock* clock,
                      const PlatformOptions& platform_options,
                      DefaultModelManager* default_model_manager);

  SegmentSelectorImpl(SegmentInfoDatabase* segment_database,
                      SignalStorageConfig* signal_storage_config,
                      std::unique_ptr<SegmentationResultPrefs> prefs,
                      const Config* config,
                      base::Clock* clock,
                      const PlatformOptions& platform_options,
                      DefaultModelManager* default_model_manager);

  ~SegmentSelectorImpl() override;

  // SegmentSelector overrides.
  void OnPlatformInitialized(ExecutionService* execution_service) override;
  void GetSelectedSegment(SegmentSelectionCallback callback) override;
  SegmentSelectionResult GetCachedSegmentResult() override;

  // Helper function to update the selected segment in the prefs. Auto-extends
  // the selection if the new result is unknown.
  virtual void UpdateSelectedSegment(OptimizationTarget new_selection);

  // Called whenever a model eval completes. Runs segment selection to find the
  // best segment, and writes it to the pref.
  void OnModelExecutionCompleted(OptimizationTarget segment_id) override;

 private:
  // For testing.
  friend class SegmentSelectorTest;

  using SegmentRanks = base::flat_map<OptimizationTarget, int>;

  // Determines whether segment selection can be run based on whether all
  // segments have met signal collection requirement, have valid results, and
  // segment selection TTL has expired.
  bool CanComputeSegmentSelection();

  // Gets ranks for each segment from SegmentResultProvider, and then computes
  // segment selection.
  void GetRankForNextSegment(std::unique_ptr<SegmentRanks> ranks);

  // Callback used to get result from SegmentResultProvider for each segment.
  void OnGetResultForSegmentSelection(
      std::unique_ptr<SegmentRanks> ranks,
      OptimizationTarget current_segment_id,
      std::unique_ptr<SegmentResultProvider::SegmentResult> result);

  // Loops through all segments, performs discrete mapping, honors finch
  // supplied tie-breakers, TTL, inertia etc, and finds the highest rank.
  // Ignores the segments that have no results.
  OptimizationTarget FindBestSegment(const SegmentRanks& segment_scores);

  std::unique_ptr<SegmentResultProvider> segment_result_provider_;

  // Helper class to read/write results to the prefs.
  std::unique_ptr<SegmentationResultPrefs> result_prefs_;

  // The database storing metadata and results.
  const raw_ptr<SegmentInfoDatabase> segment_database_;

  // The database to determine whether the signal storage requirements are met.
  const raw_ptr<SignalStorageConfig> signal_storage_config_;

  // The default model manager is used for the default model fallbacks.
  const raw_ptr<DefaultModelManager> default_model_manager_;

  // The config for providing configuration params.
  const raw_ptr<const Config> config_;

  // The time provider.
  const raw_ptr<base::Clock> clock_;

  const PlatformOptions platform_options_;

  // Segment selection result is read from prefs on init and used for serving
  // the clients in the current session.
  SegmentSelectionResult selected_segment_last_session_;

  base::WeakPtrFactory<SegmentSelectorImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SELECTOR_IMPL_H_
