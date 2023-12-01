// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_MANAGER_H_

#include <map>
#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
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
class OnDeviceModelServiceController;

class ModelExecutionManager {
 public:
  ModelExecutionManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      scoped_refptr<OnDeviceModelServiceController>
          on_device_model_service_controller,
      OptimizationGuideLogger* optimization_guide_logger);

  ~ModelExecutionManager();

  ModelExecutionManager(const ModelExecutionManager&) = delete;
  ModelExecutionManager& operator=(const ModelExecutionManager&) = delete;

  // Executes the model when model execution happens remotely.
  //
  // As this can potentially be called as a fallback from on-device,
  // `log_ai_data_request` may be populated already with any existing work prior
  // to calling this function.
  void ExecuteModel(
      proto::ModelExecutionFeature feature,
      const google::protobuf::MessageLite& request_metadata,
      std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request,
      OptimizationGuideModelExecutionResultCallback callback);

  // Starts a new session for `feature`.
  std::unique_ptr<OptimizationGuideModelExecutor::Session> StartSession(
      proto::ModelExecutionFeature feature);

 private:
  // Called from SessionImpl (via ExecuteRemoteFn) when model execution happens
  // remotely.
  void ExecuteModelWithStreaming(
      proto::ModelExecutionFeature feature,
      const google::protobuf::MessageLite& request_metadata,
      std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request,
      OptimizationGuideModelExecutionResultStreamingCallback callback);

  // Invoked when the model execution result is available.
  void OnModelExecuteResponse(
      proto::ModelExecutionFeature feature,
      std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request,
      OptimizationGuideModelExecutionResultCallback callback,
      base::expected<const proto::ExecuteResponse,
                     OptimizationGuideModelExecutionError> execute_response);

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

  // Controller for the on-device service.
  scoped_refptr<OnDeviceModelServiceController>
      on_device_model_service_controller_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get `weak_ptr_` to self.
  base::WeakPtrFactory<ModelExecutionManager> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_MANAGER_H_
