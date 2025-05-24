// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_TOOL_UTILS_H_
#define CHROME_RENDERER_ACTOR_TOOL_UTILS_H_

#include <cstdint>
#include <optional>
#include <string>

#include "chrome/common/actor.mojom-forward.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

namespace blink {
class WebNode;
class WebFrameWidget;
}  // namespace blink

namespace content {
class RenderFrame;
}  // namespace content

namespace gfx {
class PointF;
}  // namespace gfx

namespace actor {

blink::WebNode GetNodeFromId(const content::RenderFrame& frame,
                             int32_t node_id);

// Returns the center coordinates of the node's bounding box in widget space.
// Returns nullopt if the node is not a visible element or has no bounds.
std::optional<gfx::PointF> InteractionPointFromWebNode(
    const blink::WebNode& node);

// Returns whether the Node is focusable and in focus.
bool IsNodeFocused(const content::RenderFrame& frame,
                   const blink::WebNode& node);

// `point` is relative to the viewport origin.
// Note: this doesn't account for pinch-zoom.
bool IsPointWithinViewport(const gfx::PointF& point,
                           const content::RenderFrame& frame);

std::string ToDebugString(const mojom::ToolTargetPtr& target);

// Create and dispatch the mouse down event and corresponding mouse up, click
// event to the widget.
mojom::ActionResultPtr CreateAndDispatchClick(
    blink::WebMouseEvent::Button button,
    int count,
    const gfx::PointF& click_point,
    blink::WebFrameWidget* widget);

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_TOOL_UTILS_H_
