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

namespace optimization_guide {

namespace {

// The maximum number of args that can be substituted in the string template.
static constexpr int kMaxArgs = 32;

std::unique_ptr<proto::OnDeviceModelExecutionConfig>
ReadOnDeviceModelExecutionConfig(const base::FilePath& path) {
  // Unpack and verify model config file.
  base::FilePath config_path =
      path.Append(FILE_PATH_LITERAL("on_device_model_execution_config.pb"));
  std::string binary_config_pb;
  if (!base::ReadFileToString(config_path, &binary_config_pb)) {
    return nullptr;
  }

  proto::OnDeviceModelExecutionConfig config;
  if (!config.ParseFromString(binary_config_pb)) {
    return nullptr;
  }

  return std::make_unique<proto::OnDeviceModelExecutionConfig>(config);
}

std::string StringPrintfVector(const std::string& string_template,
                               std::vector<std::string> args) {
  CHECK(args.size() <= kMaxArgs);

  args.resize(kMaxArgs, "");
  return base::StringPrintfNonConstexpr(
      string_template.c_str(), args[0].c_str(), args[1].c_str(),
      args[2].c_str(), args[3].c_str(), args[4].c_str(), args[5].c_str(),
      args[6].c_str(), args[7].c_str(), args[8].c_str(), args[9].c_str(),
      args[10].c_str(), args[11].c_str(), args[12].c_str(), args[13].c_str(),
      args[14].c_str(), args[15].c_str(), args[16].c_str(), args[17].c_str(),
      args[18].c_str(), args[19].c_str(), args[20].c_str(), args[21].c_str(),
      args[22].c_str(), args[23].c_str(), args[24].c_str(), args[25].c_str(),
      args[26].c_str(), args[27].c_str(), args[28].c_str(), args[29].c_str(),
      args[30].c_str(), args[31].c_str());
}

// Returns whether `condition` applies based on `message`.
bool EvaluateCondition(const google::protobuf::MessageLite& message,
                       const proto::Condition& condition) {
  std::optional<proto::Value> proto_value =
      GetProtoValue(message, condition.proto_field());
  if (!proto_value) {
    return false;
  }

  switch (condition.operator_type()) {
    case proto::OPERATOR_TYPE_EQUAL_TO:
      return AreValuesEqual(*proto_value, condition.value());
    case proto::OPERATOR_TYPE_NOT_EQUAL_TO:
      return !AreValuesEqual(*proto_value, condition.value());
    case proto::OPERATOR_TYPE_UNSPECIFIED:
      NOTREACHED();
      return false;
  }
}

// Returns whether `conditions` apply based on `message`.
bool DoConditionsApply(const google::protobuf::MessageLite& message,
                       const proto::ConditionList& conditions) {
  if (conditions.conditions_size() == 0) {
    return true;
  }

  for (const auto& condition : conditions.conditions()) {
    bool applies = EvaluateCondition(message, condition);
    if (applies && conditions.condition_evaluation_type() ==
                       proto::CONDITION_EVALUATION_TYPE_OR) {
      return true;
    }
    if (!applies && conditions.condition_evaluation_type() ==
                        proto::CONDITION_EVALUATION_TYPE_AND) {
      return false;
    }
  }

  return conditions.condition_evaluation_type() ==
         proto::CONDITION_EVALUATION_TYPE_AND;
}

}  // namespace

OnDeviceModelExecutionConfigInterpreter::
    OnDeviceModelExecutionConfigInterpreter()
    : background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})) {}

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

  return base::Contains(feature_configs_, feature);
}

void OnDeviceModelExecutionConfigInterpreter::PopulateFeatureConfigs(
    std::unique_ptr<proto::OnDeviceModelExecutionConfig> config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!config) {
    return;
  }

  for (const auto& feature_config : config->feature_configs()) {
    feature_configs_[feature_config.feature()] = feature_config;
  }
}

void OnDeviceModelExecutionConfigInterpreter::ClearState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  feature_configs_.clear();
}

std::optional<
    OnDeviceModelExecutionConfigInterpreter::InputStringConstructionResult>
OnDeviceModelExecutionConfigInterpreter::ConstructInputString(
    proto::ModelExecutionFeature feature,
    const google::protobuf::MessageLite& request,
    bool want_input_context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Get the config to construct the input string.
  if (!HasConfigForFeature(feature)) {
    return std::nullopt;
  }
  auto feature_config = feature_configs_.at(feature);
  if (!feature_config.has_input_config()) {
    return std::nullopt;
  }
  const auto input_config = feature_config.input_config();
  if (input_config.request_base_name() != request.GetTypeName()) {
    return std::nullopt;
  }

  // Construct string.
  std::vector<std::string> substitutions;
  bool should_ignore_input_context = false;
  google::protobuf::RepeatedPtrField<proto::SubstitutedString>
      config_substitutions =
          want_input_context ? input_config.input_context_substitutions()
                             : input_config.execute_substitutions();
  for (const auto& substitution : config_substitutions) {
    if (!DoConditionsApply(request, substitution.conditions())) {
      continue;
    }

    if (substitution.should_ignore_input_context()) {
      should_ignore_input_context = true;
    }

    std::vector<std::string> args(substitution.substitutions_size());
    for (int32_t i = 0; i < substitution.substitutions_size(); ++i) {
      const auto& arg = substitution.substitutions(i);
      for (const auto& candidate : arg.candidates()) {
        if (!DoConditionsApply(request, candidate.conditions())) {
          continue;
        }

        if (candidate.has_raw_string()) {
          args[i] = candidate.raw_string();
        } else if (candidate.has_proto_field()) {
          std::optional<proto::Value> value =
              GetProtoValue(request, candidate.proto_field());
          if (!value) {
            return std::nullopt;
          }
          args[i] = GetStringFromValue(*value);
        }
        break;
      }
    }

    substitutions.push_back(
        StringPrintfVector(substitution.string_template(), std::move(args)));
  }

  return InputStringConstructionResult{
      .input_string = base::StrCat(substitutions),
      .should_ignore_input_context = should_ignore_input_context};
}

std::optional<proto::Any>
OnDeviceModelExecutionConfigInterpreter::ConstructOutputMetadata(
    proto::ModelExecutionFeature feature,
    const std::string& output) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!HasConfigForFeature(feature)) {
    return std::nullopt;
  }

  auto feature_config = feature_configs_.at(feature);
  if (!feature_config.has_output_config()) {
    return std::nullopt;
  }
  auto output_config = feature_config.output_config();

  return SetProtoValue(output_config.proto_type(), output_config.proto_field(),
                       output);
}

}  // namespace optimization_guide
