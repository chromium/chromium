// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_manager.h"

#include "base/command_line.h"
#include "components/optimization_guide/core/model_execution/model_execution_fetcher.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace optimization_guide {

namespace {

// Returns the URL endpoint for the model execution service along with the
// needed API key.
GURL GetModelExecutionServiceURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  DCHECK(command_line->HasSwitch(
      switches::kOptimizationGuideServiceModelExecutionURL));
  GURL url(command_line->GetSwitchValueASCII(
      switches::kOptimizationGuideServiceModelExecutionURL));
  return net::AppendOrReplaceQueryParameter(
      url, "key", features::GetOptimizationGuideServiceAPIKey());
}

}  // namespace

ModelExecutionManager::ModelExecutionManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    OptimizationGuideLogger* optimization_guide_logger)
    : optimization_guide_logger_(optimization_guide_logger),
      model_execution_service_url_(GetModelExecutionServiceURL()),
      url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager),
      oauth_scopes_(features::GetOAuthScopesForModelExecution()) {}

ModelExecutionManager::~ModelExecutionManager() = default;

void ModelExecutionManager::ExecuteModel(
    proto::ModelExecutionFeature feature,
    const google::protobuf::MessageLite& request_metadata,
    OptimizationGuideModelExecutionResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (active_model_execution_fetchers_.find(feature) !=
      active_model_execution_fetchers_.end()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  auto fetcher_it = active_model_execution_fetchers_.emplace(
      std::piecewise_construct, std::forward_as_tuple(feature),
      std::forward_as_tuple(url_loader_factory_, model_execution_service_url_,
                            optimization_guide_logger_));
  fetcher_it.first->second.ExecuteModel(
      feature, identity_manager_, oauth_scopes_, request_metadata,
      base::BindOnce(&ModelExecutionManager::OnModelExecuteResponse,
                     weak_ptr_factory_.GetWeakPtr(), feature,
                     std::move(callback)));
}

void ModelExecutionManager::OnModelExecuteResponse(
    proto::ModelExecutionFeature feature,
    OptimizationGuideModelExecutionResultCallback callback,
    base::optional_ref<const proto::ExecuteResponse> execute_response) {
  active_model_execution_fetchers_.erase(feature);
  if (!execute_response.has_value()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  if (execute_response->has_error_message()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  if (!execute_response->has_response_metadata()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  std::move(callback).Run(execute_response->response_metadata());
}

}  // namespace optimization_guide
