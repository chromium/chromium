// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/scroll_tool.h"

#include <optional>

#include "base/notimplemented.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace actor {

using ::blink::WebElement;
using ::blink::WebLocalFrame;
using ::blink::WebNode;

ScrollTool::ScrollTool(mojom::ScrollActionPtr action,
                       content::RenderFrame& frame)
    : frame_(frame), action_(std::move(action)) {}

ScrollTool::~ScrollTool() = default;

void ScrollTool::Execute(ToolFinishedCallback callback) {
  // The scroll distance should always be positive.
  if (action_->distance <= 0.0) {
    ACTOR_LOG() << "Invalid scroll distance: " << action_->distance;
    std::move(callback).Run(false);
    return;
  }

  WebLocalFrame* web_frame = frame_->GetWebFrame();
  if (!web_frame || !web_frame->FrameWidget()) {
    ACTOR_LOG() << "WebLocalFrame or FrameWidget is null.";
    std::move(callback).Run(false);
    return;
  }

  WebElement scrolling_element;

  if (!action_->target) {
    scrolling_element = web_frame->GetDocument().ScrollingElement();
  } else {
    if (action_->target->is_coordinate()) {
      NOTIMPLEMENTED() << "Coordinate-based target not yet supported.";
      std::move(callback).Run(false);
      return;
    }

    int32_t dom_node_id = action_->target->get_dom_node_id();
    scrolling_element =
        GetNodeFromId(frame_.get(), dom_node_id).DynamicTo<WebElement>();
  }

  if (scrolling_element.IsNull()) {
    ACTOR_LOG() << "Target element not found.";
    std::move(callback).Run(false);
    return;
  }

  gfx::Vector2dF offset_physical;
  switch (action_->direction) {
    case mojom::ScrollAction::ScrollDirection::kLeft: {
      offset_physical.set_x(-action_->distance);
      break;
    }
    case mojom::ScrollAction::ScrollDirection::kRight: {
      offset_physical.set_x(action_->distance);
      break;
    }
    case mojom::ScrollAction::ScrollDirection::kUp: {
      offset_physical.set_y(-action_->distance);
      break;
    }
    case mojom::ScrollAction::ScrollDirection::kDown: {
      offset_physical.set_y(action_->distance);
      break;
    }
  }

  if ((offset_physical.x() && !scrolling_element.IsUserScrollableX()) ||
      (offset_physical.y() && !scrolling_element.IsUserScrollableY())) {
    ACTOR_LOG() << "Target " << scrolling_element
                << " is not user scrollable for scroll offset "
                << offset_physical.ToString();
    std::move(callback).Run(false);
    return;
  }

  float physical_to_css = 1 / scrolling_element.GetEffectiveZoom();
  gfx::Vector2dF offset_css =
      gfx::ScaleVector2d(offset_physical, physical_to_css, physical_to_css);

  gfx::Vector2dF start_offset_css = scrolling_element.GetScrollOffset();
  scrolling_element.SetScrollOffset(start_offset_css + offset_css);

  bool did_scroll = scrolling_element.GetScrollOffset() != start_offset_css;
  std::move(callback).Run(did_scroll);
}

std::string ScrollTool::DebugString() const {
  return absl::StrFormat("ScrollTool[%s;direction(%s);distance(%f)]",
                         ToDebugString(action_->target),
                         base::ToString(action_->direction), action_->distance);
}

}  // namespace actor
