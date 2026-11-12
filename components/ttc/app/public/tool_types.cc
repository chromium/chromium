// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ttc/app/public/tool_types.h"

#include <utility>

#include "base/check_op.h"

namespace ttc {

ToolDefinition::ToolDefinition() = default;
ToolDefinition::~ToolDefinition() = default;
ToolDefinition::ToolDefinition(ToolDefinition&&) = default;
ToolDefinition& ToolDefinition::operator=(ToolDefinition&&) = default;

ToolDefinition ToolDefinition::Clone() const {
  ToolDefinition copy;
  copy.name = name;
  copy.description = description;
  copy.parameters_json_schema = parameters_json_schema.Clone();
  copy.behavior = behavior;
  copy.verbalization = verbalization;
  return copy;
}

ToolRequest ToolRequest::Clone() const {
  return ToolRequest{
      .name = name, .arguments = arguments.Clone(), .call_id = call_id};
}

PendingToolCall::PendingToolCall(ToolRequest request,
                                 ToolResponseCallback callback)
    : request(std::move(request)), callback(std::move(callback)) {}
PendingToolCall::PendingToolCall(PendingToolCall&&) noexcept = default;
PendingToolCall& PendingToolCall::operator=(PendingToolCall&&) noexcept =
    default;
PendingToolCall::~PendingToolCall() = default;

ToolResponse::ToolResponse(std::variant<base::DictValue, ToolError> value,
                           std::optional<VisualContent> visual_content)
    : value_(std::move(value)), visual_content_(std::move(visual_content)) {
  // The factories cannot produce a failure with nothing to report, but this
  // guards the invariant against future additions.
  if (const ToolError* error = std::get_if<ToolError>(&value_)) {
    CHECK(error->code || error->message);
  }
}

ToolResponse::ToolResponse(ToolResponse&&) noexcept = default;
ToolResponse& ToolResponse::operator=(ToolResponse&&) noexcept = default;
ToolResponse::~ToolResponse() = default;

// static
ToolResponse ToolResponse::Success(base::DictValue result) {
  return ToolResponse(std::move(result));
}

// static
ToolResponse ToolResponse::Success(base::DictValue result,
                                   VisualContent visual_content) {
  CHECK(!visual_content.image_data.empty());
  CHECK(!visual_content.mime_type.empty());
  return ToolResponse(std::move(result), std::move(visual_content));
}

// static
ToolResponse ToolResponse::Error(actor::mojom::ActionResultCode code,
                                 std::string message) {
  CHECK_NE(code, actor::mojom::ActionResultCode::kOk);
  std::optional<std::string> opt_message;
  if (!message.empty()) {
    opt_message = std::move(message);
  }
  return ToolResponse(ToolError{code, std::move(opt_message)});
}

// static
ToolResponse ToolResponse::Error(std::string message) {
  CHECK(!message.empty());
  return ToolResponse(ToolError{std::nullopt, std::move(message)});
}

bool ToolResponse::Ok() const {
  return std::holds_alternative<base::DictValue>(value_);
}

const base::DictValue& ToolResponse::GetResult() const {
  CHECK(Ok());
  return std::get<base::DictValue>(value_);
}

base::DictValue ToolResponse::TakeResult() && {
  CHECK(Ok());
  return std::move(std::get<base::DictValue>(value_));
}

const ToolError& ToolResponse::error() const {
  CHECK(!Ok());
  return std::get<ToolError>(value_);
}

}  // namespace ttc
