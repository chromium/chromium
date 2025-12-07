// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/scroll_tool.h"

#include <optional>

#include "base/feature_list.h"
#include "base/notimplemented.h"
#include "base/strings/to_string.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_constants.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/chrome_features.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "ui/display/screen_info.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace actor {

using ::blink::WebElement;
using ::blink::WebLocalFrame;
using ::blink::WebNode;

bool IsTargetUserScrollable(const blink::WebNode& node,
                            const gfx::Vector2dF& offset) {
  if (!node.IsElementNode()) {
    return false;
  }

  WebElement element = node.To<WebElement>();
  return (!offset.x() || element.IsUserScrollableX()) &&
         (!offset.y() || element.IsUserScrollableY());
}

ScrollTool::ScrollTool(content::RenderFrame& frame,
                       TaskId task_id,
                       Journal& journal,
                       mojom::ScrollActionPtr action,
                       mojom::ToolTargetPtr target,
                       mojom::ObservedToolTargetPtr observed_target)
    : ToolBase(frame,
               task_id,
               journal,
               std::move(target),
               std::move(observed_target)),
      action_(std::move(action)) {}

ScrollTool::~ScrollTool() = default;

void ScrollTool::Execute(ToolFinishedCallback callback) {
  ValidatedResult validated_result = Validate();
  if (!validated_result.has_value()) {
    std::move(callback).Run(std::move(validated_result.error()));
    return;
  }

  WebElement scrolling_element = validated_result->scroller;
  gfx::Vector2dF offset_physical = validated_result->scroll_by_offset;

  float physical_to_css = 1 / scrolling_element.GetEffectiveZoom();
  gfx::Vector2dF offset_css =
      gfx::ScaleVector2d(offset_physical, physical_to_css, physical_to_css);

  gfx::Vector2dF start_offset_css = scrolling_element.GetScrollOffset();
  bool did_scroll =
      scrolling_element.SetScrollOffset(start_offset_css + offset_css);

  journal_->Log(task_id_, "ScrollTool::Execute",
                JournalDetailsBuilder()
                    .Add("element", scrolling_element)
                    .Add("start_offset", start_offset_css)
                    .Add("offset", offset_css)
                    .Build());

  std::move(callback).Run(
      did_scroll
          ? MakeOkResult()
          : MakeResult(mojom::ActionResultCode::kScrollOffsetDidNotChange));
}

std::string ScrollTool::DebugString() const {
  return absl::StrFormat("ScrollTool[%s;direction(%s);distance(%f)]",
                         ToDebugString(target_),
                         base::ToString(action_->direction), action_->distance);
}

ScrollTool::ValidatedResult ScrollTool::Validate() const {
  WebLocalFrame* web_frame = frame_->GetWebFrame();
  CHECK(web_frame);
  CHECK(web_frame->FrameWidget());

  // The scroll distance should always be positive.
  if (action_->distance <= 0.0) {
    return base::unexpected(
        MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                   /*requires_page_stabilization=*/false, "Negative Distance"));
  }

  gfx::Vector2dF offset_dips;
  switch (action_->direction) {
    case mojom::ScrollAction::ScrollDirection::kLeft: {
      offset_dips.set_x(-action_->distance);
      break;
    }
    case mojom::ScrollAction::ScrollDirection::kRight: {
      offset_dips.set_x(action_->distance);
      break;
    }
    case mojom::ScrollAction::ScrollDirection::kUp: {
      offset_dips.set_y(-action_->distance);
      break;
    }
    case mojom::ScrollAction::ScrollDirection::kDown: {
      offset_dips.set_y(action_->distance);
      break;
    }
  }

  // No matter what frame the scroll element is on, it must be on the same
  // physical screen, so the device scale factor will be the same.
  const float dsf =
      web_frame->FrameWidget()->GetScreenInfo().device_scale_factor;
  gfx::Vector2dF offset_physical = gfx::ScaleVector2d(offset_dips, dsf);

  WebElement scrolling_element;
  if (target_->is_coordinate_dip()) {
    // We don't do a TimeOfUse check for coordinate based targets,
    // because the intended target is determined by bubbling up to
    // ancestors and only the `observed_target_` node is provided from
    // the browser.
    ToolBase::ResolveResult resolved_target = ResolveTarget(*target_);
    if (!resolved_target.has_value()) {
      return base::unexpected(std::move(resolved_target.error()));
    }

    blink::WebNode& node = resolved_target->node;
    WebElement root_scroller = web_frame->GetDocument().ScrollingElement();

    while (!node.IsNull()) {
      // If we reach the root scroller, we check if it is a scrollable target.
      if (node.IsDocumentNode() && !root_scroller.IsNull() &&
          IsTargetUserScrollable(root_scroller, offset_physical)) {
        node = root_scroller;
        break;
      }

      if (IsTargetUserScrollable(node, offset_physical)) {
        break;
      }

      node = node.ParentOrShadowHostNode();
    }

    if (node.IsNull()) {
      return base::unexpected(
          MakeResult(mojom::ActionResultCode::kScrollTargetNotUserScrollable,
                     /*requires_page_stabilization=*/false,
                     "No scrollable ancestor found by coordinate"));
    }

    scrolling_element = node.To<WebElement>();

    // At this point, `scrolling_element` is guaranteed to be a scrollable
    // element. Otherwise, we would have returned the error above.
    CHECK(IsTargetUserScrollable(scrolling_element, offset_physical));
  } else {
    int32_t dom_node_id = target_->get_dom_node_id();
    if (dom_node_id == kRootElementDomNodeId) {
      scrolling_element = web_frame->GetDocument().ScrollingElement();
      if (scrolling_element.IsNull()) {
        return base::unexpected(
            MakeResult(mojom::ActionResultCode::kScrollNoScrollingElement));
      }
    } else {
      ToolBase::ResolveResult resolved_target = ValidateAndResolveTarget();
      if (!resolved_target.has_value()) {
        return base::unexpected(std::move(resolved_target.error()));
      }
      scrolling_element = resolved_target->node.DynamicTo<WebElement>();
    }

    if (!IsTargetUserScrollable(scrolling_element, offset_physical)) {
      return base::unexpected(
          MakeResult(mojom::ActionResultCode::kScrollTargetNotUserScrollable,
                     /*requires_page_stabilization=*/false,
                     absl::StrFormat("ScrollingElement [%s]",
                                     base::ToString(scrolling_element))));
    }
  }

  return ScrollerAndDistance{scrolling_element, offset_physical};
}

}  // namespace actor
