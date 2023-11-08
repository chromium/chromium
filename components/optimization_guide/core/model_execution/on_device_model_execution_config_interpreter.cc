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

absl::optional<std::string>
OnDeviceModelExecutionConfigInterpreter::ConstructInputString(
    proto::ModelExecutionFeature feature,
    const google::protobuf::MessageLite& request) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Get the config to construct the input string.
  if (!HasConfigForFeature(feature)) {
    return absl::nullopt;
  }
  auto feature_config = feature_configs_.at(feature);
  if (!feature_config.has_input_config()) {
    return absl::nullopt;
  }
  const auto input_config = feature_config.input_config();
  if (input_config.request_base_name() != request.GetTypeName()) {
    return absl::nullopt;
  }

  // Construct string.
  std::vector<std::string> substitutions;
  for (const auto& substitution : input_config.execute_substitutions()) {
    // TODO(b/302402959): See if conditions apply.

    std::vector<std::string> args;
    for (const auto& arg : substitution.args()) {
      // TODO(b/302402959): See if conditions apply.

      if (arg.has_raw_string()) {
        args.push_back(arg.raw_string());
      } else if (arg.has_proto_field()) {
        absl::optional<proto::Value> value =
            GetProtoValue(request, arg.proto_field());
        if (!value) {
          return absl::nullopt;
        }
        args.push_back(GetStringFromValue(*value));
      }
    }
    if (static_cast<size_t>(substitution.expected_num_args()) != args.size()) {
      return absl::nullopt;
    }

    substitutions.push_back(
        StringPrintfVector(substitution.string_template(), std::move(args)));
  }

  return base::StrCat(substitutions);
}

}  // namespace optimization_guide
