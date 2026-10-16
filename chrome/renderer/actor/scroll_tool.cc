// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/scroll_tool.h"

#include <optional>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "base/strings/to_string.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_constants.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "components/actor/core/actor_logging.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/mojom/input/scroll_direction.mojom-shared.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "ui/display/screen_info.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/types/scroll_types.h"
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
  CHECK(validated_scroller_and_distance_.has_value())
      << "Execute tool was called before validation";
  WebElement scrolling_element =
      validated_scroller_and_distance_.value().scroller;

  if (action_->amount->is_granularity()) {
    ExecuteByGranularity(scrolling_element, std::move(callback));
    return;
  }

  gfx::Vector2dF offset_physical =
      validated_scroller_and_distance_.value().scroll_by_offset.value();

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

void ScrollTool::ExecuteByGranularity(WebElement& scrolling_element,
                                      ToolFinishedCallback callback) {
  gfx::Vector2dF start_offset_css = scrolling_element.GetScrollOffset();
  // This performs a user scroll (like PageUp/PageDown/Home/End), which may
  // animate. The tool finishes once the scroll has completed.
  scrolling_element.UserScrollByGranularity(
      action_->direction, action_->amount->get_granularity(),
      base::BindOnce(&ScrollTool::OnScrollByGranularityFinished,
                     weak_ptr_factory_.GetWeakPtr(), scrolling_element,
                     start_offset_css, std::move(callback)));
}

void ScrollTool::OnScrollByGranularityFinished(
    blink::WebElement scrolling_element,
    gfx::Vector2dF start_offset_css,
    ToolFinishedCallback callback,
    WebElement::ScrollByGranularityResult result) {
  journal_->Log(task_id_, "ScrollTool::Execute",
                JournalDetailsBuilder()
                    .Add("element", scrolling_element)
                    .Add("granularity",
                         static_cast<int>(action_->amount->get_granularity()))
                    .Add("start_offset", start_offset_css)
                    .Add("end_offset", scrolling_element.GetScrollOffset())
                    .Add("result", static_cast<int>(result))
                    .Build());

  switch (result) {
    case WebElement::ScrollByGranularityResult::kScrolled:
      std::move(callback).Run(MakeOkResult());
      return;
    case WebElement::ScrollByGranularityResult::kNoRoomToScroll:
      std::move(callback).Run(
          MakeResult(mojom::ActionResultCode::kScrollOffsetDidNotChange));
      return;
    case WebElement::ScrollByGranularityResult::kNotScrollable:
      std::move(callback).Run(
          MakeResult(mojom::ActionResultCode::kScrollTargetNotUserScrollable));
      return;
  }
}

std::string ScrollTool::DebugString() const {
  const std::string amount =
      action_->amount->is_distance()
          ? absl::StrFormat("distance(%f)", action_->amount->get_distance())
          : absl::StrFormat(
                "granularity(%d)",
                static_cast<int>(action_->amount->get_granularity()));
  return absl::StrFormat("ScrollTool[%s;direction(%d);%s]",
                         ToDebugString(target_),
                         static_cast<int>(action_->direction), amount);
}

