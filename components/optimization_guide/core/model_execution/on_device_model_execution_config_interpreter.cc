// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_execution_config_interpreter.h"

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_value_utils.h"
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

std::vector<Rule> ExtractRedactRules(const proto::RedactRules& proto_rules) {
  std::vector<Rule> rules;
  if (proto_rules.rules_size()) {
    for (const auto& rule : proto_rules.rules()) {
      if (rule.has_regex() && rule.has_behavior()) {
        rules.push_back(Rule());
        rules.back().regex = rule.regex();
        rules.back().behavior = rule.behavior();
        if (rule.has_replacement_string()) {
          rules.back().replacement_string = rule.replacement_string();
        }
        if (rule.has_min_pattern_length()) {
          rules.back().min_pattern_length = rule.min_pattern_length();
        }
        if (rule.has_max_pattern_length()) {
          rules.back().max_pattern_length = rule.max_pattern_length();
        }
        if (rule.has_group_index()) {
          rules.back().matching_group = rule.group_index();
        }
      }
    }
  }
  return rules;
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

  return base::Contains(feature_to_data_, feature);
}

std::string
OnDeviceModelExecutionConfigInterpreter::GetStringToCheckForRedacting(
    proto::ModelExecutionFeature feature,
    const google::protobuf::MessageLite& message) const {
  auto feature_iter = feature_to_data_.find(feature);
  if (feature_iter == feature_to_data_.end() ||
      !feature_iter->second->redactor) {
    return std::string();
  }
  const auto& feature_config = feature_iter->second->config;
  for (const auto& proto_field :
       feature_config.output_config().redact_rules().fields_to_check()) {
    std::optional<proto::Value> value = GetProtoValue(message, proto_field);
    if (value.has_value()) {
      const std::string string_value = GetStringFromValue(*value);
      if (!string_value.empty()) {
        return string_value;
      }
    }
  }
  return std::string();
}

const Redactor* OnDeviceModelExecutionConfigInterpreter::GetRedactorForFeature(
    proto::ModelExecutionFeature feature) const {
  const auto feature_iter = feature_to_data_.find(feature);
  return feature_iter != feature_to_data_.end()
             ? feature_iter->second->redactor.get()
             : nullptr;
}

void OnDeviceModelExecutionConfigInterpreter::RegisterFeature(
    const proto::OnDeviceModelExecutionFeatureConfig& config) {
  std::unique_ptr<FeatureData> feature_data = std::make_unique<FeatureData>();
  feature_data->config = config;
  if (config.has_output_config() && config.output_config().has_redact_rules() &&
      config.output_config().redact_rules().fields_to_check_size() &&
      !config.output_config().redact_rules().rules().empty()) {
    feature_data->redactor = std::make_unique<Redactor>(
        ExtractRedactRules(config.output_config().redact_rules()));
  }
  feature_to_data_[config.feature()] = std::move(feature_data);
}

void OnDeviceModelExecutionConfigInterpreter::PopulateFeatureConfigs(
    std::unique_ptr<proto::OnDeviceModelExecutionConfig> config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!config) {
    return;
  }

  for (const auto& feature_config : config->feature_configs()) {
    RegisterFeature(feature_config);
  }
}

void OnDeviceModelExecutionConfigInterpreter::ClearState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  feature_to_data_.clear();
}

std::optional<SubstitutionResult>
OnDeviceModelExecutionConfigInterpreter::ConstructInputString(
    proto::ModelExecutionFeature feature,
    const google::protobuf::MessageLite& request,
    bool want_input_context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Get the config to construct the input string.
  auto feature_iter = feature_to_data_.find(feature);
  if (feature_iter == feature_to_data_.end()) {
    return std::nullopt;
  }
  const auto& feature_config = feature_iter->second->config;
  if (!feature_config.has_input_config()) {
    return std::nullopt;
  }
  const auto input_config = feature_config.input_config();
  if (input_config.request_base_name() != request.GetTypeName()) {
    return std::nullopt;
  }
  google::protobuf::RepeatedPtrField<proto::SubstitutedString>
      config_substitutions =
          want_input_context ? input_config.input_context_substitutions()
                             : input_config.execute_substitutions();

  return CreateSubstitutions(request, config_substitutions);
}

std::optional<proto::Any>
OnDeviceModelExecutionConfigInterpreter::ConstructOutputMetadata(
    proto::ModelExecutionFeature feature,
    const std::string& output) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto iter = feature_to_data_.find(feature);
  if (iter == feature_to_data_.end()) {
    return std::nullopt;
  }
  const auto& feature_config = iter->second->config;
  if (!feature_config.has_output_config()) {
    return std::nullopt;
  }
  auto output_config = feature_config.output_config();

  return SetProtoValue(output_config.proto_type(), output_config.proto_field(),
                       output);
}

OnDeviceModelExecutionConfigInterpreter::FeatureData::FeatureData() = default;
OnDeviceModelExecutionConfigInterpreter::FeatureData::~FeatureData() = default;

}  // namespace optimization_guide
