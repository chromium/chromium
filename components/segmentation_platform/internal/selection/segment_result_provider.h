// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_RESULT_PROVIDER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_RESULT_PROVIDER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/execution/execution_request.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace base {
class Clock;
}
namespace segmentation_platform {

class ExecutionService;
class SignalStorageConfig;

// Used for retrieving the result of a particular model.
// The steps to get result for the model are as follows:
// 1. Returns score from database as result if present and valid. Do this step
//    only if `ignore_db_score = false` else jump to step 2.
// 2. If there is no valid score present in database or `ignore_db_scores =
//    true`, run the server model. If a valid score is computed return it as
//    result and save it to database.
// 3. If there is no valid score from server model execution, return default
//    model score from database. Do this step only if `ignore_db_score = false`
//    else jump to step 4.
// 4. If there is no valid score from database and server model execution,
//    execute and get score from default model. If a valid score is computed
//    return it as result and save it to database.
class SegmentResultProvider {
 public:
  SegmentResultProvider() = default;
  virtual ~SegmentResultProvider() = default;

  enum class ResultState {
    kUnknown = 0,
    kServerModelDatabaseScoreUsed = 1,
    kServerModelSegmentInfoNotAvailable = 2,
    kServerModelSignalsNotCollected = 3,
    kServerModelDatabaseScoreNotReady = 4,
    kDefaultModelDatabaseScoreUsed = 5,
    kDefaultModelSegmentInfoNotAvailable = 6,
    kDefaultModelSignalsNotCollected = 7,
    kDefaultModelDatabaseScoreNotReady = 8,
    kDefaultModelExecutionFailed = 9,
    kDefaultModelExecutionScoreUsed = 10,
    kServerModelExecutionFailed = 11,
    kServerModelExecutionScoreUsed = 12,
  };

  struct SegmentResult {
    explicit SegmentResult(ResultState state);
    SegmentResult(ResultState state,
                  const proto::PredictionResult& result,
                  float rank);
    ~SegmentResult();
    SegmentResult(const SegmentResult&) = delete;
    SegmentResult& operator=(const SegmentResult&) = delete;

    ResultState state = ResultState::kUnknown;

    // Contains the raw scores along with output config.
    proto::PredictionResult result;

    // If model was executed, the processed feature list.
    std::optional<ModelProvider::Request> model_inputs;

    // TODO(shaktisahu): Delete this after full migration.
    std::optional<float> rank;
  };
  using SegmentResultCallback =
      base::OnceCallback<void(std::unique_ptr<SegmentResult>)>;

  // Creates the instance.
  static std::unique_ptr<SegmentResultProvider> Create(
      SegmentInfoDatabase* segment_info_database,
      SignalStorageConfig* signal_storage_config,
      ExecutionService* execution_service,
      base::Clock* clock,
      bool force_refresh_results);

  // Options for `GetSegmentResult()`.
  struct GetResultOptions {
    GetResultOptions();
    ~GetResultOptions();

    // The segment ID to fetch result for.
    SegmentId segment_id = SegmentId::OPTIMIZATION_TARGET_UNKNOWN;

    // The key is needed for computing segment rank from discrete mapping.
    std::string discrete_mapping_key;

    // Ignores model results stored in database and executes them to fetch
    // results. When set to false, the result could be from following:
    //  * Score cached in the database for server model.
    //  * When score is missing for server model in DB, executes server model if
    //    exists.
    //  * If server model fails to execute, get score from DB for default model.
    //  * When score is missing for default model in DB, executes default model.
    // When set to true, the result could be from following:
    //  * Execution of server model.
    //  * Fallback to default model execution when server model fails to
    //  execute.
    bool ignore_db_scores = false;

    // If `save_results_to_db` is set to true, whenever server/default model is
    // executed, result is written to database.
    bool save_results_to_db = false;

    // Callback to return the segment result.
    SegmentResultCallback callback;

    // Current context of the browser that is needed by feature processor for
    // some of the models.
    scoped_refptr<InputContext> input_context;
  };

  // Returns latest available score for the segment.
  virtual void GetSegmentResult(std::unique_ptr<GetResultOptions> options) = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_RESULT_PROVIDER_H_
