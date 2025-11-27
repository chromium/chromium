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
#include "components/optimization_guide/core/delivery/optimization_target_model_observer.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
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

class ModelExecutionManager final {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Used to provide alternative fetcher implementations.
    virtual std::unique_ptr<ModelExecutionFetcher> CreateLegionFetcher() = 0;
  };

  ModelExecutionManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      std::unique_ptr<Delegate> delegate,
      OptimizationGuideLogger* optimization_guide_logger,
      base::WeakPtr<ModelQualityLogsUploaderService>
          model_quality_uploader_service);
  ~ModelExecutionManager();

  ModelExecutionManager(const ModelExecutionManager&) = delete;
  ModelExecutionManager& operator=(const ModelExecutionManager&) = delete;

  // Executes the model when model execution happens remotely.
  //
  // As this can potentially be called as a fallback from on-device,
  // `log_ai_data_request` may be populated already with any existing work prior
  // to calling this function.
  void ExecuteModel(
      ModelBasedCapabilityKey feature,
      const google::protobuf::MessageLite& request_metadata,
      std::optional<base::TimeDelta> timeout,
      std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request,
      ModelExecutionServiceType service_type,
      OptimizationGuideModelExecutionResultCallback callback);

  // Records a fake model execution response to be returned when ExecuteModel is
  // called for the given feature.
  void AddExecutionResultForTesting(
      ModelBasedCapabilityKey feature,
      OptimizationGuideModelExecutionResult result);

  void Shutdown();

 private:
  // Identifies a ModelExecutionFetcher.
  using FetcherId = size_t;

  // All active executions for a certain feature.
  using ActiveFeatureExecutions =
      std::map<FetcherId, std::unique_ptr<ModelExecutionFetcher>>;

  // Creates a new ModelExecutionFetcher.
  std::unique_ptr<ModelExecutionFetcher> CreateModelExecutionFetcher(
      ModelExecutionServiceType service_type);

  // Invoked when the model execution result is available.
  void OnModelExecuteResponse(
      ModelBasedCapabilityKey feature,
      FetcherId fetcher_id,
      std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request,
      OptimizationGuideModelExecutionResultCallback callback,
      base::expected<const proto::ExecuteResponse,
                     OptimizationGuideModelExecutionError> execute_response);

  // Returns the `OnDeviceModelAdaptationMetadata` for `feature`.
  std::optional<optimization_guide::OnDeviceModelAdaptationMetadata>
  GetOnDeviceModelAdaptationMetadata(
      optimization_guide::ModelBasedCapabilityKey feature);

  // Owned by OptimizationGuideKeyedService and outlives `this`. This is to be
  // passed through the ModelQualityLogEntry to invoke upload during log
  // destruction.
  base::WeakPtr<ModelQualityLogsUploaderService>
      model_quality_uploader_service_;

  // Owned by OptimizationGuideKeyedService and outlives `this`.
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;

  // The endpoint for the model execution service.
  const GURL model_execution_service_url_;

  // Provides alternative fetcher implementations.
  std::unique_ptr<Delegate> delegate_;

  // The next available `FetcherId`. Assigned in increasing order.
  FetcherId next_model_execution_fetcher_id = 0;

  // The active fetchers per ModelExecutionFeature.
  std::map<ModelBasedCapabilityKey, ActiveFeatureExecutions>
      active_model_execution_fetchers_;

  // Model execution results to override in tests.
  std::map<ModelBasedCapabilityKey, OptimizationGuideModelExecutionResult>
      test_execution_results_;

  // The URL Loader Factory that will be used by the fetchers.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Unowned IdentityManager for fetching access tokens. Could be null for
  // incognito profiles.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get `weak_ptr_` to self.
  base::WeakPtrFactory<ModelExecutionManager> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_MANAGER_H_
