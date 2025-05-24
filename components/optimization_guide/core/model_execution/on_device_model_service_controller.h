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

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/safe_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "base/types/pass_key.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/model_execution/on_device_model_metadata.h"
#include "components/optimization_guide/core/model_execution/on_device_model_validator.h"
#include "components/optimization_guide/core/model_execution/safety_checker.h"
#include "components/optimization_guide/core/model_execution/safety_client.h"
#include "components/optimization_guide/core/model_execution/safety_model_info.h"
#include "components/optimization_guide/core/model_execution/session_impl.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "feature_keys.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/cpp/service_client.h"
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
class OnDeviceModelAdaptationController;

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
// of the models, and executing them via the service.
//
// As all OnDeviceModelServiceController's share the same model, and we do not
// want to load duplicate models (would consume excessive amounts of memory), at
// most one instance of OnDeviceModelServiceController is created.
class OnDeviceModelServiceController
    : public base::RefCounted<OnDeviceModelServiceController>,
      public mojom::ModelBroker {
 public:
  OnDeviceModelServiceController(
      std::unique_ptr<OnDeviceModelAccessController> access_controller,
      base::WeakPtr<OnDeviceModelComponentStateManager>
          on_device_component_state_manager,
      on_device_model::ServiceClient::LaunchFn launch_fn);

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
      const std::optional<SessionConfigParams>& config_params);

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

  // Add/remove observers for notifying on-device model availability changes.
  void AddOnDeviceModelAvailabilityChangeObserver(
      ModelBasedCapabilityKey feature,
      OnDeviceModelAvailabilityObserver* observer);
  void RemoveOnDeviceModelAvailabilityChangeObserver(
      ModelBasedCapabilityKey feature,
      OnDeviceModelAvailabilityObserver* observer);

  // Calls `callback` with the capabilities of the current model.
  on_device_model::Capabilities GetCapabilities();

  base::WeakPtr<OnDeviceModelServiceController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  OnDeviceModelAdaptationMetadata* GetFeatureMetadata(
      ModelBasedCapabilityKey feature);

  void BindBroker(mojo::PendingReceiver<mojom::ModelBroker> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  // Ensures the performance class will be up to date and available when
  // `complete` runs.
  void EnsurePerformanceClassAvailable(base::OnceClosure complete);

  virtual void RegisterPerformanceClassSyntheticTrial(
      OnDeviceModelPerformanceClass perf_class) {}

 protected:
  ~OnDeviceModelServiceController() override;

  std::optional<base::FilePath> language_detection_model_path() const {
    return safety_client_.language_detection_model_path();
  }

 private:
  // A set of (references to) compatible, versioned dependencies that implement
  // a ModelBasedCapability.
  // e.g. "You can summarize with this model by building the prompt this way."
  class Solution : public mojom::ModelSolution {
   public:
    Solution(ModelBasedCapabilityKey feature,
             scoped_refptr<const OnDeviceModelFeatureAdapter> adapter,
             base::WeakPtr<ModelController> model_controller,
             std::unique_ptr<SafetyChecker> safety_checker,
             base::SafeRef<OnDeviceModelServiceController> controller);
    Solution(Solution&&);
    ~Solution() override;
    Solution& operator=(Solution&&);

    // Whether all of the dependencies are still available.
    bool IsValid();

    // Creates a config describing this solution;
    mojom::ModelSolutionConfigPtr MakeConfig();

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
    ModelBasedCapabilityKey feature_;
    // Describes how to implement this capability with these dependencies.
    scoped_refptr<const OnDeviceModelFeatureAdapter> adapter_;
    // The language model the adapter config is for.
    base::WeakPtr<ModelController> model_controller_;
    // A safety config and model that satisfy the adapter's requirements.
    std::unique_ptr<SafetyChecker> safety_checker_;
    // The controller that owns this.
    base::SafeRef<OnDeviceModelServiceController> controller_;
  };

  using MaybeSolution =
      base::expected<Solution, OnDeviceModelEligibilityReason>;

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
        ModelBasedCapabilityKey key,
        base::optional_ref<const on_device_model::AdaptationAssetPaths>
            adaptation_assets);

    void EraseController(ModelBasedCapabilityKey key);

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
    void OnDisconnect();

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

    // Controllers for adaptations that depend on this model.
    std::map<ModelBasedCapabilityKey, OnDeviceModelAdaptationController>
        model_adaptation_controllers_;

    std::unique_ptr<OnDeviceModelValidator> model_validator_;
    mojo::Remote<on_device_model::mojom::OnDeviceModel> remote_;
    base::WeakPtrFactory<BaseModelController> weak_ptr_factory_{this};
  };

  // Implements OnDeviceOptions::Client for Sessions created by this object.
  class OnDeviceModelClient final : public OnDeviceOptions::Client {
   public:
    OnDeviceModelClient(
        ModelBasedCapabilityKey feature,
        base::WeakPtr<OnDeviceModelServiceController> controller,
        base::WeakPtr<ModelController> model_controller);
    ~OnDeviceModelClient() override;
    std::unique_ptr<OnDeviceOptions::Client> Clone() const override;
    bool ShouldUse() override;
    void StartSession(
        mojo::PendingReceiver<on_device_model::mojom::Session> pending,
        on_device_model::mojom::SessionParamsPtr params) override;
    void OnResponseCompleted() override;

   private:
    ModelBasedCapabilityKey feature_;
    base::WeakPtr<OnDeviceModelServiceController> controller_;
    base::WeakPtr<ModelController> model_controller_;
  };

  // Keeps subscribers updated with the current solution.
  class SolutionProvider final {
   public:
    explicit SolutionProvider(
        ModelBasedCapabilityKey feature,
        base::SafeRef<OnDeviceModelServiceController> controller);
    ~SolutionProvider();

    void AddSubscriber(mojo::PendingRemote<mojom::ModelSubscriber> pending);
    void AddObserver(OnDeviceModelAvailabilityObserver* observer);
    void RemoveObserver(OnDeviceModelAvailabilityObserver* observer);

    void Update(MaybeSolution solution);

    MaybeSolution& solution() { return solution_; }

   private:
    void UpdateSubscribers();
    void UpdateSubscriber(mojom::ModelSubscriber& client);
    void UpdateObservers();

    ModelBasedCapabilityKey feature_;
    base::SafeRef<OnDeviceModelServiceController> controller_;

    mojo::RemoteSet<mojom::ModelSubscriber> subscribers_;
    base::ObserverList<OnDeviceModelAvailabilityObserver> observers_;

    MaybeSolution solution_ =
        base::unexpected(OnDeviceModelEligibilityReason::kUnknown);
    mojo::ReceiverSet<mojom::ModelSolution> receivers_;
  };
  friend class SolutionProvider;
  friend class OnDeviceModelAdaptationController;
  friend class OnDeviceModelClient;
  friend class base::RefCounted<OnDeviceModelServiceController>;

  // Called when the service disconnects unexpectedly.
  void OnServiceDisconnected(on_device_model::ServiceDisconnectReason reason);

  // Constructs a solution using the currently available dependencies.
  MaybeSolution GetSolution(ModelBasedCapabilityKey feature);

  // Get (or construct) the solution provider for the feature.
  SolutionProvider& GetSolutionProvider(ModelBasedCapabilityKey feature);

  // Called to update model availability for all features.
  void UpdateSolutionProviders();

  // Called to update the model availability changes for `feature`.
  void UpdateSolutionProvider(ModelBasedCapabilityKey feature);

  // mojom::ModelBroker:
  void Subscribe(mojom::ModelSubscriptionOptionsPtr opts,
                 mojo::PendingRemote<mojom::ModelSubscriber> client) override;

  void SubscribeInternal(mojom::ModelSubscriptionOptionsPtr opts,
                         mojo::PendingRemote<mojom::ModelSubscriber> client);

  // Called when performance class has finished updating.
  void PerformanceClassUpdated(OnDeviceModelPerformanceClass perf_class);

  // This may be null in the destructor, otherwise non-null.
  std::unique_ptr<OnDeviceModelAccessController> access_controller_;
  std::optional<OnDeviceModelMetadataLoader> model_metadata_loader_;
  base::WeakPtr<OnDeviceModelComponentStateManager>
      on_device_component_state_manager_;

  on_device_model::ServiceClient service_client_;
  SafetyClient safety_client_;

  // Map from feature to its adaptation assets. Present only for features that
  // have valid model adaptation. It could be missing for features that require
  // model adaptation, but they have not been loaded yet.
  base::flat_map<ModelBasedCapabilityKey, OnDeviceModelAdaptationMetadata>
      model_adaptation_metadata_;

  std::map<ModelBasedCapabilityKey, SolutionProvider> solution_providers_;

  SEQUENCE_CHECKER(sequence_checker_);

  std::optional<BaseModelController> base_model_controller_;

  mojo::ReceiverSet<mojom::ModelBroker> receivers_;

  enum class PerformanceClassState {
    kNotSet,
    kComputing,
    kComplete,
  };
  PerformanceClassState performance_class_state_ =
      PerformanceClassState::kNotSet;

  // Callbacks waiting for performance class to finish computing.
  base::OnceClosureList performance_class_callbacks_;

  // Used to get `weak_ptr_` to self.
  base::WeakPtrFactory<OnDeviceModelServiceController> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_
