// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/mouse_move_tool.h"

#include <optional>

#include "base/time/time.h"
#include "chrome/common/actor.mojom-shared.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_widget.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/latency/latency_info.h"

namespace actor {

using ::blink::WebWidget;

MouseMoveTool::MouseMoveTool(content::RenderFrame& frame,
                             TaskId task_id,
                             Journal& journal,
                             mojom::MouseMoveActionPtr action,
                             mojom::ToolTargetPtr target,
                             mojom::ObservedToolTargetPtr observed_target)
    : ToolBase(frame,
               task_id,
               journal,
               std::move(target),
               std::move(observed_target)),
      action_(std::move(action)) {}

MouseMoveTool::~MouseMoveTool() = default;

void MouseMoveTool::Execute(ToolFinishedCallback callback) {
  CHECK(validated_target_.has_value())
      << "Execute tool was called before validation";
  // Dispatch MouseMove event
  blink::WebMouseEvent mouse_event(blink::WebInputEvent::Type::kMouseMove,
                                   blink::WebInputEvent::kNoModifiers,
                                   ui::EventTimeForNow());
  // No button for move
  mouse_event.button = blink::WebMouseEvent::Button::kNoButton;
  mouse_event.SetPositionInWidget(validated_target_->widget_point);

  WebWidget* widget = validated_target_->GetWidget(*this);
  CHECK(widget);
  blink::WebInputEventResult move_result = widget->HandleInputEvent(
      blink::WebCoalescedInputEvent(mouse_event, ui::LatencyInfo()));

  // Note: KNotHandled probably shouldn't result in an error.
  if (move_result == blink::WebInputEventResult::kNotHandled ||
      move_result == blink::WebInputEventResult::kHandledSuppressed) {
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kMouseMoveEventSuppressed));
    return;
  }
  std::move(callback).Run(MakeOkResult());
}

std::string MouseMoveTool::DebugString() const {
  return absl::StrFormat("MouseMoveTool[%s]", ToDebugString(target_));
}

ValidationResult MouseMoveTool::Validate() {
  CHECK(frame_->GetWebFrame());
  CHECK(frame_->GetWebFrame()->FrameWidget());

  auto resolved_target = ValidateAndResolveTarget();
  if (!resolved_target.has_value()) {
    return ValidationResult(std::move(resolved_target.error()));
  }

  validated_target_ = std::move(resolved_target.value());
  return ValidationResult(MakeOkResult(), validated_target_->widget_point);
}

}  // namespace actor
