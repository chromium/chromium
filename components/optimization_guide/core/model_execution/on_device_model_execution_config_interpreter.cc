// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_execution_config_interpreter.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_value_utils.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/redactor.h"
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

bool OnDeviceModelExecutionConfigInterpreter::HasConfigForFeature(
    proto::ModelExecutionFeature feature) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return base::Contains(adapters_, feature);
}

const OnDeviceModelFeatureAdapter*
OnDeviceModelExecutionConfigInterpreter::GetAdapter(
    proto::ModelExecutionFeature feature) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto iter = adapters_.find(feature);
  return iter != adapters_.end() ? iter->second.get() : nullptr;
}

std::string
OnDeviceModelExecutionConfigInterpreter::GetStringToCheckForRedacting(
    proto::ModelExecutionFeature feature,
    const google::protobuf::MessageLite& message) const {
  if (const auto* adapter = GetAdapter(feature)) {
    return adapter->GetStringToCheckForRedacting(message);
  }
  return std::string();
}

const Redactor* OnDeviceModelExecutionConfigInterpreter::GetRedactorForFeature(
    proto::ModelExecutionFeature feature) const {
  if (const auto* adapter = GetAdapter(feature)) {
    return adapter->redactor();
  }
  return nullptr;
}

void OnDeviceModelExecutionConfigInterpreter::PopulateFeatureConfigs(
    std::unique_ptr<proto::OnDeviceModelExecutionConfig> config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!config) {
    return;
  }

  for (auto& feature_config : *config->mutable_feature_configs()) {
    auto feature = feature_config.feature();
    adapters_[feature] = std::make_unique<OnDeviceModelFeatureAdapter>(
        std::move(feature_config));
  }
}

void OnDeviceModelExecutionConfigInterpreter::ClearState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  adapters_.clear();
}

std::optional<SubstitutionResult>
OnDeviceModelExecutionConfigInterpreter::ConstructInputString(
    proto::ModelExecutionFeature feature,
    const google::protobuf::MessageLite& request,
    bool want_input_context) const {
  if (const auto* adapter = GetAdapter(feature)) {
    return adapter->ConstructInputString(request, want_input_context);
  }
  return std::nullopt;
}

std::optional<proto::Any>
OnDeviceModelExecutionConfigInterpreter::ConstructOutputMetadata(
    proto::ModelExecutionFeature feature,
    const std::string& output) const {
  if (const auto* adapter = GetAdapter(feature)) {
    return adapter->ConstructOutputMetadata(output);
  }
  return std::nullopt;
}

}  // namespace optimization_guide
