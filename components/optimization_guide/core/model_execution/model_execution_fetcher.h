// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FETCHER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FETCHER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/proto/model_execution.pb.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace optimization_guide {

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
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FETCHER_H_
