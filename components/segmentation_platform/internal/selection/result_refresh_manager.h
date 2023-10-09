// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_RESULT_REFRESH_MANAGER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_RESULT_REFRESH_MANAGER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "components/segmentation_platform/internal/database/cached_result_writer.h"
#include "components/segmentation_platform/internal/scheduler/execution_service.h"
#include "components/segmentation_platform/internal/selection/segment_result_provider.h"
#include "components/segmentation_platform/public/result.h"

namespace segmentation_platform {
struct Config;
class SegmentResultProvider;

// ResultRefreshManager runs on every startup, ensuring that model results are
// valid both in database and prefs. Responsible for two things.
// 1. Getting model result from database or by running the model and updating
// datab.
// 2. Call to update prefs with the latest model result, if prefs results are
// invalid.
class ResultRefreshManager {
 public:
  ResultRefreshManager(const ConfigHolder* config_holder,
                       CachedResultWriter* cached_result_writer,
                       const PlatformOptions& platform_options);

  ~ResultRefreshManager();

  // Disallow copy/assign.
  ResultRefreshManager(ResultRefreshManager&) = delete;
  ResultRefreshManager& operator=(ResultRefreshManager&) = delete;

  // This method initialises the class with result providers.
  void Initialize(std::map<std::string, std::unique_ptr<SegmentResultProvider>>
                      result_providers,
                  ExecutionService* execution_service);

  // This refreshes model results. At startup, it refreshes model results after
  // `kModelInitializationTimeoutMs` delay. It does the following when result
  // for a model is required.
  // 1. Try to get model result from database.
  // 2. If present in database, update it in prefs if prefs result is invalid or
  // expired.
  // 3. Else, try to get result by running the model and also save computed
  // scores in db and update it in prefs if prefs result is invalid or
  // expired.
  void RefreshModelResults(bool is_startup);

  // This is triggered when model info is updated. This ensures model execution,
  // updating prefs and database if required on model update. If delay isn't
  // executed, it won't do anything and wait for RefreshModelResult to execute
  // the model.
  void OnModelUpdated(proto::SegmentInfo* segment_info);

 private:
  // Tells about the state of delay before model execution starts.
  enum DelayState {
    DELAY_NOT_HIT = 0,   // Delay isn't introduced yet.
    DELAY_EXECUTED = 1,  // Delay is executed completely.
  };

  // Method that internally execute all the models one by one and get result for
  // them.
  void RefreshModelResultsInternal();

  // Gives result for the model based on `run_model`. If `run_model` is false,
  // tries to get the result from database, else tries to get the result by
  // executing model. It also saves to the result to database after model
  // execution.
  void GetCachedResultOrRunModel(const Config* config);

  void OnGetCachedResultOrRunModel(
      SegmentResultProvider* segment_result_provider,
      const Config* config,
      std::unique_ptr<SegmentResultProvider::SegmentResult> result);

  // Configs for all registered clients.
  const raw_ptr<const ConfigHolder> config_holder_;

  // Stores `SegmentResultProvider` for all clients.
  std::map<std::string, std::unique_ptr<SegmentResultProvider>>
      result_providers_;

  // Delegate to write results for all clients to prefs if previous results are
  // not present or invalid.
  const raw_ptr<CachedResultWriter, DanglingUntriaged> cached_result_writer_;

  // Execution Service
  raw_ptr<ExecutionService> execution_service_{nullptr};

  // Platform options indicating whether to force refresh results or not.
  const PlatformOptions platform_options_;

  DelayState delay_state_{DelayState::DELAY_NOT_HIT};

  base::WeakPtrFactory<ResultRefreshManager> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_RESULT_REFRESH_MANAGER_H_
