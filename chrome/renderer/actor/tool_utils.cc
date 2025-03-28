// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/tool_utils.h"

#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

namespace actor {
std::optional<gfx::PointF> InteractionPointFromWebNode(
    const blink::WebNode& node) {
  // Find and validate the bounding box.
  blink::WebElement web_element = node.To<blink::WebElement>();
  gfx::Rect rect = web_element.BoundsInWidget();
  // Validate element is visible.
  if (rect.width() == 0 || rect.height() == 0) {
    return std::nullopt;
  }
  return {{rect.x() + rect.width() / 2.0f, rect.y() + rect.height() / 2.0f}};
}

blink::WebNode GetNodeFromId(const content::RenderFrame& frame,
                             int32_t node_id) {
  const blink::WebLocalFrame* web_frame = frame.GetWebFrame();
  if (!web_frame) {
    return blink::WebNode();
  }

  blink::WebNode node = blink::WebNode::FromDomNodeId(node_id);
  // Make sure the node we're getting belongs to the document inside this frame.
  if (node.IsNull() || node.GetDocument() != web_frame->GetDocument()) {
    return blink::WebNode();
  }
  return node;
}

}  // namespace actor
