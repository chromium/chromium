// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_ADAPTATION_LOADER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_ADAPTATION_LOADER_H_

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/types/optional_util.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "services/on_device_model/public/cpp/model_assets.h"

namespace optimization_guide {

class OnDeviceModelFeatureAdapter;
class OnDeviceModelMetadata;
class OptimizationGuideModelProvider;
enum class OnDeviceModelAdaptationAvailability;

class OnDeviceModelAdaptationMetadata {
 public:
  static std::unique_ptr<OnDeviceModelAdaptationMetadata> New(
      on_device_model::AdaptationAssetPaths* asset_paths,
      int64_t version,
      scoped_refptr<OnDeviceModelFeatureAdapter> adapter);

  OnDeviceModelAdaptationMetadata(const OnDeviceModelAdaptationMetadata&);
  ~OnDeviceModelAdaptationMetadata();

  const on_device_model::AdaptationAssetPaths* asset_paths() const {
    return base::OptionalToPtr(asset_paths_);
  }

  scoped_refptr<const OnDeviceModelFeatureAdapter> adapter() const {
    return adapter_;
  }

  int64_t version() const { return version_; }

 private:
  friend class OnDeviceModelServiceControllerTest;

  OnDeviceModelAdaptationMetadata(
      on_device_model::AdaptationAssetPaths* asset_paths,
      int64_t version,
      scoped_refptr<OnDeviceModelFeatureAdapter> adapter);
  std::optional<on_device_model::AdaptationAssetPaths> asset_paths_;
  int64_t version_;
  scoped_refptr<OnDeviceModelFeatureAdapter> adapter_;
};

// Loads model adaptation assets for a particular feature. Performs adaptation
// model compatibility checks with the base model and reloads the assets if the
// base model changes.
class OnDeviceModelAdaptationLoader
    : public OptimizationTargetModelObserver,
      public OnDeviceModelComponentStateManager::Observer {
 public:
  using OnLoadFn = base::RepeatingCallback<void(
      std::unique_ptr<OnDeviceModelAdaptationMetadata>)>;

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

  base::expected<std::unique_ptr<on_device_model::AdaptationAssetPaths>,
                 OnDeviceModelAdaptationAvailability>
  ProcessModelUpdate(
      base::optional_ref<const optimization_guide::ModelInfo> model_info);

  ModelBasedCapabilityKey feature_;

  // The model spec of the latest base model, received from the component
  // state manager.
  std::optional<OnDeviceBaseModelSpec> base_model_spec_;

  OnLoadFn on_load_fn_;

  base::ScopedObservation<OnDeviceModelComponentStateManager,
                          OnDeviceModelComponentStateManager::Observer>
      component_state_manager_observation_{this};

  base::WeakPtr<OnDeviceModelComponentStateManager>
      on_device_component_state_manager_;

  raw_ptr<PrefService> local_state_;

  // The model provider to observe for updates to model adaptations.
  raw_ptr<OptimizationGuideModelProvider> model_provider_;
  bool registered_with_model_provider_ = false;

  // Background thread where file processing should be performed.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_ADAPTATION_LOADER_H_
