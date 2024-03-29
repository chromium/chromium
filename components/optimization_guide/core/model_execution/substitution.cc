// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/substitution.h"

#include <optional>
#include <sstream>
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
#include "components/optimization_guide/proto/descriptors.pb.h"
#include "components/optimization_guide/proto/substitution.pb.h"

namespace optimization_guide {

namespace {

using google::protobuf::RepeatedPtrField;

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

// Resolve various expression in proto::SubstitutedString by appending
// appropriate text to an output string and updating state.
// Methods return false on error.
class StringBuilder {
 public:
  enum class Error {
    OK = 0,
    FAILED = 1,
  };
  StringBuilder() = default;
  Error ResolveSubstitutedString(const google::protobuf::MessageLite& request,
                                 const proto::SubstitutedString& substitution);

  SubstitutionResult result() {
    return SubstitutionResult{
        .input_string = out_.str(),
        .should_ignore_input_context = should_ignore_input_context_};
  }

 private:
  Error ResolveSubstitution(const google::protobuf::MessageLite& request,
                            const proto::StringSubstitution& arg);

  // Resolve a StringArg, returns false on error.
  Error ResolveArg(const google::protobuf::MessageLite& request,
                   const proto::StringArg& candidate);

  Error ResolveRangeExpr(const google::protobuf::MessageLite& request,
                         const proto::RangeExpr& expr);

  Error ResolveProtoField(const google::protobuf::MessageLite& request,
                          const proto::ProtoField& field);

  std::ostringstream out_;
  bool should_ignore_input_context_ = false;
};

StringBuilder::Error StringBuilder::ResolveProtoField(
    const google::protobuf::MessageLite& request,
    const proto::ProtoField& field) {
  std::optional<proto::Value> value = GetProtoValue(request, field);
  if (!value) {
    return Error::FAILED;
  }
  out_ << GetStringFromValue(*value);
  return Error::OK;
}

StringBuilder::Error StringBuilder::ResolveRangeExpr(
    const google::protobuf::MessageLite& request,
    const proto::RangeExpr& expr) {
  std::vector<std::string> vals;
  auto it = GetProtoRepeated(&request, expr.proto_field());
  if (!it) {
    return Error::FAILED;
  }
  for (const auto* msg : *it) {
    Error error = ResolveSubstitutedString(*msg, expr.expr());
    if (error != Error::OK) {
      return error;
    }
  }
  return Error::OK;
}

StringBuilder::Error StringBuilder::ResolveArg(
    const google::protobuf::MessageLite& request,
    const proto::StringArg& candidate) {
  switch (candidate.arg_case()) {
    case proto::StringArg::kRawString:
      out_ << candidate.raw_string();
      return Error::OK;
    case proto::StringArg::kProtoField:
      return ResolveProtoField(request, candidate.proto_field());
    case proto::StringArg::kRangeExpr:
      return ResolveRangeExpr(request, candidate.range_expr());
    case proto::StringArg::ARG_NOT_SET:
      return Error::FAILED;
  }
}

StringBuilder::Error StringBuilder::ResolveSubstitution(
    const google::protobuf::MessageLite& request,
    const proto::StringSubstitution& arg) {
  for (const auto& candidate : arg.candidates()) {
    if (DoConditionsApply(request, candidate.conditions())) {
      return ResolveArg(request, candidate);
    }
  }
  return Error::OK;
}

StringBuilder::Error StringBuilder::ResolveSubstitutedString(
    const google::protobuf::MessageLite& request,
    const proto::SubstitutedString& substitution) {
  if (!DoConditionsApply(request, substitution.conditions())) {
    return Error::OK;
  }
  if (substitution.should_ignore_input_context()) {
    should_ignore_input_context_ = true;
  }
  std::string_view templ = substitution.string_template();
  int32_t substitution_idx = 0;
  size_t template_idx = 0;
  for (size_t pos = templ.find('%', template_idx);
       pos != std::string_view::npos; pos = templ.find('%', template_idx)) {
    out_ << templ.substr(template_idx, pos - template_idx);
    std::string_view token = templ.substr(pos, 2);
    template_idx = pos + 2;
    if (token == "%%") {
      out_ << "%";
      continue;
    }
    if (token != "%s") {
      return Error::FAILED;  // Invalid token
    }
    if (substitution_idx >= substitution.substitutions_size()) {
      return Error::FAILED;
    }
    Error error = ResolveSubstitution(
        request, substitution.substitutions(substitution_idx));
    if (error != Error::OK) {
      return error;
    }
    ++substitution_idx;
  }
  out_ << templ.substr(template_idx, std::string_view::npos);
  if (substitution_idx != substitution.substitutions_size()) {
    return Error::FAILED;
  }
  return Error::OK;
}

}  // namespace

std::optional<SubstitutionResult> CreateSubstitutions(
    const google::protobuf::MessageLite& request,
    const google::protobuf::RepeatedPtrField<proto::SubstitutedString>&
        config_substitutions) {
  StringBuilder builder;
  for (const auto& substitution : config_substitutions) {
    auto error = builder.ResolveSubstitutedString(request, substitution);
    if (error != StringBuilder::Error::OK) {
      return std::nullopt;
    }
  }
  return builder.result();
}

}  // namespace optimization_guide
