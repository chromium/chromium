// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/tool_utils.h"

#include <sstream>

#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/latency/latency_info.h"

namespace {
constexpr base::TimeDelta kClickDelay = base::Milliseconds(50);
}

namespace actor {

using ::blink::WebCoalescedInputEvent;
using ::blink::WebFrameWidget;
using ::blink::WebInputEvent;
using ::blink::WebInputEventResult;
using ::blink::WebMouseEvent;
using ::blink::WebNode;

std::optional<gfx::PointF> InteractionPointFromWebNode(
    const blink::WebNode& node) {
  blink::WebElement element = node.DynamicTo<blink::WebElement>();
  if (element.IsNull()) {
    return std::nullopt;
  }

  gfx::Rect rect = element.VisibleBoundsInWidget();
  if (rect.IsEmpty()) {
    return std::nullopt;
  }

  return gfx::PointF(rect.CenterPoint());
}

blink::WebNode GetNodeFromId(const content::RenderFrame& frame,
                             int32_t node_id) {
  const blink::WebLocalFrame* web_frame = frame.GetWebFrame();
  if (!web_frame) {
    return blink::WebNode();
  }

  blink::WebNode node = blink::WebNode::FromDomNodeId(node_id);
  // Make sure the node we're getting belongs to the document inside this
  // frame.
  if (node.IsNull() || node.GetDocument() != web_frame->GetDocument()) {
    return blink::WebNode();
  }
  return node;
}

bool IsNodeFocused(const content::RenderFrame& frame,
                   const blink::WebNode& node) {
  blink::WebDocument document = frame.GetWebFrame()->GetDocument();
  blink::WebElement currently_focused = document.FocusedElement();
  blink::WebElement element = node.To<blink::WebElement>();
  return element == currently_focused;
}

bool IsPointWithinViewport(const gfx::PointF& point,
                           const content::RenderFrame& frame) {
  gfx::Rect viewport(frame.GetWebFrame()->FrameWidget()->VisibleViewportSize());
  return viewport.Contains(gfx::ToFlooredPoint(point));
}

std::string ToDebugString(const mojom::ToolTargetPtr& target) {
  if (target.is_null()) {
    return "target(null)";
  }

  std::stringstream ss;
  ss << "target(";
  if (target->is_coordinate()) {
    ss << "XY=" << target->get_coordinate().x() << ","
       << target->get_coordinate().y();
  } else {
    ss << "ID=" << target->get_dom_node_id();
  }
  ss << ")";
  return ss.str();
}

mojom::ActionResultPtr CreateAndDispatchClick(WebMouseEvent::Button button,
                                              int count,
                                              const gfx::PointF& click_point,
                                              WebFrameWidget* widget) {
  WebMouseEvent mouse_down(WebInputEvent::Type::kMouseDown,
                           WebInputEvent::kNoModifiers, ui::EventTimeForNow());
  mouse_down.button = button;
  mouse_down.click_count = count;
  mouse_down.SetPositionInWidget(click_point);
  // TODO(crbug.com/402082828): Find a way to set screen position.
  //   const gfx::Rect offset =
  //     render_frame_host_->GetRenderWidgetHost()->GetView()->GetViewBounds();
  //   mouse_event_.SetPositionInScreen(point.x() + offset.x(),
  //                                    point.y() + offset.y());

  WebMouseEvent mouse_up = mouse_down;
  WebInputEventResult result = widget->HandleInputEvent(
      WebCoalescedInputEvent(mouse_down, ui::LatencyInfo()));

  if (result == WebInputEventResult::kHandledSuppressed) {
    return MakeResult(mojom::ActionResultCode::kClickSuppressed);
  }

  mouse_up.SetType(WebInputEvent::Type::kMouseUp);
  mouse_up.SetTimeStamp(mouse_down.TimeStamp() + kClickDelay);

  // TODO(crbug.com/402082828): Delay the mouse up to simulate natural click
  // after ToolExecutor lifetime update.

  result = widget->HandleInputEvent(
      WebCoalescedInputEvent(std::move(mouse_up), ui::LatencyInfo()));

  if (result == WebInputEventResult::kHandledSuppressed) {
    return MakeResult(mojom::ActionResultCode::kClickSuppressed);
  }

  return MakeOkResult();
}

}  // namespace actor
