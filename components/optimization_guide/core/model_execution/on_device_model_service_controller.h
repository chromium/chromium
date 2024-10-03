// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_

#include <sys/types.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/optional_ref.h"
#include "base/types/pass_key.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/model_execution/on_device_model_metadata.h"
#include "components/optimization_guide/core/model_execution/on_device_model_validator.h"
#include "components/optimization_guide/core/model_execution/safety_model_info.h"
#include "components/optimization_guide/core/model_execution/session_impl.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "feature_keys.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/cpp/text_safety_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

class OptimizationGuideLogger;

namespace base {
class FilePath;
}  // namespace base

namespace optimization_guide {
enum class OnDeviceModelEligibilityReason;
class OnDeviceModelAccessController;
class OnDeviceModelAdaptationMetadata;
class OnDeviceModelComponentStateManager;
class OnDeviceModelMetadata;
class ModelQualityLogsUploaderService;
class OnDeviceModelAdaptationController;

// Controls the lifetime of the on-device model service, loading and unloading
// of the models, and executing them via the service.
//
// As all OnDeviceModelServiceController's share the same model, and we do not
// want to load duplicate models (would consume excessive amounts of memory), at
// most one instance of OnDeviceModelServiceController is created.
//
// TODO(b/302402576): Handle unloading the model, and stopping the service. The
// StreamingResponder should notify the controller upon completion to accomplish
// this. Also handle multiple requests gracefully and fail the subsequent
// requests, while handling the first one.
class OnDeviceModelServiceController
    : public base::RefCounted<OnDeviceModelServiceController> {
 public:
  OnDeviceModelServiceController(
      std::unique_ptr<OnDeviceModelAccessController> access_controller,
      base::WeakPtr<OnDeviceModelComponentStateManager>
          on_device_component_state_manager);

  // Initializes OnDeviceModelServiceController. This should be called once
  // after creation.
  void Init();

  // Whether an on-device session can be created for `feature`.
  OnDeviceModelEligibilityReason CanCreateSession(
      ModelBasedCapabilityKey feature);

  // Starts a session for `feature`. This will start the service and load the
  // model if it is not already loaded. The session will handle updating
  // context, executing input, and sending the response.
  std::unique_ptr<OptimizationGuideModelExecutor::Session> CreateSession(
      ModelBasedCapabilityKey feature,
      ExecuteRemoteFn execute_remote_fn,
      base::WeakPtr<OptimizationGuideLogger> logger,
      base::WeakPtr<ModelQualityLogsUploaderService>
          model_quality_uploader_service,
      const std::optional<SessionConfigParams>& config_params);

  // Launches the on-device model-service.
  virtual void LaunchService() = 0;

  // Starts the service and calls |callback| with the estimated performance
  // class. Will call with std::nullopt if the service crashes.
  using GetEstimatedPerformanceClassCallback = base::OnceCallback<void(
      std::optional<on_device_model::mojom::PerformanceClass>
          performance_class)>;
  void GetEstimatedPerformanceClass(
      GetEstimatedPerformanceClassCallback callback);

  bool IsConnectedForTesting() {
    return base_model_remote_.is_bound() || service_remote_.is_bound();
  }

  // Sets the language detection model to be used by the ODM service when text
  // safety evaluation is restricted to a specific set of languages.
  void SetLanguageDetectionModel(
      base::optional_ref<const ModelInfo> model_info);

  // Updates safety model if the model path provided by `model_info` differs
  // from what is already loaded. Virtual for testing.
  virtual void MaybeUpdateSafetyModel(
      base::optional_ref<const ModelInfo> model_info);

  // Updates the main execution model.
  void UpdateModel(std::unique_ptr<OnDeviceModelMetadata> model_metadata);

  // Updates the model adaptation for the feature.
  void MaybeUpdateModelAdaptation(
      ModelBasedCapabilityKey feature,
      std::unique_ptr<OnDeviceModelAdaptationMetadata> adaptation_metadata);

  // Called when the model adaptation remote is disconnected.
  void OnModelAdaptationRemoteDisconnected(ModelBasedCapabilityKey feature,
                                           ModelRemoteDisconnectReason reason);

  // Add/remove observers for notifying on-device model availability changes.
  void AddOnDeviceModelAvailabilityChangeObserver(
      ModelBasedCapabilityKey feature,
      OnDeviceModelAvailabilityObserver* observer);
  void RemoveOnDeviceModelAvailabilityChangeObserver(
      ModelBasedCapabilityKey feature,
      OnDeviceModelAvailabilityObserver* observer);

  base::WeakPtr<OnDeviceModelServiceController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  virtual ~OnDeviceModelServiceController();

  std::optional<base::FilePath> language_detection_model_path() const {
    return language_detection_model_path_;
  }

  mojo::Remote<on_device_model::mojom::OnDeviceModel>& base_model_remote() {
    return base_model_remote_;
  }

 private:
  class OnDeviceModelClient : public SessionImpl::OnDeviceModelClient {
   public:
    OnDeviceModelClient(
        ModelBasedCapabilityKey feature,
        base::WeakPtr<OnDeviceModelServiceController> controller,
        const on_device_model::TextSafetyLoaderParams& ts_params,
        const on_device_model::ModelAssetPaths& model_paths,
        base::optional_ref<const on_device_model::AdaptationAssetPaths>
            adaptation_assets);
    ~OnDeviceModelClient() override;
    bool ShouldUse() override;
    mojo::Remote<on_device_model::mojom::OnDeviceModel>& GetModelRemote()
        override;
    mojo::Remote<on_device_model::mojom::TextSafetyModel>&
    GetTextSafetyModelRemote() override;
    void OnResponseCompleted() override;
    void OnSessionTimedOut() override;

   private:
    ModelBasedCapabilityKey feature_;
    base::WeakPtr<OnDeviceModelServiceController> controller_;
    on_device_model::ModelAssetPaths model_paths_;
    on_device_model::TextSafetyLoaderParams ts_params_;

    // Model adaptation assets are populated when it was required.
    std::optional<on_device_model::AdaptationAssetPaths> adaptation_assets_;
  };
  friend class ChromeOnDeviceModelServiceController;
  friend class FakeOnDeviceModelServiceController;
  friend class OnDeviceModelAdaptationController;
  friend class OnDeviceModelClient;
  friend class OnDeviceModelServiceAdaptationControllerTest;
  friend class OnDeviceModelServiceControllerIOS;
  friend class OnDeviceModelServiceControllerTest;
  friend class base::RefCounted<OnDeviceModelServiceController>;

  // Ensures the service is running and provides a remote for the model.
  mojo::Remote<on_device_model::mojom::OnDeviceModel>& GetOrCreateModelRemote(
      ModelBasedCapabilityKey feature,
      const on_device_model::ModelAssetPaths& model_paths,
      base::optional_ref<const on_device_model::AdaptationAssetPaths>
          adaptation_assets);

  mojo::Remote<on_device_model::mojom::TextSafetyModel>&
  GetTextSafetyModelRemote(
      const on_device_model::TextSafetyLoaderParams& params);

  void OnTextSafetyParamsLoaded(
      mojo::PendingReceiver<on_device_model::mojom::TextSafetyModel> model,
      on_device_model::mojom::TextSafetyModelParamsPtr params);

  // Ensures the service is running and base model remote is created.
  void MaybeCreateBaseModelRemote(
      const on_device_model::ModelAssetPaths& model_paths);

  // Invoked at the end of model load, to continue with model execution.
  void OnLoadModelResult(on_device_model::mojom::LoadModelResult result);

  // Called when the model assets have been loaded from disk and are ready to be
  // sent to the service.
  void OnModelAssetsLoaded(
      mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
      on_device_model::ModelAssets assets);

  // Called when disconnected from the model.
  void OnBaseModelDisconnected();

  // Called when `base_model_remote_` is idle.
  void OnBaseModelRemoteIdle();

  scoped_refptr<const OnDeviceModelFeatureAdapter> GetFeatureAdapter(
      ModelBasedCapabilityKey feature);

  // Begins the on-device model validation flow.
  void StartValidation();

  // Called when validation has finished or failed.
  void FinishValidation(OnDeviceModelValidationResult result);

  on_device_model::ModelAssetPaths PopulateModelPaths();

  on_device_model::TextSafetyLoaderParams PopulateTextSafetyParams() const;

  // Called to update the model availability changes for `feature`.
  void NotifyModelAvailabilityChange(ModelBasedCapabilityKey feature);

  // This may be null in the destructor, otherwise non-null.
  std::unique_ptr<OnDeviceModelAccessController> access_controller_;
  std::optional<OnDeviceModelMetadataLoader> model_metadata_loader_;
  base::WeakPtr<OnDeviceModelComponentStateManager>
      on_device_component_state_manager_;

  // Full path of the language detection model file if it's available.
  std::optional<base::FilePath> language_detection_model_path_;

  // Can be null if no safety model available.
  std::unique_ptr<SafetyModelInfo> safety_model_info_;
  std::unique_ptr<OnDeviceModelMetadata> model_metadata_;
  mojo::Remote<on_device_model::mojom::OnDeviceModelService> service_remote_;

  mojo::Remote<on_device_model::mojom::OnDeviceModel> base_model_remote_;

  mojo::Remote<on_device_model::mojom::TextSafetyModel> ts_model_remote_;

  // Maintains the live model adaptation controllers per feature. Created when
  // model adaptation is needed for a feature, and removed when adaptation
  // remote gets disconnected.
  std::map<ModelBasedCapabilityKey, OnDeviceModelAdaptationController>
      model_adaptation_controllers_;

  // Map from feature to its adaptation assets. Present only for features that
  // have valid model adaptation. It could be missing for features that require
  // model adaptation, but they have not been loaded yet.
  base::flat_map<ModelBasedCapabilityKey, OnDeviceModelAdaptationMetadata>
      model_adaptation_metadata_;

  // Whether a session has been started for the most recently updated model.
  bool has_started_session_ = false;

  std::unique_ptr<OnDeviceModelValidator> model_validator_;

  std::map<ModelBasedCapabilityKey,
           base::ObserverList<OnDeviceModelAvailabilityObserver>>
      model_availability_change_observers_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get weak pointers that are reset if the base model is updated.
  base::WeakPtrFactory<OnDeviceModelServiceController>
      base_model_scoped_weak_ptr_factory_{this};

  // Used to get `weak_ptr_` to self.
  base::WeakPtrFactory<OnDeviceModelServiceController> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_
