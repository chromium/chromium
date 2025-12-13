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
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/safe_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/optional_ref.h"
#include "base/types/pass_key.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/core/model_execution/model_broker_impl.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/model_execution/on_device_model_metadata.h"
#include "components/optimization_guide/core/model_execution/on_device_model_validator.h"
#include "components/optimization_guide/core/model_execution/safety_checker.h"
#include "components/optimization_guide/core/model_execution/safety_client.h"
#include "components/optimization_guide/core/model_execution/safety_model_info.h"
#include "components/optimization_guide/core/model_execution/session_impl.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/cpp/service_client.h"
#include "services/on_device_model/public/cpp/text_safety_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace optimization_guide {
enum class OnDeviceModelEligibilityReason;
class OnDeviceModelAccessController;
class OnDeviceModelAdaptationMetadata;
class OnDeviceModelComponentStateManager;
class OnDeviceModelMetadata;
class OnDeviceModelAdaptationController;
class PerformanceClassifier;

class ModelController {
 public:
  ModelController();
  virtual ~ModelController() = 0;

  ModelController(ModelController&) = delete;
  ModelController& operator=(ModelController&) = delete;

  virtual mojo::Remote<on_device_model::mojom::OnDeviceModel>&
  GetOrCreateRemote() = 0;
};

// Controls the lifetime of the on-device model service, loading and unloading
// of the models, and executing them via the service. There is normally only
// a single instance of this object.
class OnDeviceModelServiceController final {
 public:
  OnDeviceModelServiceController(
      std::unique_ptr<OnDeviceModelAccessController> access_controller,
      base::SafeRef<PerformanceClassifier> performance_classifier,
      base::WeakPtr<OnDeviceModelComponentStateManager>
          on_device_component_state_manager,
      UsageTracker& usage_tracker,
      base::SafeRef<on_device_model::ServiceClient> service_client);
  ~OnDeviceModelServiceController();

  // Whether an on-device session can be created for `feature`.
  OnDeviceModelEligibilityReason CanCreateSession(
      mojom::OnDeviceFeature feature);

  // Starts a session for `feature`. This will start the service and load the
  // model if it is not already loaded. The session will handle updating
  // context, executing input, and sending the response.
  std::unique_ptr<OnDeviceSession> CreateSession(
      mojom::OnDeviceFeature feature,
      base::WeakPtr<OptimizationGuideLogger> logger,
      const SessionConfigParams& config_params);

  // Sets the language detection model to be used by the ODM service when text
  // safety evaluation is restricted to a specific set of languages.
  void SetLanguageDetectionModel(
      base::optional_ref<const ModelInfo> model_info);

  // Updates safety model if the model path provided by `safety_model_info`
  // differs from what is already loaded. Virtual for testing.
  void MaybeUpdateSafetyModel(
      std::unique_ptr<SafetyModelInfo> safety_model_info);

  // Updates the main execution model.
  void UpdateModel(std::unique_ptr<OnDeviceModelMetadata> model_metadata);

  // Updates the model adaptation for the feature.
  void MaybeUpdateModelAdaptation(mojom::OnDeviceFeature feature,
                                  MaybeAdaptationMetadata adaptation_metadata);

  // Add/remove observers for notifying on-device model availability changes.
  void AddOnDeviceModelAvailabilityChangeObserver(
      mojom::OnDeviceFeature feature,
      OnDeviceModelAvailabilityObserver* observer);
  void RemoveOnDeviceModelAvailabilityChangeObserver(
      mojom::OnDeviceFeature feature,
      OnDeviceModelAvailabilityObserver* observer);

  // Calls `callback` with the capabilities of the current model.
  on_device_model::Capabilities GetCapabilities();

  base::WeakPtr<OnDeviceModelServiceController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Retrieves the object storing the adaptation metadata for 'feature'.
  MaybeAdaptationMetadata& GetFeatureMetadata(mojom::OnDeviceFeature feature);

  // Returns the selected performance hint.
  proto::OnDeviceModelPerformanceHint GetPerformanceHint();

  void BindBroker(mojo::PendingReceiver<mojom::ModelBroker> receiver) {
    model_broker_impl_.BindBroker(std::move(receiver));
  }

  const SafetyClient& GetSafetyClientForTesting() const {
    return safety_client_;
  }

 private:
  // A set of (references to) compatible, versioned dependencies that implement
  // a OnDeviceFeature.
  // e.g. "You can summarize with this model by building the prompt this way."
  class Solution final : public ModelBrokerImpl::Solution {
   public:
    Solution(mojom::OnDeviceFeature feature,
             scoped_refptr<const OnDeviceModelFeatureAdapter> adapter,
             base::WeakPtr<ModelController> model_controller,
             std::unique_ptr<SafetyChecker> safety_checker,
             base::SafeRef<OnDeviceModelServiceController> controller);
    ~Solution() override;
    Solution(Solution&) = delete;
    Solution(Solution&&) = delete;
    Solution& operator=(Solution&) = delete;
    Solution& operator=(Solution&&) = delete;

    // Whether all of the dependencies are still available.
    bool IsValid() const override;

    // Creates a config describing this solution;
    mojom::ModelSolutionConfigPtr MakeConfig() const override;

