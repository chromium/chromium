// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FETCHER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FETCHER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/feature_registry/feature_keys.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/proto/model_execution.pb.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace optimization_guide {

// Status of a request to fetch model execution from the optimization guide
// service.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ModelExecutionFetcherRequestStatus)
enum class ModelExecutionFetcherRequestStatus {
  // DEPRECATED: No fetch status known. Used in testing.
  // kUnknown = 0,
  // Fetch request was sent and a response received.
  kSuccess = 1,
  // Fetch request was sent but no response received.
  kResponseError = 2,
  // Legacy value from HintsFetcher: Fetch request not sent because of offline
  // network status.
  // kDeprecatedNetworkOffline = 3,
  // Fetch request not sent because fetcher was busy with another request.
  kFetcherBusy = 4,
  // Legacy value from HintsFetcher: Hints fetch request not sent because the
  // host and URL lists were empty.
  // kNoHostsOrURLsToFetchHints = 5,
  // Legacy value from HintsFetcher: Hints fetch request not sent because no
  // supported optimization types were provided.
  // kNoSupportedOptimizationTypesToFetchHints = 6,
  // Fetch request was canceled before completion.
  kRequestCanceled = 7,
  // Fetch request was not started because user was not signed-in.
  kUserNotSignedIn = 8,

  // Insert new values before this line.
  kMaxValue = kUserNotSignedIn
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/optimization/enums.xml:OptimizationGuideModelExecutionFetcherRequestStatus)

using ModelExecuteResponseCallback = base::OnceCallback<void(
    base::expected<const proto::ExecuteResponse,
                   OptimizationGuideModelExecutionError>)>;

// Interface for fetching model executions from the remote Optimization Guide
// Service.
class ModelExecutionFetcher {
 public:
  virtual ~ModelExecutionFetcher() = default;

  // Executes a model for the given `feature`. The `request_metadata` is the
  // feature-specific request metadata. The `callback` is invoked when the
  // execution is complete.
  virtual void ExecuteModel(
      ModelBasedCapabilityKey feature,
      signin::IdentityManager* identity_manager,
      const google::protobuf::MessageLite& request_metadata,
      std::optional<base::TimeDelta> timeout,
      ModelExecuteResponseCallback callback) = 0;

 protected:
  // Records the request status histogram for the given `feature`.
  static void RecordRequestStatusHistogram(
      ModelBasedCapabilityKey feature,
      ModelExecutionFetcherRequestStatus status);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FETCHER_H_
