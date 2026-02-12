// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/script_tool.h"

#include <memory>
#include <optional>

#include "base/notimplemented.h"
#include "base/strings/to_string.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_constants.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/mojom/content_extraction/script_tools.mojom.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/events/base_event_utils.h"

namespace actor {
namespace {

mojom::ActionResultPtr OnToolExecuted(
    const std::string& name,
    const std::string& input_arguments,
    std::unique_ptr<blink::WebDocument::ScriptToolDeclaration> tool,
    base::expected<blink::WebString, blink::WebDocument::ScriptToolError>
        response) {
  if (!response.has_value()) {
    mojom::ActionResultCode code;
    switch (response.error().code) {
      case blink::WebDocument::ScriptToolError::kInvalidToolName:
        code = mojom::ActionResultCode::kScriptToolInvalidName;
        break;
      case blink::WebDocument::ScriptToolError::kInvalidInputArguments:
        code = mojom::ActionResultCode::kScriptToolInvalidInputArguments;
        break;
      case blink::WebDocument::ScriptToolError::kMissingRequiredSubmitButton:
        code = mojom::ActionResultCode::kScriptToolMissingRequiredSubmitButton;
        break;
      case blink::WebDocument::ScriptToolError::kToolInvocationFailed:
        code = mojom::ActionResultCode::kScriptToolInvocationFailed;
        break;
      case blink::WebDocument::ScriptToolError::kToolCancelled:
        code = mojom::ActionResultCode::kScriptToolCancelled;
        break;
    }
    return MakeResult(code, /*requires_page_stabilization=*/false,
                      response.error().message.Utf8());
  }

  auto result = MakeOkResult();
  auto script_tool_response = mojom::ScriptToolResponse::New();
  script_tool_response->name = name;
  script_tool_response->input_arguments = input_arguments;
  script_tool_response->tool = blink::mojom::ScriptTool::New();
  script_tool_response->tool->name = name;
  script_tool_response->tool->description = tool->description.Utf8();
  script_tool_response->tool->input_schema = tool->input_schema.Utf8();
  if (tool->read_only.has_value()) {
    script_tool_response->tool->annotations =
        blink::mojom::ScriptToolAnnotations::New();
    script_tool_response->tool->annotations->read_only =
        tool->read_only.value();
  }
  if (!response->IsEmpty()) {
    script_tool_response->result = response->Utf8();
  }
  result->script_tool_response = std::move(script_tool_response);

  return result;
}

}  // namespace

ScriptTool::ScriptTool(content::RenderFrame& frame,
                       TaskId task_id,
                       Journal& journal,
                       mojom::ToolTargetPtr target,
                       mojom::ObservedToolTargetPtr observed_target,
                       mojom::ScriptToolActionPtr action)
    : ToolBase(frame,
               task_id,
               journal,
               std::move(target),
               std::move(observed_target)),
      action_(std::move(action)) {}

ScriptTool::~ScriptTool() = default;

void ScriptTool::Execute(ToolFinishedCallback callback) {
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  std::optional<uint32_t> execution_id =
      frame_->GetWebFrame()->GetDocument().ExecuteScriptTool(
          blink::WebString::FromUTF8(action_->name),
          blink::WebString::FromUTF8(action_->input_arguments),
          base::BindOnce(&OnToolExecuted, action_->name,
                         action_->input_arguments)
              .Then(std::move(callback)));
  // If the tool completed synchronously, `this` is now destroyed
  // via a tool_.reset() call in ToolExecutor::ToolFinished().
  // We can only write to execution_id_ if this object is still alive.
  if (weak_this) {
    execution_id_ = execution_id;
  }
}

void ScriptTool::Cancel() {
  if (!execution_id_.has_value()) {
    return;
  }
  frame_->GetWebFrame()->GetDocument().CancelScriptTool(execution_id_.value());
  execution_id_.reset();
}

std::string ScriptTool::DebugString() const {
  return absl::StrFormat("ScriptTool[tool_name(%s);input_arguments(%s)]",
                         action_->name, action_->input_arguments);
}

ValidationResult ScriptTool::Validate() {
  return ValidationResult(MakeOkResult());
}

}  // namespace actor
