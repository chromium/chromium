// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/no_op_tool.h"

#include "base/notimplemented.h"
#include "chrome/common/actor.mojom-shared.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "ui/events/base_event_utils.h"

namespace actor {

NoOpTool::NoOpTool(content::RenderFrame& frame,
                   TaskId task_id,
                   Journal& journal,
                   mojom::ToolTargetPtr target,
                   mojom::ObservedToolTargetPtr observed_target)
    : ToolBase(frame,
               task_id,
               journal,
               std::move(target),
               std::move(observed_target)) {}

NoOpTool::~NoOpTool() = default;

void NoOpTool::Execute(ToolFinishedCallback callback) {
  CHECK(validated_target_.has_value())
      << "Execute tool was called before validation";
  std::move(callback).Run(MakeOkResult());
}

std::string NoOpTool::DebugString() const {
  return absl::StrFormat("NoOpTool[%s]", ToDebugString(target_));
}

ValidationResult NoOpTool::Validate() {
  CHECK(frame_->GetWebFrame());
  CHECK(frame_->GetWebFrame()->FrameWidget());

  auto resolved_target = ValidateAndResolveTarget();
  if (!resolved_target.has_value()) {
    return ValidationResult(std::move(resolved_target.error()));
  }

  validated_target_ = std::move(resolved_target.value());
  return ValidationResult(MakeOkResult());
}

}  // namespace actor
