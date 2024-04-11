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
#include "components/optimization_guide/core/model_execution/session_impl.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

class OptimizationGuideLogger;

namespace base {
class FilePath;
}  // namespace base

namespace optimization_guide {
class OnDeviceModelAccessController;
class OnDeviceModelComponentStateManager;
class OnDeviceModelExecutionConfigInterpreter;
class ModelQualityLogsUploaderService;

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
    : public base::RefCounted<OnDeviceModelServiceController>,
      public OnDeviceModelComponentStateManager::Observer {
 public:
  OnDeviceModelServiceController(
      std::unique_ptr<OnDeviceModelAccessController> access_controller,
      base::WeakPtr<OnDeviceModelComponentStateManager>
          on_device_component_state_manager);

  // Initializes OnDeviceModelServiceController. This should be called once
  // after creation.
  void Init();

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

  // Shuts down the service if there is no active model.
  void ShutdownServiceIfNoModelLoaded();

  bool IsConnectedForTesting() {
    return model_remote_.is_bound() || service_remote_.is_bound();
  }

  // Sets the language detection model to be used by the ODM service when text
  // safety evaluation is restricted to a specific set of languages.
  void SetLanguageDetectionModel(
      base::optional_ref<const ModelInfo> model_info);

  // Updates safety model if the model path provided by `model_info` differs
  // from what is already loaded. Virtual for testing.
  virtual void MaybeUpdateSafetyModel(
      base::optional_ref<const ModelInfo> model_info);

  // OnDeviceModelComponentStateManager::Observer.
  void StateChanged(const OnDeviceModelComponentState* state) override;

  OnDeviceModelExecutionConfigInterpreter& ConfigInterpreterForTesting() {
    return *config_interpreter_;
  }

 protected:
  ~OnDeviceModelServiceController() override;

  std::optional<base::FilePath> language_detection_model_path() const {
    return language_detection_model_path_;
  }

 private:
  class OnDeviceModelClient : public SessionImpl::OnDeviceModelClient {
   public:
    OnDeviceModelClient(
        base::WeakPtr<OnDeviceModelServiceController> controller,
        on_device_model::ModelAssetPaths model_paths);
    ~OnDeviceModelClient() override;
    bool ShouldUse() override;
    mojo::Remote<on_device_model::mojom::OnDeviceModel>& GetModelRemote()
        override;
    void OnResponseCompleted() override;
    void OnSessionTimedOut() override;

   private:
    base::WeakPtr<OnDeviceModelServiceController> controller_;
    on_device_model::ModelAssetPaths model_paths_;
  };
  friend class OnDeviceModelClient;
  friend class base::RefCounted<OnDeviceModelServiceController>;
  friend class ChromeOnDeviceModelServiceController;
  friend class OnDeviceModelServiceControllerTest;
  friend class FakeOnDeviceModelServiceController;

  class SafetyModelInfo {
   public:
    ~SafetyModelInfo();

    static std::unique_ptr<SafetyModelInfo> Load(
        base::optional_ref<const ModelInfo> model_info);
    std::optional<proto::FeatureTextSafetyConfiguration> GetConfig(
        proto::ModelExecutionFeature feature) const;
    base::FilePath GetDataPath() const;
    base::FilePath GetSpModelPath() const;
    int64_t GetVersion() const;
    uint32_t num_output_categories() const { return num_output_categories_; }

   private:
    SafetyModelInfo(
        const ModelInfo& model_info,
        uint32_t num_output_categories,
        base::flat_map<proto::ModelExecutionFeature,
                       proto::FeatureTextSafetyConfiguration> feature_configs);

    const ModelInfo model_info_;
    const uint32_t num_output_categories_;
    base::flat_map<proto::ModelExecutionFeature,
                   proto::FeatureTextSafetyConfiguration>
        feature_configs_;
  };

  // Sets the base model directory and initializes the on-device model
  // controller with the parameters, to be ready to load models and execute.
  void SetModelPath(const base::FilePath& model_path,
                    const std::string& component_version);
  void ClearModelPath();

  // Ensures the service is running and provides a remote for the model.
  mojo::Remote<on_device_model::mojom::OnDeviceModel>& GetOrCreateModelRemote(
      on_device_model::ModelAssetPaths model_paths);

  // Invoked at the end of model load, to continue with model execution.
  void OnLoadModelResult(on_device_model::mojom::LoadModelResult result);

  // Called when the model assets have been loaded from disk and are ready to be
  // sent to the service.
  void OnModelAssetsLoaded(
      mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
      on_device_model::ModelAssets assets);

  // Called when disconnected from the model.
  void OnDisconnected();

  // Called when the remote (either `service_remote_` or `_model_remote_` is
  // idle.
  void OnRemoteIdle();

  // Gets the model versions based on the current model paths set.
  proto::OnDeviceModelVersions GetModelVersions(
      const std::string& component_version) const;

  // This may be null in the destructor, otherwise non-null.
  std::unique_ptr<OnDeviceModelAccessController> access_controller_;
  base::WeakPtr<OnDeviceModelComponentStateManager>
      on_device_component_state_manager_;

  // Directory containing file assets for underlying on-device models. This does
  // not include the text safety model or the language detection model.
  std::optional<base::FilePath> model_path_;
  // Full path of the language detection model file if it's available.
  std::optional<base::FilePath> language_detection_model_path_;

  std::optional<proto::OnDeviceModelVersions> model_versions_;
  // Can be null if no safey model available.
  std::unique_ptr<SafetyModelInfo> safety_model_info_;
  std::unique_ptr<OnDeviceModelExecutionConfigInterpreter> config_interpreter_;
  mojo::Remote<on_device_model::mojom::OnDeviceModelService> service_remote_;
  mojo::Remote<on_device_model::mojom::OnDeviceModel> model_remote_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get `weak_ptr_` to self.
  base::WeakPtrFactory<OnDeviceModelServiceController> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_