    const scoped_refptr<const OnDeviceModelFeatureAdapter>& adapter() const {
      return adapter_;
    }
    const base::WeakPtr<ModelController>& model_controller() const {
      return model_controller_;
    }
    const SafetyChecker& safety_checker() const { return *safety_checker_; }

   private:
    // mojom::ModelSolution
    void CreateSession(
        mojo::PendingReceiver<on_device_model::mojom::Session> pending,
        on_device_model::mojom::SessionParamsPtr params) override;
    void CreateTextSafetySession(
        mojo::PendingReceiver<on_device_model::mojom::TextSafetySession>
            pending) override;
    void ReportHealthyCompletion() override;

    // What this is a solution for.
    mojom::OnDeviceFeature feature_;
    // Describes how to implement this capability with these dependencies.
    scoped_refptr<const OnDeviceModelFeatureAdapter> adapter_;
    // The language model the adapter config is for.
    base::WeakPtr<ModelController> model_controller_;
    // A safety config and model that satisfy the adapter's requirements.
    std::unique_ptr<SafetyChecker> safety_checker_;
    // The controller that owns this.
    base::SafeRef<OnDeviceModelServiceController> controller_;
  };

  using MaybeSolution = ModelBrokerImpl::MaybeSolution;

  // Manages assets and loading of a particular base model and it's adaptations.
  class BaseModelController final : public ModelController {
   public:
    explicit BaseModelController(
        base::SafeRef<OnDeviceModelServiceController> controller,
        std::unique_ptr<OnDeviceModelMetadata> model_metadata);
    ~BaseModelController() override;

    // Ensures the service is running and base model remote is created.
    // TODO(holte): Don't take paths as an argument.
    mojo::Remote<on_device_model::mojom::OnDeviceModel>& GetOrCreateRemote()
        override;

    // Return the remote for direct use by the feature, adjusting idle timeout.
    mojo::Remote<on_device_model::mojom::OnDeviceModel>& DirectUse();

    on_device_model::ModelAssetPaths PopulateModelPaths();

    OnDeviceModelMetadata* model_metadata() { return model_metadata_.get(); }

    base::WeakPtr<BaseModelController> GetWeakPtr() {
      return weak_ptr_factory_.GetWeakPtr();
    }

    base::WeakPtr<ModelController> GetOrCreateFeatureController(
        mojom::OnDeviceFeature feature,
        const OnDeviceModelAdaptationMetadata& metadata);

    void EraseController(mojom::OnDeviceFeature feature);

    void RequireAdaptationRank(uint32_t rank);

   private:
    OnDeviceModelAccessController& access_controller() {
      return *controller_->access_controller_;
    }

    // Called when the model assets have been loaded from disk and are ready to
    // be sent to the service.
    void OnModelAssetsLoaded(
        mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
        on_device_model::ModelAssets assets);

    // Called when the base model is disconnected unexpectedly.
    void OnDisconnect(uint32_t reason, const std::string& description);

    // Begins the on-device model validation flow.
    void StartValidation();

    // Called when validation has finished or failed.
    void FinishValidation(OnDeviceModelValidationResult result);

    // The service controller that owns this.
    base::SafeRef<OnDeviceModelServiceController> controller_;
    // The metadata of the model this can load.
    std::unique_ptr<OnDeviceModelMetadata> model_metadata_;

    // Whether any feature uses this without an adaptation.
    bool has_direct_use_ = false;

    // The set of adaptations ranks the model is required to support, if loaded.
    std::vector<uint32_t> supported_adaptation_ranks_;

    // Controllers for adaptations that depend on this model.
    std::map<mojom::OnDeviceFeature, OnDeviceModelAdaptationController>
        model_adaptation_controllers_;

    std::unique_ptr<OnDeviceModelValidator> model_validator_;
    mojo::Remote<on_device_model::mojom::OnDeviceModel> remote_;
    base::WeakPtrFactory<BaseModelController> weak_ptr_factory_{this};
  };

  friend class OnDeviceModelAdaptationController;

  // Called when the service disconnects unexpectedly.
  void OnServiceDisconnected(on_device_model::ServiceDisconnectReason reason);

  // Constructs a solution using the currently available dependencies.
  MaybeSolution GetSolution(mojom::OnDeviceFeature feature);

  void UpdateSolutionProviders();
  void UpdateSolutionProvider(mojom::OnDeviceFeature feature);

  // This may be null in the destructor, otherwise non-null.
  std::unique_ptr<OnDeviceModelAccessController> access_controller_;
  base::WeakPtr<OnDeviceModelComponentStateManager>
      on_device_component_state_manager_;
  base::raw_ref<UsageTracker> usage_tracker_;

  base::SafeRef<on_device_model::ServiceClient> service_client_;
  SafetyClient safety_client_;

  AdaptationMetadataMap adaptation_metadata_;
  std::optional<OnDeviceModelMetadataLoader> model_metadata_loader_;

  std::optional<BaseModelController> base_model_controller_;

  ModelBrokerImpl model_broker_impl_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get `weak_ptr_` to self.
  base::WeakPtrFactory<OnDeviceModelServiceController> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_
