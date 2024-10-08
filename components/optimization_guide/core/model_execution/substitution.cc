// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/substitution.h"

#include <sys/types.h>

#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_value_utils.h"
#include "components/optimization_guide/proto/descriptors.pb.h"
#include "components/optimization_guide/proto/substitution.pb.h"
#include "services/on_device_model/ml/chrome_ml_types.h"

namespace optimization_guide {

namespace {

using google::protobuf::RepeatedPtrField;
using on_device_model::mojom::Input;
using on_device_model::mojom::InputPiece;
using on_device_model::mojom::InputPtr;

// A context for resolving substitution expressions.
struct ResolutionContext {
  // The message we are resolving expressions against.
  raw_ptr<const google::protobuf::MessageLite> message;

  // 0-based index of 'message' in the repeated field that contains it.
  // 0 for the top level message.
  size_t offset = 0;
};

// Returns whether `condition` applies based on `message`.
bool EvaluateCondition(const ResolutionContext& ctx,
                       const proto::Condition& condition) {
  std::optional<proto::Value> proto_value =
      GetProtoValue(*ctx.message, condition.proto_field());
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

bool AndConditions(const ResolutionContext& ctx,
                   const RepeatedPtrField<proto::Condition>& conditions) {
  for (const auto& condition : conditions) {
    if (!EvaluateCondition(ctx, condition)) {
      return false;
    }
  }
  return true;
}

bool OrConditions(const ResolutionContext& ctx,
                  const RepeatedPtrField<proto::Condition>& conditions) {
  for (const auto& condition : conditions) {
    if (EvaluateCondition(ctx, condition)) {
      return true;
    }
  }
  return false;
}

// Returns whether `conditions` apply based on `message`.
bool DoConditionsApply(const ResolutionContext& ctx,
                       const proto::ConditionList& conditions) {
  if (conditions.conditions_size() == 0) {
    return true;
  }

  switch (conditions.condition_evaluation_type()) {
    case proto::CONDITION_EVALUATION_TYPE_OR:
      return OrConditions(ctx, conditions.conditions());
    case proto::CONDITION_EVALUATION_TYPE_AND:
      return AndConditions(ctx, conditions.conditions());
    default:
      base::debug::DumpWithoutCrashing();
      return false;
  }
}

// Resolve various expression in proto::SubstitutedString by appending
// appropriate text to an output string and updating state.
// Methods return false on error.
class InputBuilder final {
 public:
  enum class Error {
    OK = 0,
    FAILED = 1,
  };
  InputBuilder() : out_(Input::New()) {}
  Error ResolveSubstitutedString(const ResolutionContext& ctx,
                                 const proto::SubstitutedString& substitution);

  SubstitutionResult result() && {
    SubstitutionResult res;
    res.input = std::move(out_);
    res.should_ignore_input_context = should_ignore_input_context_;
    return res;
  }

 private:
  Error ResolveSubstitution(const ResolutionContext& ctx,
                            const proto::StringSubstitution& arg);
  Error ResolveStringArg(const ResolutionContext& ctx,
                         const proto::StringArg& candidate);

  Error ResolveProtoField(const ResolutionContext& ctx,
                          const proto::ProtoField& field);
  Error ResolveRangeExpr(const ResolutionContext& ctx,
                         const proto::RangeExpr& expr);
  Error ResolveIndexExpr(const ResolutionContext& ctx,
                         const proto::IndexExpr& field);
  Error ResolveControlToken(const ResolutionContext& ctx,
                            proto::ControlToken token);

  void AddToken(ml::Token token) { out_->pieces.emplace_back(token); }

  void AddString(std::string_view str) {
    out_->pieces.emplace_back(std::string(str));
  }

  InputPtr out_;
  bool should_ignore_input_context_ = false;
};

InputBuilder::Error InputBuilder::ResolveProtoField(
    const ResolutionContext& ctx,
    const proto::ProtoField& field) {
  std::optional<proto::Value> value = GetProtoValue(*ctx.message, field);
  if (!value) {
    DVLOG(1) << "Invalid proto field of " << ctx.message->GetTypeName();
    return Error::FAILED;
  }
  AddString(GetStringFromValue(*value));
  return Error::OK;
}

InputBuilder::Error InputBuilder::ResolveRangeExpr(
    const ResolutionContext& ctx,
    const proto::RangeExpr& expr) {
  std::vector<std::string> vals;
  auto it = GetProtoRepeated(ctx.message, expr.proto_field());
  if (!it) {
    DVLOG(1) << "Invalid proto field for RangeExpr over "
             << ctx.message->GetTypeName();
    return Error::FAILED;
  }
  size_t i = 0;
  for (const auto* msg : *it) {
    Error error =
        ResolveSubstitutedString(ResolutionContext{msg, i++}, expr.expr());
    if (error != Error::OK) {
      return error;
    }
  }
  return Error::OK;
}

InputBuilder::Error InputBuilder::ResolveIndexExpr(
    const ResolutionContext& ctx,
    const proto::IndexExpr& expr) {
  AddString(base::NumberToString(ctx.offset + expr.one_based()));
  return Error::OK;
}

InputBuilder::Error InputBuilder::ResolveControlToken(
    const ResolutionContext& ctx,
    proto::ControlToken token) {
  switch (token) {
    case proto::CONTROL_TOKEN_SYSTEM:
      AddToken(ml::Token::kSystem);
      break;
    case proto::CONTROL_TOKEN_MODEL:
      AddToken(ml::Token::kModel);
      break;
    case proto::CONTROL_TOKEN_USER:
      AddToken(ml::Token::kUser);
      break;
    case proto::CONTROL_TOKEN_END:
      AddToken(ml::Token::kEnd);
      break;
    default:
      return Error::FAILED;
  }
  return Error::OK;
}

InputBuilder::Error InputBuilder::ResolveStringArg(
    const ResolutionContext& ctx,
    const proto::StringArg& candidate) {
  switch (candidate.arg_case()) {
    case proto::StringArg::kRawString:
      AddString(candidate.raw_string());
      return Error::OK;
    case proto::StringArg::kProtoField:
      return ResolveProtoField(ctx, candidate.proto_field());
    case proto::StringArg::kRangeExpr:
      return ResolveRangeExpr(ctx, candidate.range_expr());
    case proto::StringArg::kIndexExpr:
      return ResolveIndexExpr(ctx, candidate.index_expr());
    case proto::StringArg::kControlToken:
      return ResolveControlToken(ctx, candidate.control_token());
    case proto::StringArg::ARG_NOT_SET:
      DVLOG(1) << "StringArg is incomplete.";
      return Error::FAILED;
  }
}

InputBuilder::Error InputBuilder::ResolveSubstitution(
    const ResolutionContext& ctx,
    const proto::StringSubstitution& arg) {
  for (const auto& candidate : arg.candidates()) {
    if (DoConditionsApply(ctx, candidate.conditions())) {
      return ResolveStringArg(ctx, candidate);
    }
  }
  return Error::OK;
}

InputBuilder::Error InputBuilder::ResolveSubstitutedString(
    const ResolutionContext& ctx,
    const proto::SubstitutedString& substitution) {
  if (!DoConditionsApply(ctx, substitution.conditions())) {
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
    AddString(templ.substr(template_idx, pos - template_idx));
    std::string_view token = templ.substr(pos, 2);
    template_idx = pos + 2;
    if (token == "%%") {
      AddString("%");
      continue;
    }
    if (token != "%s") {
      DVLOG(1) << "Invalid Token";
      return Error::FAILED;  // Invalid token
    }
    if (substitution_idx >= substitution.substitutions_size()) {
      DVLOG(1) << "Too many substitutions";
      return Error::FAILED;
    }
    Error error =
        ResolveSubstitution(ctx, substitution.substitutions(substitution_idx));
    if (error != Error::OK) {
      return error;
    }
    ++substitution_idx;
  }
  AddString(templ.substr(template_idx, std::string_view::npos));
  if (substitution_idx != substitution.substitutions_size()) {
    DVLOG(1) << "Missing substitutions";
    return Error::FAILED;
  }
  return Error::OK;
}

// Placeholder strings for a control token in MQLS logs / display.
std::string PlaceholderForToken(ml::Token token) {
  switch (token) {
    case ml::Token::kSystem:
      return "<system>";
    case ml::Token::kModel:
      return "<model>";
    case ml::Token::kUser:
      return "<user>";
    case ml::Token::kEnd:
      return "<end>";
  }
}

}  // namespace

SubstitutionResult::SubstitutionResult() = default;
SubstitutionResult::~SubstitutionResult() = default;
SubstitutionResult::SubstitutionResult(SubstitutionResult&&) = default;
SubstitutionResult& SubstitutionResult::operator=(SubstitutionResult&&) =
    default;

std::string OnDeviceInputToString(const on_device_model::mojom::Input& input) {
  std::ostringstream oss;
  for (const auto& piece : input.pieces) {
    if (std::holds_alternative<std::string>(piece)) {
      oss << std::get<std::string>(piece);
    }
    if (std::holds_alternative<ml::Token>(piece)) {
      oss << PlaceholderForToken(std::get<ml::Token>(piece));
    }
  }
  return oss.str();
}

std::string SubstitutionResult::ToString() const {
  return OnDeviceInputToString(*input);
}

std::optional<SubstitutionResult> CreateSubstitutions(
    const google::protobuf::MessageLite& request,
    const google::protobuf::RepeatedPtrField<proto::SubstitutedString>&
        config_substitutions) {
  InputBuilder builder;
  for (const auto& substitution : config_substitutions) {
    auto error = builder.ResolveSubstitutedString(
        ResolutionContext{&request, 0}, substitution);
    if (error != InputBuilder::Error::OK) {
      return std::nullopt;
    }
  }
  return std::move(builder).result();
}

}  // namespace optimization_guide
