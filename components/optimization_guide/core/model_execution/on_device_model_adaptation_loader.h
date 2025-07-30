// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_ADAPTATION_LOADER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_ADAPTATION_LOADER_H_

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/delivery/optimization_target_model_observer.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "services/on_device_model/public/cpp/model_assets.h"

namespace optimization_guide {

class OnDeviceModelFeatureAdapter;
class OnDeviceModelMetadata;
class OptimizationGuideModelProvider;

// Detailed availability reason for histograms recording.
enum class OnDeviceModelAdaptationAvailability {
  // Adaptation model was available.
  kAvailable = 0,

  // Base model was not available.
  kBaseModelUnavailable = 1,

  // Base model spec was invalid, so adaptation model cannot be fetched.
  kBaseModelSpecInvalid = 2,

  // Adaptation model was not available.
  kAdaptationModelUnavailable = 3,

  // The received adaptation model was invalid.
  kAdaptationModelInvalid = 4,

  // The received adaptation model was incompatible with the base model.
  kAdaptationModelIncompatible = 5,

  // The execution config in the adaptation model was invalid.
  kAdaptationModelExecutionConfigInvalid = 6,

  // The model execution feature was not recently used.
  kFeatureNotRecentlyUsed = 7,

  // The received adaptation model was incompatible with the base model's
  // performance hints.
  kAdaptationModelHintsIncompatible = 8,

  // This must be kept in sync with OnDeviceModelAdaptationAvailability in
  // optimization/enums.xml.
  kMaxValue = kAdaptationModelHintsIncompatible,
};

// Indication of why a feature adaptation is not available.
// Simplification of OnDeviceModelAdaptationAvailability which is for
// metrics purposes.
enum class AdaptationUnavailability {
  // The adaptation is being replaced.
  kUpdatePending = 0,
  // No model is expected to be available.
  kNotSupported = 1,
};

class OnDeviceModelAdaptationMetadata {
 public:
  OnDeviceModelAdaptationMetadata(
      on_device_model::AdaptationAssetPaths* asset_paths,
      int64_t version,
      scoped_refptr<OnDeviceModelFeatureAdapter> adapter);
  OnDeviceModelAdaptationMetadata(const OnDeviceModelAdaptationMetadata&);
  OnDeviceModelAdaptationMetadata(OnDeviceModelAdaptationMetadata&&);
  ~OnDeviceModelAdaptationMetadata();

  OnDeviceModelAdaptationMetadata& operator=(OnDeviceModelAdaptationMetadata&&);
  bool operator==(const OnDeviceModelAdaptationMetadata& other) const;

  const on_device_model::AdaptationAssetPaths* asset_paths() const;

  scoped_refptr<const OnDeviceModelFeatureAdapter> adapter() const {
    return adapter_;
  }

  int64_t version() const { return version_; }

 private:
  std::optional<on_device_model::AdaptationAssetPaths> asset_paths_;
  int64_t version_;
  scoped_refptr<OnDeviceModelFeatureAdapter> adapter_;
};

using MaybeAdaptationMetadata =
    base::expected<OnDeviceModelAdaptationMetadata, AdaptationUnavailability>;

// Loads model adaptation assets for a particular feature. Performs adaptation
// model compatibility checks with the base model and reloads the assets if the
// base model changes.
class OnDeviceModelAdaptationLoader
    : public OptimizationTargetModelObserver,
      public OnDeviceModelComponentStateManager::Observer {
 public:
  using OnLoadFn = base::RepeatingCallback<void(MaybeAdaptationMetadata)>;

  OnDeviceModelAdaptationLoader(
      ModelBasedCapabilityKey feature,
      OptimizationGuideModelProvider* model_provider,
      base::WeakPtr<OnDeviceModelComponentStateManager>
          on_device_component_state_manager,
      PrefService* local_state,
      OnLoadFn on_load_fn);
  ~OnDeviceModelAdaptationLoader() override;

  OnDeviceModelAdaptationLoader(const OnDeviceModelAdaptationLoader&) = delete;
  OnDeviceModelAdaptationLoader& operator=(
      const OnDeviceModelAdaptationLoader&) = delete;

 private:
  friend class OnDeviceModelAdaptationLoaderTest;

  // Removes any registration for model updates.
  void Unregister();

  // OptimizationTargetModelObserver:
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info) final;

  // OnDeviceModelComponentStateManager::Observer.
  void StateChanged(const OnDeviceModelComponentState* state) final;
  void OnDeviceEligibleFeatureFirstUsed(ModelBasedCapabilityKey feature) final;

  // Registers for adaptation model download, if the conditions are right.
  void MaybeRegisterModelDownload(const OnDeviceModelComponentState* state,
                                  bool was_feature_recently_used);

  ModelBasedCapabilityKey feature_;
  proto::OptimizationTarget target_;

  // The model provider to observe for updates to model adaptations.
  raw_ptr<OptimizationGuideModelProvider> model_provider_;
  base::WeakPtr<OnDeviceModelComponentStateManager>
      on_device_component_state_manager_;
  raw_ptr<PrefService> local_state_;
  OnLoadFn on_load_fn_;

  base::ScopedObservation<OnDeviceModelComponentStateManager,
                          OnDeviceModelComponentStateManager::Observer>
      component_state_manager_observation_{this};

  // The compatibility spec that we've registered for adaptations with.
  std::optional<OnDeviceBaseModelSpec> registered_spec_;

  // Background thread where file processing should be performed.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_ADAPTATION_LOADER_H_