ValidationResult ScrollTool::Validate() {
  WebLocalFrame* web_frame = frame_->GetWebFrame();
  CHECK(web_frame);
  CHECK(web_frame->FrameWidget());

  // The scroll distance should always be positive.
  if (action_->amount->is_distance() &&
      action_->amount->get_distance() <= 0.0) {
    return ValidationResult(
        MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                   /*requires_page_stabilization=*/false, "Negative Distance"));
  }

  // Only page and document granularity scrolls are supported.
  if (action_->amount->is_granularity() &&
      action_->amount->get_granularity() !=
          ui::ScrollGranularity::kScrollByPage &&
      action_->amount->get_granularity() !=
          ui::ScrollGranularity::kScrollByDocument) {
    return ValidationResult(MakeResult(
        mojom::ActionResultCode::kArgumentsInvalid,
        /*requires_page_stabilization=*/false, "Unsupported Granularity"));
  }

  // A unit vector in the scroll direction. Finding the scroller only depends
  // on the direction, not on how far the scroll goes.
  gfx::Vector2dF direction;
  switch (action_->direction) {
    case blink::mojom::ScrollDirectionPhysical::kLeft: {
      direction.set_x(-1);
      break;
    }
    case blink::mojom::ScrollDirectionPhysical::kRight: {
      direction.set_x(1);
      break;
    }
    case blink::mojom::ScrollDirectionPhysical::kUp: {
      direction.set_y(-1);
      break;
    }
    case blink::mojom::ScrollDirectionPhysical::kDown: {
      direction.set_y(1);
      break;
    }
  }

  WebElement scrolling_element;
  if (target_->is_coordinate_dip()) {
    // We don't do a TimeOfUse check for coordinate based targets,
    // because the intended target is determined by bubbling up to
    // ancestors and only the `observed_target_` node is provided from
    // the browser.
    ToolBase::ResolveResult resolved_target = ResolveTarget(*target_);
    if (!resolved_target.has_value()) {
      return ValidationResult(std::move(resolved_target.error()));
    }

    blink::WebNode& node = resolved_target->node;
    WebElement root_scroller = web_frame->GetDocument().ScrollingElement();

    while (!node.IsNull()) {
      // If we reach the root scroller, we check if it is a scrollable target.
      if (node.IsDocumentNode() && !root_scroller.IsNull() &&
          IsTargetUserScrollable(root_scroller, direction)) {
        node = root_scroller;
        break;
      }

      if (IsTargetUserScrollable(node, direction)) {
        break;
      }

      node = node.ParentOrShadowHostNode();
    }

    if (node.IsNull()) {
      return ValidationResult(
          MakeResult(mojom::ActionResultCode::kScrollTargetNotUserScrollable,
                     /*requires_page_stabilization=*/false,
                     "No scrollable ancestor found by coordinate"));
    }

    scrolling_element = node.To<WebElement>();

    // At this point, `scrolling_element` is guaranteed to be a scrollable
    // element. Otherwise, we would have returned the error above.
    CHECK(IsTargetUserScrollable(scrolling_element, direction));
  } else {
    int32_t dom_node_id = target_->get_dom_node_id();
    if (dom_node_id == kRootElementDomNodeId) {
      scrolling_element = web_frame->GetDocument().ScrollingElement();
      if (scrolling_element.IsNull()) {
        return ValidationResult(
            MakeResult(mojom::ActionResultCode::kScrollNoScrollingElement));
      }
    } else {
      ToolBase::ResolveResult resolved_target = ValidateAndResolveTarget();
      if (!resolved_target.has_value()) {
        return ValidationResult(std::move(resolved_target.error()));
      }
      scrolling_element = resolved_target->node.DynamicTo<WebElement>();
    }

    if (!IsTargetUserScrollable(scrolling_element, direction)) {
      return ValidationResult(
          MakeResult(mojom::ActionResultCode::kScrollTargetNotUserScrollable,
                     /*requires_page_stabilization=*/false,
                     absl::StrFormat("ScrollingElement [%s]",
                                     base::ToString(scrolling_element))));
    }
  }

  // Page/document scroll amounts are determined by Blink when the scroll is
  // performed; only pixel scrolls have an offset to compute here.
  std::optional<gfx::Vector2dF> offset_physical;
  if (action_->amount->is_distance()) {
    // No matter what frame the scroll element is on, it must be on the same
    // physical screen, so the device scale factor will be the same.
    const float dsf =
        web_frame->FrameWidget()->GetScreenInfo().device_scale_factor;
    offset_physical =
        gfx::ScaleVector2d(direction, action_->amount->get_distance() * dsf);
  }
  validated_scroller_and_distance_.emplace(
      ScrollerAndDistance{scrolling_element, offset_physical});
  return ValidationResult(MakeOkResult());
}

}  // namespace actor
