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
  // `model_info` must be non-null, use RemoveModel to remove models.
  void UpdateModel(proto::OptimizationTarget optimization_target,
                   std::unique_ptr<ModelInfo> model_info);
  // Removes the model and notifies observers.
  void RemoveModel(proto::OptimizationTarget optimization_target);
  // Like UpdateModel, but NotifyObservers right away instead of via PostTask.
  void UpdateModelImmediatelyForTesting(
      proto::OptimizationTarget optimization_target,
      std::unique_ptr<ModelInfo> model_info);
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

  base::flat_map<proto::OptimizationTarget, std::unique_ptr<ModelInfo>>
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
