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
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
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
class OnDeviceModelComponentStateManager;
class OnDeviceModelAdaptationLoader;
class OnDeviceModelServiceController;
class OptimizationGuideModelProvider;

class ModelExecutionManager : public OptimizationTargetModelObserver {
 public:
  ModelExecutionManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* local_state,
      signin::IdentityManager* identity_manager,
      scoped_refptr<OnDeviceModelServiceController>
          on_device_model_service_controller,
      OptimizationGuideModelProvider* model_provider,
      base::WeakPtr<OnDeviceModelComponentStateManager>
          on_device_component_state_manager,
      OptimizationGuideLogger* optimization_guide_logger,
      base::WeakPtr<ModelQualityLogsUploaderService>
          model_quality_uploader_service);

  ~ModelExecutionManager() override;

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
      std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request,
      OptimizationGuideModelExecutionResultCallback callback);

  // Returns whether an on-device session can be created for `feature`.  An
  // optional `debug_reason` parameter can be provided for more detailed reasons
  // for why an on-device session could not be created.
  bool CanCreateOnDeviceSession(
      ModelBasedCapabilityKey feature,
      OnDeviceModelEligibilityReason* on_device_model_eligibility_reason);

  // Starts a new session for `feature`.
  std::unique_ptr<OptimizationGuideModelExecutor::Session> StartSession(
      ModelBasedCapabilityKey feature,
      const std::optional<SessionConfigParams>& config_params);

  // OptimizationTargetModelObserver:
  void OnModelUpdated(proto::OptimizationTarget target,
                      base::optional_ref<const ModelInfo> model_info) override;

  void Shutdown();

 private:
  // Invoked when the model execution result is available.
  void OnModelExecuteResponse(
      ModelBasedCapabilityKey feature,
      std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request,
      OptimizationGuideModelExecutionResultCallback callback,
      base::expected<const proto::ExecuteResponse,
                     OptimizationGuideModelExecutionError> execute_response);

  // Owned by OptimizationGuideKeyedService and outlives `this`. This is to be
  // passed through the ModelQualityLogEntry to invoke upload during log
  // destruction.
  base::WeakPtr<ModelQualityLogsUploaderService>
      model_quality_uploader_service_;

  // Owned by OptimizationGuideKeyedService and outlives `this`.
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;

  // The endpoint for the model execution service.
  const GURL model_execution_service_url_;

  // The active fetchers per ModelExecutionFeature.
  std::map<ModelBasedCapabilityKey, ModelExecutionFetcher>
      active_model_execution_fetchers_;

  // The URL Loader Factory that will be used by the fetchers.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Unowned IdentityManager for fetching access tokens. Could be null for
  // incognito profiles.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  // Map from feature to its model adaptation loader. Present only for features
  // that require model adaptation.
  const std::map<ModelBasedCapabilityKey, OnDeviceModelAdaptationLoader>
      model_adaptation_loaders_;

  // The model provider to observe for updates to auxiliary models.
  raw_ptr<OptimizationGuideModelProvider> model_provider_;

  // Controller for the on-device service.
  scoped_refptr<OnDeviceModelServiceController>
      on_device_model_service_controller_;

  // Whether the user registered for supplementary on-device models.
  bool did_register_for_supplementary_on_device_models_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get `weak_ptr_` to self.
  base::WeakPtrFactory<ModelExecutionManager> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_MANAGER_H_
