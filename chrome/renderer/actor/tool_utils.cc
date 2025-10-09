// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/tool_utils.h"

#include <sstream>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/renderer/actor/tool_base.h"
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

blink::WebNode GetNodeFromId(const content::RenderFrame& local_root_frame,
                             int32_t node_id) {
  const blink::WebLocalFrame* web_frame = local_root_frame.GetWebFrame();
  if (!web_frame) {
    return blink::WebNode();
  }

  // The passed in frame must be a local root.
  CHECK_EQ(web_frame, web_frame->LocalRoot());

  blink::WebNode node = blink::WebNode::FromDomNodeId(node_id);
  // Make sure the node we're getting belongs to a frame under the local root
  // frame.
  if (node.IsNull() || !node.GetDocument() || !node.GetDocument().GetFrame() ||
      node.GetDocument().GetFrame()->LocalRoot() != web_frame) {
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

bool IsPointWithinViewport(const gfx::Point& point,
                           const content::RenderFrame& frame) {
  CHECK(frame.GetWebFrame());
  CHECK_EQ(frame.GetWebFrame(), frame.GetWebFrame()->LocalRoot());
  gfx::Rect viewport(frame.GetWebFrame()->FrameWidget()->VisibleViewportSize());
  return viewport.Contains(point);
}

bool IsPointWithinViewport(const gfx::PointF& point,
                           const content::RenderFrame& frame) {
  return IsPointWithinViewport(gfx::ToFlooredPoint(point), frame);
}

std::string ToDebugString(const mojom::ToolTargetPtr& target) {
  if (target.is_null()) {
    return "target(null)";
  }

  std::stringstream ss;
  ss << "target(";
  if (target->is_coordinate_dip()) {
    ss << "XY[DIP]=" << target->get_coordinate_dip().x() << ","
       << target->get_coordinate_dip().y();
  } else {
    ss << "ID=" << target->get_dom_node_id();
  }
  ss << ")";
  return ss.str();
}

bool IsNodeWithinViewport(const blink::WebNode& node) {
  blink::WebElement element = node.DynamicTo<blink::WebElement>();
  if (element.IsNull()) {
    return false;
  }

  gfx::Rect rect = element.VisibleBoundsInWidget();
  return !rect.IsEmpty();
}

void CreateAndDispatchClick(
    WebMouseEvent::Button button,
    int count,
    const gfx::PointF& click_point,
    base::WeakPtr<ToolBase> tool,
    base::OnceCallback<void(mojom::ActionResultPtr)> on_complete) {
  if (!tool) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(on_complete),
                       MakeResult(mojom::ActionResultCode::kExecutorDestroyed,
                                  /*requires_page_stabilization=*/true,
                                  "Tool destroyed before click.")));
    return;
  }

  WebFrameWidget* widget = tool->frame()->GetWebFrame()->FrameWidget();
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
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(on_complete),
                       MakeResult(mojom::ActionResultCode::kClickSuppressed,
                                  /*requires_page_stabilization=*/false)));
    return;
  }

  mouse_up.SetType(WebInputEvent::Type::kMouseUp);

  const base::TimeDelta delay = features::kGlicActorClickDelay.Get();

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](blink::WebMouseEvent mouse_up, base::WeakPtr<ToolBase> tool,
             base::OnceCallback<void(mojom::ActionResultPtr)> on_complete) {
            if (!tool) {
              std::move(on_complete)
                  .Run(MakeResult(mojom::ActionResultCode::kExecutorDestroyed,
                                  /*requires_page_stabilization=*/true,
                                  "Tool destroyed before mouse up."));
              return;
            }
            blink::WebLocalFrame* web_frame = tool->frame()->GetWebFrame();
            if (!web_frame || !web_frame->FrameWidget()) {
              std::move(on_complete)
                  .Run(MakeResult(
                      mojom::ActionResultCode::kFrameWentAway,
                      /*requires_page_stabilization=*/false,
                      "WebFrame or WebFrameWidget was null before mouse up."));
              return;
            }
            mouse_up.SetTimeStamp(ui::EventTimeForNow());
            WebInputEventResult result =
                web_frame->FrameWidget()->HandleInputEvent(
                    WebCoalescedInputEvent(std::move(mouse_up),
                                           ui::LatencyInfo()));
            if (result == WebInputEventResult::kHandledSuppressed) {
              std::move(on_complete)
                  .Run(MakeResult(mojom::ActionResultCode::kClickSuppressed,
                                  /*requires_page_stabilization=*/true));
              return;
            }
            std::move(on_complete).Run(MakeOkResult());
          },
          std::move(mouse_up), std::move(tool), std::move(on_complete)),
      delay);
}

std::string NodeToDebugSring(const blink::WebNode& node) {
  if (node.IsTextNode()) {
    // Truncate it to 100 characters, enough for debugging.
    return base::StrCat({"text=", node.NodeValue().Substring(0u, 100u).Utf8()});
  }
  if (node.IsElementNode()) {
    const blink::WebElement element = node.DynamicTo<blink::WebElement>();
    return base::StrCat({element.TagName().Utf8(),
                         " id=", element.GetIdAttribute().Utf8(),
                         " class=", element.GetAttribute("class").Utf8()});
  }
  if (node.IsDocumentNode()) {
    return "document";
  }
  return "";
}

}  // namespace actor
