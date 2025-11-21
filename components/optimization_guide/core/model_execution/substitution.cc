// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/substitution.h"

#include <sys/types.h>

#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_value_utils.h"
#include "components/optimization_guide/proto/descriptors.pb.h"
#include "components/optimization_guide/proto/substitution.pb.h"
#include "services/on_device_model/ml/chrome_ml_audio_buffer.h"
#include "services/on_device_model/ml/chrome_ml_types.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace optimization_guide {

namespace {

using google::protobuf::RepeatedPtrField;
using on_device_model::mojom::Input;
using on_device_model::mojom::InputPiece;
using on_device_model::mojom::InputPtr;

// A context for resolving substitution expressions.
struct ResolutionContext {
  // The message we are resolving expressions against.
  MultimodalMessageReadView view;

  // 0-based index of 'message' in the repeated field that contains it.
  // 0 for the top level message.
  int offset = 0;
};

enum class ConditionResult { kFalse, kTrue, kStop };

ConditionResult AsResult(bool value) {
  return value ? ConditionResult::kTrue : ConditionResult::kFalse;
}

// Returns whether `condition` applies based on `message`.
ConditionResult EvaluateCondition(const ResolutionContext& ctx,
                                  const proto::Condition& condition) {
  if (ctx.view.IsPending(condition.proto_field())) {
    return ConditionResult::kStop;
  }
  std::optional<proto::Value> proto_value =
      ctx.view.GetValue(condition.proto_field());
  if (!proto_value) {
    return AsResult(false);
  }

  switch (condition.operator_type()) {
    case proto::OPERATOR_TYPE_EQUAL_TO:
      return AsResult(AreValuesEqual(*proto_value, condition.value()));
    case proto::OPERATOR_TYPE_NOT_EQUAL_TO:
      return AsResult(!AreValuesEqual(*proto_value, condition.value()));
    default:
      base::debug::DumpWithoutCrashing();
      return AsResult(false);
  }
}

ConditionResult AndConditions(
    const ResolutionContext& ctx,
    const RepeatedPtrField<proto::Condition>& conditions) {
  for (const auto& condition : conditions) {
    ConditionResult result = EvaluateCondition(ctx, condition);
    if (result != ConditionResult::kTrue) {
      return result;
    }
  }
  return ConditionResult::kTrue;
}

ConditionResult OrConditions(
    const ResolutionContext& ctx,
    const RepeatedPtrField<proto::Condition>& conditions) {
  for (const auto& condition : conditions) {
    ConditionResult result = EvaluateCondition(ctx, condition);
    if (result != ConditionResult::kFalse) {
      return result;
    }
  }
  return ConditionResult::kFalse;
}

// Returns whether `conditions` apply based on `message`.
ConditionResult DoConditionsApply(const ResolutionContext& ctx,
                                  const proto::ConditionList& conditions) {
  if (conditions.conditions_size() == 0) {
    return ConditionResult::kTrue;
  }

  switch (conditions.condition_evaluation_type()) {
    case proto::CONDITION_EVALUATION_TYPE_OR:
      return OrConditions(ctx, conditions.conditions());
    case proto::CONDITION_EVALUATION_TYPE_AND:
      return AndConditions(ctx, conditions.conditions());
    default:
      base::debug::DumpWithoutCrashing();
      return ConditionResult::kFalse;
  }
}

// Resolve various expression in proto::SubstitutedString by appending
// appropriate text to an output string and updating state.
// Methods return false on error.
class InputBuilder final {
 public:
  enum class Error {
    kOk = 0,
    kFailed = 1,  // The config is not valid over this input.
    kStop = 2,    // Terminate early due to a pending field.
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
  Error ResolveMediaField(const ResolutionContext& ctx,
                          proto::MediaField token);

  void AddToken(ml::Token token) { out_->pieces.emplace_back(token); }

  void AddString(std::string_view str) {
    if (!str.empty()) {
      out_->pieces.emplace_back(std::string(str));
    }
  }

