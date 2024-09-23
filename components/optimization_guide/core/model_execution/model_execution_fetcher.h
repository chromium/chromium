// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FETCHER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FETCHER_H_

#include <memory>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "url/gurl.h"

class OptimizationGuideLogger;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace optimization_guide {

using ModelExecuteResponseCallback = base::OnceCallback<void(
    base::expected<const proto::ExecuteResponse,
                   OptimizationGuideModelExecutionError>)>;

class ModelExecutionFetcher {
 public:
  ModelExecutionFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& optimization_guide_service_url,
      OptimizationGuideLogger* optimization_guide_logger);

  ModelExecutionFetcher(const ModelExecutionFetcher&) = delete;
  ModelExecutionFetcher& operator=(const ModelExecutionFetcher&) = delete;

  ~ModelExecutionFetcher();

  void ExecuteModel(ModelBasedCapabilityKey feature,
                    signin::IdentityManager* identity_manager,
                    const google::protobuf::MessageLite& request_metadata,
                    ModelExecuteResponseCallback callback);

 private:
  // Invoked when the access token is received, to continue with the model
  // execution request.
  void OnAccessTokenReceived(const std::string& serialized_request,
                             const std::string& access_token);

  // URL loader completion callback.
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  // The URL for the remote Optimization Guide Service.
  const GURL optimization_guide_service_url_;

  // Used to hold the callback while the SimpleURLLoader performs the request
  // asynchronously.
  ModelExecuteResponseCallback model_execution_callback_;

  // Holds the currently active url request.
  std::unique_ptr<network::SimpleURLLoader> active_url_loader_;

  // Used for creating an `active_url_loader_` when needed for request hints.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The start time of the current fetch, used to determine the latency in
  // retrieving from the remote Optimization Guide Service.
  base::TimeTicks fetch_start_time_;

  // The model execution feature the fetch is happening for.
  std::optional<ModelBasedCapabilityKey> model_execution_feature_;

  // Owned by OptimizationGuideKeyedService and outlives `this`.
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get `weak_ptr_` to self.
  base::WeakPtrFactory<ModelExecutionFetcher> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FETCHER_H_
