// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_MANAGER_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "url/gurl.h"

class OptimizationGuideLogger;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace optimization_guide {

class ModelExecutionFetcher;

class ModelExecutionManager {
 public:
  ModelExecutionManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      OptimizationGuideLogger* optimization_guide_logger);

  ~ModelExecutionManager();

  ModelExecutionManager(const ModelExecutionManager&) = delete;
  ModelExecutionManager& operator=(const ModelExecutionManager&) = delete;

  void ExecuteModel(proto::ModelExecutionFeature feature,
                    const google::protobuf::MessageLite& request_metadata,
                    OptimizationGuideModelExecutionResultCallback callback);

 private:
  // Invoked when the model execution result is available.
  void OnModelExecuteResponse(
      proto::ModelExecutionFeature feature,
      OptimizationGuideModelExecutionResultCallback callback,
      base::optional_ref<const proto::ExecuteResponse> execute_response);

  // Owned by OptimizationGuideKeyedService and outlives `this`.
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;

  // The endpoint for the model execution service.
  const GURL model_execution_service_url_;

  // The active fetchers per ModelExecutionFeature.
  std::map<proto::ModelExecutionFeature, ModelExecutionFetcher>
      active_model_execution_fetchers_;

  // The URL Loader Factory that will be used by the fetchers.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Unowned IdentityManager for fetching access tokens. Could be null for
  // incognito profiles.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  // The set of OAuth scopes to use for requesting access token.
  std::set<std::string> oauth_scopes_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get `weak_ptr_` to self.
  base::WeakPtrFactory<ModelExecutionManager> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_MANAGER_H_
