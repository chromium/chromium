// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/mouse_move_tool.h"

#include <optional>

#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "content/public/renderer/render_frame.h"
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
  // TODO(crbug.com/402082828): Set screen position if possible/needed.
  return mouse_event;
}
}  // namespace

namespace actor {

MouseMoveTool::MouseMoveTool(mojom::MouseMoveActionPtr action,
                             base::raw_ref<content::RenderFrame> frame)
    : frame_(frame), action_(std::move(action)) {}

MouseMoveTool::~MouseMoveTool() = default;

void MouseMoveTool::Execute(ToolFinishedCallback callback) {
  blink::WebLocalFrame* web_frame = frame_->GetWebFrame();
  if (!web_frame || !web_frame->FrameWidget()) {
    DLOG(ERROR) << "RenderFrame or FrameWidget is invalid.";
    std::move(callback).Run(false);
    return;
  }

  mojom::ToolTargetPtr& target = action_->target;

  // Currently only support DOMNodeId as target.
  int32_t dom_node_id = target->dom_node_id;
  CHECK(dom_node_id);

  blink::WebNode node = GetNodeFromId(frame_.get(), dom_node_id);
  if (node.IsNull()) {
    DLOG(ERROR) << "Cannot find dom node with id " << dom_node_id;
    std::move(callback).Run(false);
    return;
  }

  // Get interaction point for MouseMove
  std::optional<gfx::PointF> center_point = InteractionPointFromWebNode(node);
  if (!center_point.has_value()) {
    DLOG(ERROR) << "Cannot get center interaction point for node id "
                << dom_node_id;
    std::move(callback).Run(false);
    return;
  }

  // Dispatch MouseMove event
  blink::WebMouseEvent mouse_move = CreateMouseEvent(
      blink::WebInputEvent::Type::kMouseMove, center_point.value());

  blink::WebInputEventResult move_result =
      web_frame->FrameWidget()->HandleInputEvent(
          blink::WebCoalescedInputEvent(mouse_move, ui::LatencyInfo()));

  if (move_result == blink::WebInputEventResult::kNotHandled ||
      move_result == blink::WebInputEventResult::kHandledSuppressed) {
    DLOG(ERROR) << "MouseMove event was not handled or suppressed for node id "
                << dom_node_id;
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(true);
}

}  // namespace actor