  InputPtr out_;
  bool should_ignore_input_context_ = false;
};

InputBuilder::Error InputBuilder::ResolveProtoField(
    const ResolutionContext& ctx,
    const proto::ProtoField& field) {
  if (ctx.view.IsPending(field)) {
    return Error::kStop;
  }
  std::optional<proto::Value> value = ctx.view.GetValue(field);
  if (!value) {
    DVLOG(1) << "Invalid proto field of " << ctx.view.GetTypeName();
    return Error::kFailed;
  }
  AddString(GetStringFromValue(*value));
  return Error::kOk;
}

InputBuilder::Error InputBuilder::ResolveRangeExpr(
    const ResolutionContext& ctx,
    const proto::RangeExpr& expr) {
  if (ctx.view.IsPending(expr.proto_field())) {
    return Error::kStop;
  }
  auto repeated = ctx.view.GetRepeated(expr.proto_field());
  if (!repeated) {
    DVLOG(1) << "Invalid proto field for RangeExpr over "
             << ctx.view.GetTypeName();
    return Error::kFailed;
  }
  int repeated_size = repeated->Size();
  for (int i = 0; i < repeated_size; i++) {
    Error error = ResolveSubstitutedString(
        ResolutionContext{repeated->Get(i), i}, expr.expr());
    if (error != Error::kOk) {
      return error;
    }
  }
  if (repeated->IsIncomplete()) {
    return Error::kStop;
  }
  return Error::kOk;
}

InputBuilder::Error InputBuilder::ResolveIndexExpr(
    const ResolutionContext& ctx,
    const proto::IndexExpr& expr) {
  AddString(base::NumberToString(ctx.offset + expr.one_based()));
  return Error::kOk;
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
      return Error::kFailed;
  }
  return Error::kOk;
}

InputBuilder::Error InputBuilder::ResolveMediaField(
    const ResolutionContext& ctx,
    proto::MediaField field) {
  if (ctx.view.IsPending(field.proto_field())) {
    return Error::kStop;
  }
  MultimodalType mtype = ctx.view.GetMultimodalType(field.proto_field());
  switch (mtype) {
    case MultimodalType::kAudio:
      out_->pieces.emplace_back(*ctx.view.GetAudio(field.proto_field()));
      return Error::kOk;
    case MultimodalType::kImage:
      out_->pieces.emplace_back(*ctx.view.GetImage(field.proto_field()));
      return Error::kOk;
    case MultimodalType::kNone:
      return Error::kOk;
  }
}

InputBuilder::Error InputBuilder::ResolveStringArg(
    const ResolutionContext& ctx,
    const proto::StringArg& candidate) {
  switch (candidate.arg_case()) {
    case proto::StringArg::kRawString:
      AddString(candidate.raw_string());
      return Error::kOk;
    case proto::StringArg::kProtoField:
      return ResolveProtoField(ctx, candidate.proto_field());
    case proto::StringArg::kRangeExpr:
      return ResolveRangeExpr(ctx, candidate.range_expr());
    case proto::StringArg::kIndexExpr:
      return ResolveIndexExpr(ctx, candidate.index_expr());
    case proto::StringArg::kControlToken:
      return ResolveControlToken(ctx, candidate.control_token());
    case proto::StringArg::kMediaField:
      return ResolveMediaField(ctx, candidate.media_field());
    case proto::StringArg::ARG_NOT_SET:
      DVLOG(1) << "StringArg is incomplete.";
      return Error::kFailed;
  }
}

InputBuilder::Error InputBuilder::ResolveSubstitution(
    const ResolutionContext& ctx,
    const proto::StringSubstitution& arg) {
  for (const auto& candidate : arg.candidates()) {
    switch (DoConditionsApply(ctx, candidate.conditions())) {
      case ConditionResult::kFalse:
        continue;
      case ConditionResult::kStop:
        return Error::kStop;
      case ConditionResult::kTrue:
        return ResolveStringArg(ctx, candidate);
    }
  }
  return Error::kOk;
}

InputBuilder::Error InputBuilder::ResolveSubstitutedString(
    const ResolutionContext& ctx,
    const proto::SubstitutedString& substitution) {
  switch (DoConditionsApply(ctx, substitution.conditions())) {
    case ConditionResult::kFalse:
      return Error::kOk;
    case ConditionResult::kStop:
      return Error::kStop;
    case ConditionResult::kTrue:
      break;
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
      return Error::kFailed;  // Invalid token
    }
    if (substitution_idx >= substitution.substitutions_size()) {
      DVLOG(1) << "Too many substitutions";
      return Error::kFailed;
    }
    Error error =
        ResolveSubstitution(ctx, substitution.substitutions(substitution_idx));
    if (error != Error::kOk) {
      return error;
    }
    ++substitution_idx;
  }
  AddString(templ.substr(template_idx, std::string_view::npos));
  if (substitution_idx != substitution.substitutions_size()) {
    DVLOG(1) << "Missing substitutions";
    return Error::kFailed;
  }
  return Error::kOk;
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
    case ml::Token::kToolCall:
      return "<tool-call>";
    case ml::Token::kToolResponse:
      return "<tool-response>";
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
    } else if (std::holds_alternative<ml::Token>(piece)) {
      oss << PlaceholderForToken(std::get<ml::Token>(piece));
    } else if (std::holds_alternative<SkBitmap>(piece)) {
      oss << "<image>";
    } else if (std::holds_alternative<ml::AudioBuffer>(piece)) {
      oss << "<audio>";
    } else {
      NOTREACHED();
    }
  }
  return oss.str();
}

std::string SubstitutionResult::ToString() const {
  return OnDeviceInputToString(*input);
}

std::optional<SubstitutionResult> CreateSubstitutions(
    MultimodalMessageReadView request,
    const google::protobuf::RepeatedPtrField<proto::SubstitutedString>&
        config_substitutions) {
  InputBuilder builder;
  for (const auto& substitution : config_substitutions) {
    auto error = builder.ResolveSubstitutedString(ResolutionContext{request, 0},
                                                  substitution);
    if (error == InputBuilder::Error::kStop) {
      break;
    }
    if (error != InputBuilder::Error::kOk) {
      return std::nullopt;
    }
  }
  return std::move(builder).result();
}

}  // namespace optimization_guide
