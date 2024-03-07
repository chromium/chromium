// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/substitution.h"

#include <optional>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_value_utils.h"
#include "components/optimization_guide/proto/substitution.pb.h"

namespace optimization_guide {

namespace {

using google::protobuf::RepeatedPtrField;

// The maximum number of args that can be substituted in the string template.
static constexpr int kMaxArgs = 32;

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
    default:
      base::debug::DumpWithoutCrashing();
      return false;
  }
}

bool AndConditions(const google::protobuf::MessageLite& message,
                   const RepeatedPtrField<proto::Condition>& conditions) {
  for (const auto& condition : conditions) {
    if (!EvaluateCondition(message, condition)) {
      return false;
    }
  }
  return true;
}

bool OrConditions(const google::protobuf::MessageLite& message,
                  const RepeatedPtrField<proto::Condition>& conditions) {
  for (const auto& condition : conditions) {
    if (EvaluateCondition(message, condition)) {
      return true;
    }
  }
  return false;
}

// Returns whether `conditions` apply based on `message`.
bool DoConditionsApply(const google::protobuf::MessageLite& message,
                       const proto::ConditionList& conditions) {
  if (conditions.conditions_size() == 0) {
    return true;
  }

  switch (conditions.condition_evaluation_type()) {
    case proto::CONDITION_EVALUATION_TYPE_OR:
      return OrConditions(message, conditions.conditions());
    case proto::CONDITION_EVALUATION_TYPE_AND:
      return AndConditions(message, conditions.conditions());
    default:
      base::debug::DumpWithoutCrashing();
      return false;
  }
}

}  // namespace

std::optional<SubstitutionResult> CreateSubstitutions(
    const google::protobuf::MessageLite& request,
    const google::protobuf::RepeatedPtrField<proto::SubstitutedString>&
        config_substitutions) {
  // Construct string.
  std::vector<std::string> substitutions;
  bool should_ignore_input_context = false;
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

  return SubstitutionResult{
      .input_string = base::StrCat(substitutions),
      .should_ignore_input_context = should_ignore_input_context};
}

}  // namespace optimization_guide
