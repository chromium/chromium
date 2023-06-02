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

  // This refreshes model results. It does the following when result for a
  // model is required.
  // 1. Try to get model result from database.
  // 2. If present in database, update it in prefs if prefs result is invalid or
  // expired.
  // 3. Else, try to get result by running the model and also save computed
  // scores in db and update it in prefs if prefs result is invalid or
  // expired.
  void RefreshModelResults(
      std::map<std::string, std::unique_ptr<SegmentResultProvider>>
          result_providers,
      ExecutionService* execution_service);

  // This is triggered when model info is updated. This ensures model execution,
  // updating prefs and database if required on model update.
  void OnModelUpdated(proto::SegmentInfo* segment_info,
                      ExecutionService* execution_service);

 private:
  // Gives result for the model based on `run_model`. If `run_model` is false,
  // tries to get the result from database, else tries to get the result by
  // executing model. It also saves to the result to database after model
  // execution.
  void GetCachedResultOrRunModel(SegmentResultProvider* segment_result_provider,
                                 const Config* config,
                                 ExecutionService* execution_service);

  void OnGetCachedResultOrRunModel(
      SegmentResultProvider* segment_result_provider,
      const Config* config,
      ExecutionService* execution_service,
      std::unique_ptr<SegmentResultProvider::SegmentResult> result);

  // Configs for all registered clients.
  const raw_ptr<const ConfigHolder> config_holder_;

  // Stores `SegmentResultProvider` for all clients.
  std::map<std::string, std::unique_ptr<SegmentResultProvider>>
      result_providers_;

  // Delegate to write results for all clients to prefs if previous results are
  // not present or invalid.
  const raw_ptr<CachedResultWriter, DanglingUntriaged> cached_result_writer_;

  // Platform options indicating whether to force refresh results or not.
  const PlatformOptions platform_options_;

  base::WeakPtrFactory<ResultRefreshManager> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_RESULT_REFRESH_MANAGER_H_
