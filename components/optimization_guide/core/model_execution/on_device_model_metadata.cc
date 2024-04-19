// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_metadata.h"

#include <cstddef>
#include <memory>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"

namespace optimization_guide {

namespace {

std::unique_ptr<proto::OnDeviceModelExecutionConfig>
ReadOnDeviceModelExecutionConfig(const base::FilePath& path) {
  // Unpack and verify model config file.
  base::FilePath config_path = path.Append(kOnDeviceModelExecutionConfigFile);
  std::string binary_config_pb;
  if (!base::ReadFileToString(config_path, &binary_config_pb)) {
    return nullptr;
  }

  auto config = std::make_unique<proto::OnDeviceModelExecutionConfig>();
  if (!config->ParseFromString(binary_config_pb)) {
    return nullptr;
  }

  return config;
}

}  // namespace

OnDeviceModelMetadata::OnDeviceModelMetadata(
    const base::FilePath& model_path,
    const std::string& version,
    std::unique_ptr<proto::OnDeviceModelExecutionConfig> config)
    : model_path_(model_path), version_(version) {
  if (!config) {
    return;
  }

  for (auto& feature_config : *config->mutable_feature_configs()) {
    auto feature = feature_config.feature();
    adapters_[feature] = base::MakeRefCounted<OnDeviceModelFeatureAdapter>(
        std::move(feature_config));
  }
}

OnDeviceModelMetadata::~OnDeviceModelMetadata() = default;

// static
std::unique_ptr<OnDeviceModelMetadata> OnDeviceModelMetadata::New(
    base::FilePath model_path,
    std::string version,
    std::unique_ptr<proto::OnDeviceModelExecutionConfig> config) {
  return base::WrapUnique(
      new OnDeviceModelMetadata(model_path, version, std::move(config)));
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

  auto model_path_override_switch =
      switches::GetOnDeviceModelExecutionOverride();
  if (model_path_override_switch) {
    Load(*StringToFilePath(*model_path_override_switch), "override");
    return;
  }

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

void OnDeviceModelMetadataLoader::Load(const base::FilePath& model_path,
                                       const std::string& version) {
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadOnDeviceModelExecutionConfig, model_path),
      base::BindOnce(&OnDeviceModelMetadata::New, model_path, version)
          .Then(on_load_fn_));
}

void OnDeviceModelMetadataLoader::StateChanged(
    const OnDeviceModelComponentState* state) {
  // Invalidate the current model immediately.
  Invalidate();
  if (!state) {
    return;
  }
  Load(state->GetInstallDirectory(), state->GetComponentVersion().GetString());
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
