// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/tool_utils.h"

#include <sstream>

#include "chrome/common/actor.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

namespace actor {

std::optional<gfx::PointF> InteractionPointFromWebNode(
    const blink::WebNode& node) {
  blink::WebElement element = node.DynamicTo<blink::WebElement>();
  if (element.IsNull()) {
    return std::nullopt;
  }

  gfx::Rect rect = element.BoundsInWidget();
  if (rect.IsEmpty()) {
    return std::nullopt;
  }

  // TODO(crbug.com/389739308): This should clip to the viewport so the center
  // point stays within the viewport..

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

}  // namespace actor
