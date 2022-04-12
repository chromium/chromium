// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_RESULT_PROVIDER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_RESULT_PROVIDER_H_

#include "base/callback.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Clock;
}
namespace segmentation_platform {

class DefaultModelManager;
class ExecutionService;
class SignalStorageConfig;

// Used for retrieving the result of a particular model.
class SegmentResultProvider {
 public:
  SegmentResultProvider() = default;
  virtual ~SegmentResultProvider() = default;

  enum class ResultState {
    kUnknown = 0,
    kSuccessFromDatabase = 1,
    kSegmentNotAvailable = 2,
    kSignalsNotCollected = 3,
    kDatabaseScoreNotReady = 4,
    kDefaultModelSignalNotCollected = 5,
    kDefaultModelMetadataMissing = 6,
    kDefaultModelExecutionFailed = 7,
    kDefaultModelScoreUsed = 8,
  };
  struct SegmentResult {
    explicit SegmentResult(ResultState state);
    SegmentResult(ResultState state, int rank);
    ~SegmentResult();
    SegmentResult(SegmentResult&) = delete;
    SegmentResult& operator=(SegmentResult&) = delete;

    ResultState state = ResultState::kUnknown;
    absl::optional<int> rank;
  };
  using SegmentResultCallback =
      base::OnceCallback<void(std::unique_ptr<SegmentResult>)>;

  // Creates the instance.
  static std::unique_ptr<SegmentResultProvider> Create(
      SegmentInfoDatabase* segment_info_database,
      SignalStorageConfig* signal_storage_config,
      DefaultModelManager* default_model_manager,
      ExecutionService* execution_service,
      base::Clock* clock,
      bool force_refresh_results);

  // Returns latest available score for the segment.
  virtual void GetSegmentResult(OptimizationTarget segment_id,
                                const std::string& segmentation_key,
                                SegmentResultCallback callback) = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_RESULT_PROVIDER_H_
