// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SELECTOR_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SELECTOR_IMPL_H_

#include <utility>
#include "base/containers/flat_map.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/internal/scheduler/execution_service.h"
#include "components/segmentation_platform/internal/selection/segment_result_provider.h"
#include "components/segmentation_platform/internal/selection/segment_selector.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/segment_selection_result.h"

class PrefService;

namespace base {
class Clock;
}  // namespace base

namespace segmentation_platform {

struct Config;
class ExperimentalGroupRecorder;
class FieldTrialRegister;
class SegmentationResultPrefs;
class SignalStorageConfig;

class SegmentSelectorImpl : public SegmentSelector {
 public:
  SegmentSelectorImpl(SegmentInfoDatabase* segment_database,
                      SignalStorageConfig* signal_storage_config,
                      PrefService* pref_service,
                      const Config* config,
                      FieldTrialRegister* field_trial_register,
                      base::Clock* clock,
                      const PlatformOptions& platform_options);

  SegmentSelectorImpl(SegmentInfoDatabase* segment_database,
                      SignalStorageConfig* signal_storage_config,
                      std::unique_ptr<SegmentationResultPrefs> prefs,
                      const Config* config,
                      FieldTrialRegister* field_trial_register,
                      base::Clock* clock,
                      const PlatformOptions& platform_options);

  ~SegmentSelectorImpl() override;

  // SegmentSelector overrides.
  void OnPlatformInitialized(ExecutionService* execution_service) override;
  void GetSelectedSegment(SegmentSelectionCallback callback) override;
  SegmentSelectionResult GetCachedSegmentResult() override;

  // Helper function to update the selected segment in the prefs. Auto-extends
  // the selection if the new result is unknown.
  virtual void UpdateSelectedSegment(SegmentId new_selection, float rank);

  // Called whenever a model eval completes. Runs segment selection to find the
  // best segment, and writes it to the pref.
  void OnModelExecutionCompleted(SegmentId segment_id) override;

  void set_segment_result_provider_for_testing(
      std::unique_ptr<SegmentResultProvider> result_provider) {
    segment_result_provider_ = std::move(result_provider);
  }

  void set_training_data_collector_for_testing(
      TrainingDataCollector* training_data_collector) {
    training_data_collector_ = training_data_collector;
  }

 private:
  // For testing.
  friend class SegmentSelectorTest;

  using SegmentRanks = base::flat_map<SegmentId, float>;

  // Determines whether segment selection can be run based on whether the
  // segment selection TTL has expired, or selection is unavailable.
  bool IsPreviousSelectionInvalid();

  // Gets scores for all segments and recomputes selection and stores the result
  // to prefs.
  void SelectSegmentAndStoreToPrefs();

  // Gets ranks for each segment from SegmentResultProvider, and then computes
  // segment selection.
  void GetRankForNextSegment(std::unique_ptr<SegmentRanks> ranks,
                             scoped_refptr<InputContext> input_context,
                             SegmentSelectionCallback callback);

  // Callback used to get result from SegmentResultProvider for each segment.
  void OnGetResultForSegmentSelection(
      std::unique_ptr<SegmentRanks> ranks,
      scoped_refptr<InputContext> input_context,
      SegmentSelectionCallback callback,
      SegmentId current_segment_id,
      std::unique_ptr<SegmentResultProvider::SegmentResult> result);

  void RecordFieldTrials() const;

  // Loops through all segments, performs discrete mapping, honors finch
  // supplied tie-breakers, TTL, inertia etc, and finds the highest rank.
  // Ignores the segments that have no results.
  std::pair<SegmentId, float> FindBestSegment(
      const SegmentRanks& segment_scores);

  // Wrapped result callback for recording metrics.
  void CallbackWrapper(base::Time start_time,
                       SegmentSelectionCallback callback,
                       const SegmentSelectionResult& result);

  std::unique_ptr<SegmentResultProvider> segment_result_provider_;

  // Helper class to read/write results to the prefs.
  std::unique_ptr<SegmentationResultPrefs> result_prefs_;

  // The database storing metadata and results.
  const raw_ptr<SegmentInfoDatabase> segment_database_;

  // The database to determine whether the signal storage requirements are met.
  const raw_ptr<SignalStorageConfig> signal_storage_config_;

  // The config for providing configuration params.
  const raw_ptr<const Config, DanglingUntriaged> config_;

  // Delegate that records selected segments to metrics.
  const raw_ptr<FieldTrialRegister> field_trial_register_;

  // Helper that records segmentation subgroups to `field_trial_register_`. Once
  // for each segment in the `config_`.
  std::vector<std::unique_ptr<ExperimentalGroupRecorder>>
      experimental_group_recorder_;

  // The time provider.
  const raw_ptr<base::Clock> clock_;

  const PlatformOptions platform_options_;

  // Segment selection result is read from prefs on init and used for serving
  // the clients in the current session. The selection could be updated if it
  // was unused by the client and a result refresh was triggered. If used by
  // client, then the result is not updated and effective onnly in the next
  // session.
  SegmentSelectionResult selected_segment_;
  bool used_result_in_current_session_ = false;

  // Pointer to the training data collector.
  raw_ptr<TrainingDataCollector, DanglingUntriaged> training_data_collector_ =
      nullptr;

  base::WeakPtrFactory<SegmentSelectorImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SELECTOR_IMPL_H_
