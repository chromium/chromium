// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_execution_config_interpreter.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"

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

OnDeviceModelExecutionConfigInterpreter::
    OnDeviceModelExecutionConfigInterpreter() {
  // Set background task priority to user visible if feature param is specified
  // to load config with higher priority. Otherwise, use best effort.
  auto background_task_priority =
      features::ShouldLoadOnDeviceModelExecutionConfigWithHigherPriority()
          ? base::TaskPriority::USER_VISIBLE
          : base::TaskPriority::BEST_EFFORT;
  background_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), background_task_priority});
}

OnDeviceModelExecutionConfigInterpreter::
    ~OnDeviceModelExecutionConfigInterpreter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void OnDeviceModelExecutionConfigInterpreter::UpdateConfigWithFileDir(
    const base::FilePath& file_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Received a new config. The current state of this object is now invalid.
  ClearState();

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadOnDeviceModelExecutionConfig, file_dir),
      base::BindOnce(
          &OnDeviceModelExecutionConfigInterpreter::PopulateFeatureConfigs,
          weak_ptr_factory_.GetWeakPtr()));
}

scoped_refptr<const OnDeviceModelFeatureAdapter>
OnDeviceModelExecutionConfigInterpreter::GetAdapter(
    proto::ModelExecutionFeature feature) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto iter = adapters_.find(feature);
  return iter != adapters_.end() ? iter->second : nullptr;
}

void OnDeviceModelExecutionConfigInterpreter::PopulateFeatureConfigs(
    std::unique_ptr<proto::OnDeviceModelExecutionConfig> config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!config) {
    return;
  }

  for (auto& feature_config : *config->mutable_feature_configs()) {
    auto feature = feature_config.feature();
    adapters_[feature] = base::MakeRefCounted<OnDeviceModelFeatureAdapter>(
        std::move(feature_config));
  }
}

void OnDeviceModelExecutionConfigInterpreter::ClearState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  adapters_.clear();
}

}  // namespace optimization_guide
