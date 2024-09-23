// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_metadata.h"

#include <cstddef>
#include <memory>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"

namespace optimization_guide {

OnDeviceModelMetadata::OnDeviceModelMetadata(
    const base::FilePath& model_path,
    const std::string& version,
    const OnDeviceBaseModelSpec& model_spec,
    std::unique_ptr<proto::OnDeviceModelExecutionConfig> config)
    : model_path_(model_path), version_(version), model_spec_(model_spec) {
  if (!config) {
    return;
  }

  for (auto& feature_config : *config->mutable_feature_configs()) {
    auto feature = feature_config.feature();
    adapters_[feature] = base::MakeRefCounted<OnDeviceModelFeatureAdapter>(
        std::move(feature_config));
  }

  validation_config_ = std::move(*config->mutable_validation_config());
}

OnDeviceModelMetadata::~OnDeviceModelMetadata() = default;

// static
std::unique_ptr<OnDeviceModelMetadata> OnDeviceModelMetadata::New(
    base::FilePath model_path,
    std::string version,
    const OnDeviceBaseModelSpec& model_spec,
    std::unique_ptr<proto::OnDeviceModelExecutionConfig> config) {
  return base::WrapUnique(new OnDeviceModelMetadata(
      model_path, version, model_spec, std::move(config)));
}

scoped_refptr<const OnDeviceModelFeatureAdapter>
OnDeviceModelMetadata::GetAdapter(proto::ModelExecutionFeature feature) const {
  const auto iter = adapters_.find(feature);
  return iter != adapters_.end() ? iter->second : nullptr;
}

OnDeviceModelMetadataLoader::OnDeviceModelMetadataLoader(
    OnLoadFn on_load_fn,
    base::WeakPtr<OnDeviceModelComponentStateManager>
        on_device_component_state_manager)
    : on_load_fn_(std::move(on_load_fn)) {
  // Set background task priority to user visible if feature param is specified
  // to load config with higher priority. Otherwise, use best effort.
  auto background_task_priority =
      features::ShouldLoadOnDeviceModelExecutionConfigWithHigherPriority()
          ? base::TaskPriority::USER_VISIBLE
          : base::TaskPriority::BEST_EFFORT;
  background_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), background_task_priority});

  if (on_device_component_state_manager) {
    on_device_component_state_manager_ =
        std::move(on_device_component_state_manager);
    StateChanged(on_device_component_state_manager_->GetState());
    on_device_component_state_manager_->AddObserver(this);
  }
}

OnDeviceModelMetadataLoader::~OnDeviceModelMetadataLoader() {
  if (on_device_component_state_manager_) {
    on_device_component_state_manager_->RemoveObserver(this);
  }
}

void OnDeviceModelMetadataLoader::Load(
    const base::FilePath& model_path,
    const std::string& version,
    const OnDeviceBaseModelSpec& model_spec) {
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ReadOnDeviceModelExecutionConfig,
                     model_path.Append(kOnDeviceModelExecutionConfigFile)),
      base::BindOnce(&OnDeviceModelMetadata::New, model_path, version,
                     model_spec)
          .Then(on_load_fn_));
}

void OnDeviceModelMetadataLoader::StateChanged(
    const OnDeviceModelComponentState* state) {
  // Invalidate the current model immediately.
  Invalidate();
  if (!state) {
    return;
  }
  Load(state->GetInstallDirectory(), state->GetComponentVersion().GetString(),
       state->GetBaseModelSpec());
}

void OnDeviceModelMetadataLoader::Invalidate() {
  // Post task to remove model again after any ongoing Load() completes.
  background_task_runner_->PostTaskAndReply(
      FROM_HERE, base::DoNothing(),
      base::BindOnce([]() -> std::unique_ptr<OnDeviceModelMetadata> {
        return nullptr;
      }).Then(on_load_fn_));
}

}  // namespace optimization_guide
