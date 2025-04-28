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
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point_f.h"

namespace actor {

ScrollTool::ScrollTool(mojom::ScrollActionPtr action,
                       content::RenderFrame& frame)
    : frame_(frame), action_(std::move(action)) {}

ScrollTool::~ScrollTool() = default;

void ScrollTool::Execute(ToolFinishedCallback callback) {
  blink::WebLocalFrame* web_frame = frame_->GetWebFrame();
  if (!web_frame || !web_frame->FrameWidget()) {
    ACTOR_LOG() << "RenderFrame or FrameWidget is invalid.";
    std::move(callback).Run(false);
    return;
  }

  if (action_->target) {
    if (action_->target->is_coordinate()) {
      NOTIMPLEMENTED() << "Coordinate-based target not yet supported.";
      std::move(callback).Run(false);
      return;
    }

    int32_t dom_node_id = action_->target->get_dom_node_id();
    blink::WebNode node = GetNodeFromId(frame_.get(), dom_node_id);
    if (node.IsNull()) {
      ACTOR_LOG() << "Cannot find dom node with id " << dom_node_id;
      std::move(callback).Run(false);
      return;
    }

    // TODO(crbug.com/402083666): add support for scrolling subscrollers later.
    NOTIMPLEMENTED();
    std::move(callback).Run(false);
    return;
  }

  // The scroll distance should always be positive.
  if (action_->distance <= 0.0) {
    std::move(callback).Run(false);
    return;
  }

  // Scrolling the page's viewport.
  float scroll_offset_x = 0;
  float scroll_offset_y = 0;
  switch (action_->direction) {
    case mojom::ScrollAction::ScrollDirection::kLeft: {
      scroll_offset_x = -action_->distance;
      break;
    }
    case mojom::ScrollAction::ScrollDirection::kRight: {
      scroll_offset_x = action_->distance;
      break;
    }
    case mojom::ScrollAction::ScrollDirection::kUp: {
      scroll_offset_y = -action_->distance;
      break;
    }
    case mojom::ScrollAction::ScrollDirection::kDown: {
      scroll_offset_y = action_->distance;
      break;
    }
  }

  // Calculate the new scroll offset from the current offset.
  gfx::PointF offset = web_frame->GetScrollOffset();
  bool did_scroll = web_frame->SetScrollOffset(
      gfx::PointF(offset.x() + scroll_offset_x, offset.y() + scroll_offset_y));

  std::move(callback).Run(did_scroll);
}

std::string ScrollTool::DebugString() const {
  return absl::StrFormat("ScrollTool[%s;direction(%s);distance(%f)]",
                         ToDebugString(action_->target),
                         base::ToString(action_->direction), action_->distance);
}

}  // namespace actor
