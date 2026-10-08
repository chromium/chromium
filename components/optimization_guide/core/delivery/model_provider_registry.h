// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_MODEL_PROVIDER_REGISTRY_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_MODEL_PROVIDER_REGISTRY_H_

#include "base/observer_list.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/optimization_guide_internals/webui/optimization_guide_internals.mojom.h"

namespace optimization_guide {

// Different events of the prediction model delivery lifecycle for an
// OptimizationTarget.
// Keep in sync with OptimizationGuideModelDeliveryEvent in enums.xml.
enum class ModelDeliveryEvent {
  kUnknown = 0,

  // The model was delivered from immediately or after a
  // successful download.
  kModelDeliveredAtRegistration = 1,
  kModelDelivered = 2,

  // GetModelsRequest was sent to the optimization guide server.
  kGetModelsRequest = 3,

  // Model was requested to be downloaded using download service.
  kDownloadServiceRequest = 4,

  // Download service started the model download.
  kModelDownloadStarted = 5,

  // Model got downloaded from the download service.
  kModelDownloaded = 6,

  // Download service was unavailable.
  kDownloadServiceUnavailable = 7,

  // GetModelsResponse failed.
  kGetModelsResponseFailure = 8,

  // Download URL received from model metadata is invalid
  kDownloadURLInvalid = 9,

  // Model download failed due to download service or verifying the downloaded
  // model.
  kModelDownloadFailure = 10,

  // Loading the model from store failed.
  kModelLoadFailed = 11,

  // Model download was attempted after the model load failed.
  kModelDownloadDueToModelLoadFailure = 12,

  // Add new values above this line.
  kMaxValue = kModelDownloadDueToModelLoadFailure,
};

// Basic implementation of OptimizationGuideModelProvider that tracks
// observers and current models, and notifies observers on updates.
// In production, this will be wrapped by PredictionManager to add download
// triggering on registration, but this implementation can also be used
// directly in tests.
class ModelProviderRegistry final : public OptimizationGuideModelProvider {
 public:
  explicit ModelProviderRegistry(OptimizationGuideLogger* logger);
  ~ModelProviderRegistry() override;

  // OptimizationGuideModelProvider:
  void AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      const std::optional<proto::Any>& model_metadata,
      scoped_refptr<base::SequencedTaskRunner> model_task_runner,
      OptimizationTargetModelObserver* observer) override;
  void RemoveObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      OptimizationTargetModelObserver* observer) override;

  bool HasRegistrations() { return !model_registration_info_map_.empty(); }
  bool IsRegistered(proto::OptimizationTarget target) {
    return model_registration_info_map_.contains(target);
  }
  // Gets the model metadata for the current registration, if any.
  base::optional_ref<const proto::Any> GetRegistrationMetadata(
      proto::OptimizationTarget target) const;
  // Gets the set of all registered targets.
  base::flat_set<proto::OptimizationTarget> GetRegisteredOptimizationTargets()
      const;

  // Returns the current model for the target, or nullptr if one is not
  // available yet.
  const ModelInfo* GetModel(proto::OptimizationTarget target) const;
  // Gets information about all the available models.
  std::vector<optimization_guide_internals::mojom::DownloadedModelInfoPtr>
  GetDownloadedModelsInfoForWebUI() const;
  // Updates the model for `optimization_target` and notifies observers.
  void UpdateModel(proto::OptimizationTarget optimization_target,
                   ModelInfo model_info);
  // Removes the model and notifies observers.
  void RemoveModel(proto::OptimizationTarget optimization_target);
  // Like UpdateModel, but NotifyObservers right away instead of via PostTask.
  void UpdateModelImmediatelyForTesting(
      proto::OptimizationTarget optimization_target,
      ModelInfo model_info);
  // Updates the lifecycle histogram for the target.
  static void RecordLifecycleState(
      proto::OptimizationTarget optimization_target,
      ModelDeliveryEvent event);

 private:
  // Contains the model registration specific info to be kept for each
  // optimization target.
  struct ModelRegistrationInfo {
    explicit ModelRegistrationInfo(std::optional<proto::Any> metadata);
    ~ModelRegistrationInfo();

    // The feature-provided metadata that was registered with the prediction
    // manager.
    std::optional<proto::Any> metadata;

    // The set of model observers that were registered to receive model updates
    // from the prediction manager.
    base::ObserverList<OptimizationTargetModelObserver> model_observers;
  };

  // Notifies observers of `optimization_target` that the model has been
  // updated. `model_info` will be nullopt when the model was stopped to be
  // served from the server, and removed from the store,
  void NotifyObserversOfNewModel(
      proto::OptimizationTarget optimization_target,
      base::optional_ref<const ModelInfo> model_info);

  base::flat_map<proto::OptimizationTarget, ModelInfo>
      optimization_target_model_info_map_;

  std::map<proto::OptimizationTarget, ModelRegistrationInfo>
      model_registration_info_map_;

  // The logger that plumbs the debug logs to the optimization guide
  // internals page. Not owned. Guaranteed to outlive |this|, since the logger
  // and |this| are owned by the optimization guide keyed service.
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;

  base::WeakPtrFactory<ModelProviderRegistry> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_MODEL_PROVIDER_REGISTRY_H_
