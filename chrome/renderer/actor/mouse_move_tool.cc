// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/mouse_move_tool.h"

#include <optional>

#include "base/time/time.h"
#include "chrome/common/actor.mojom-data-view.h"
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
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/latency/latency_info.h"

namespace {
blink::WebMouseEvent CreateMouseEvent(blink::WebInputEvent::Type event_type,
                                      const gfx::PointF& position) {
  blink::WebMouseEvent mouse_event(
      event_type, blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow());
  // No button for move
  mouse_event.button = blink::WebMouseEvent::Button::kNoButton;
  mouse_event.SetPositionInWidget(position);
  return mouse_event;
}
}  // namespace

namespace actor {

MouseMoveTool::MouseMoveTool(mojom::MouseMoveActionPtr action,
                             content::RenderFrame& frame)
    : frame_(frame), action_(std::move(action)) {}

MouseMoveTool::~MouseMoveTool() = default;

void MouseMoveTool::Execute(ToolFinishedCallback callback) {
  ValidatedResult validated_result = Validate();
  if (!validated_result.has_value()) {
    std::move(callback).Run(std::move(validated_result.error()));
    return;
  }

  gfx::PointF move_point = validated_result.value();

  // Dispatch MouseMove event
  blink::WebMouseEvent mouse_move =
      CreateMouseEvent(blink::WebInputEvent::Type::kMouseMove, move_point);

  blink::WebInputEventResult move_result =
      frame_->GetWebFrame()->FrameWidget()->HandleInputEvent(
          blink::WebCoalescedInputEvent(mouse_move, ui::LatencyInfo()));

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
  return absl::StrFormat("MouseMoveTool[%s]", ToDebugString(action_->target));
}

MouseMoveTool::ValidatedResult MouseMoveTool::Validate() const {
  if (!frame_->GetWebFrame() || !frame_->GetWebFrame()->FrameWidget()) {
    return base::unexpected(
        MakeResult(mojom::ActionResultCode::kFrameWentAway));
  }

  if (action_->target->is_coordinate()) {
    gfx::PointF move_point = gfx::PointF(action_->target->get_coordinate());
    if (!IsPointWithinViewport(move_point, frame_.get())) {
      return base::unexpected(
          MakeResult(mojom::ActionResultCode::kCoordinatesOutOfBounds,
                     absl::StrFormat("Point [%s]", move_point.ToString())));
    }

    return move_point;
  }

  blink::WebNode node =
      GetNodeFromId(frame_.get(), action_->target->get_dom_node_id());
  if (node.IsNull()) {
    return base::unexpected(
        MakeResult(mojom::ActionResultCode::kInvalidDomNodeId));
  }

  std::optional<gfx::PointF> move_point = InteractionPointFromWebNode(node);
  if (!move_point.has_value()) {
    return base::unexpected(
        MakeResult(mojom::ActionResultCode::kElementOffscreen));
  }

  return *move_point;
}

}  // namespace actor
