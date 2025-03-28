// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/click_tool.h"

#include <cstdint>
#include <optional>

#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/latency/latency_info.h"

namespace {
constexpr base::TimeDelta kClickDelay = base::Milliseconds(50);
}

namespace actor {

ClickTool::ClickTool(mojom::ClickActionPtr action,
                     base::raw_ref<content::RenderFrame> frame)
    : frame_(frame), action_(std::move(action)) {}

ClickTool::~ClickTool() = default;

blink::WebMouseEvent ClickTool::CreateClickMouseEvent(
    const blink::WebNode& node,
    mojom::ClickAction::Type type,
    mojom::ClickAction::Count count,
    blink::WebInputEvent::Type event_type,
    const gfx::PointF& click_point) {
  blink::WebMouseEvent mouse_event(
      event_type, blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow());

  switch (type) {
    case mojom::ClickAction::Type::kLeft: {
      mouse_event.button = blink::WebMouseEvent::Button::kLeft;
      break;
    }
    case mojom::ClickAction::Type::kRight: {
      mouse_event.button = blink::WebMouseEvent::Button::kRight;
      break;
    }
  }

  switch (count) {
    case mojom::ClickAction::Count::kSingle: {
      mouse_event.click_count = 1;
      break;
    }
    case mojom::ClickAction::Count::kDouble: {
      mouse_event.click_count = 2;
      break;
    }
  }

  mouse_event.SetPositionInWidget(click_point);

  // TODO(crbug.com/402082828): Find a way to set screen position.
  //   const gfx::Rect offset =
  //     render_frame_host_->GetRenderWidgetHost()->GetView()->GetViewBounds();
  //   mouse_event_.SetPositionInScreen(point.x() + offset.x(),
  //                                    point.y() + offset.y());
  return mouse_event;
}

void ClickTool::Execute(ToolFinishedCallback callback) {
  if (!frame_->GetWebFrame()->FrameWidget()) {
    DLOG(ERROR) << "RenderWidget is invalid.";
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

  std::optional<gfx::PointF> click_point = InteractionPointFromWebNode(node);
  if (!click_point.has_value()) {
    std::move(callback).Run(false);
    return;
  }

  // Create and send MouseDown event
  blink::WebMouseEvent mouse_down = CreateClickMouseEvent(
      node, action_->type, action_->count,
      blink::WebInputEvent::Type::kMouseDown, click_point.value());
  blink::WebMouseEvent mouse_up = mouse_down;
  blink::WebInputEventResult result =
      frame_->GetWebFrame()->FrameWidget()->HandleInputEvent(
          blink::WebCoalescedInputEvent(mouse_down, ui::LatencyInfo()));

  if (result == blink::WebInputEventResult::kNotHandled ||
      result == blink::WebInputEventResult::kHandledSuppressed) {
    std::move(callback).Run(false);
    return;
  }

  mouse_up.SetType(blink::WebInputEvent::Type::kMouseUp);
  mouse_up.SetTimeStamp(mouse_down.TimeStamp() + kClickDelay);

  // TODO(crbug.com/402082828): Delay the mouse up to simulate natural click
  // after ToolExecutor lifetime update.

  result = frame_->GetWebFrame()->FrameWidget()->HandleInputEvent(
      blink::WebCoalescedInputEvent(std::move(mouse_up), ui::LatencyInfo()));

  if (result == blink::WebInputEventResult::kNotHandled ||
      result == blink::WebInputEventResult::kHandledSuppressed) {
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(true);
  return;
}

}  // namespace actor
