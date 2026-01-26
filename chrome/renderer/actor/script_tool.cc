// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/script_tool.h"

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
    base::expected<blink::WebString, blink::WebDocument::ScriptToolError>
        response) {
  if (!response.has_value()) {
    switch (response.error()) {
      case blink::WebDocument::ScriptToolError::kInvalidToolName:
        return MakeResult(mojom::ActionResultCode::kScriptToolInvalidName);
      case blink::WebDocument::ScriptToolError::kInvalidInputArguments:
        return MakeResult(
            mojom::ActionResultCode::kScriptToolInvalidInputArguments);
      case blink::WebDocument::ScriptToolError::kToolInvocationFailed:
        return MakeResult(mojom::ActionResultCode::kScriptToolInvocationFailed);
    }
    NOTREACHED();
  }

  auto result = MakeOkResult();
  auto script_tool_response = mojom::ScriptToolResponse::New();
  script_tool_response->name = name;
  script_tool_response->input_arguments = input_arguments;
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
  frame_->GetWebFrame()->GetDocument().ExecuteScriptTool(
      blink::WebString::FromUTF8(action_->name),
      blink::WebString::FromUTF8(action_->input_arguments),
      base::BindOnce(&OnToolExecuted, action_->name, action_->input_arguments)
          .Then(std::move(callback)));
}

std::string ScriptTool::DebugString() const {
  return absl::StrFormat("ScriptTool[tool_name(%s);input_arguments(%s)]",
                         action_->name, action_->input_arguments);
}

}  // namespace actor
