// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/tool_base.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/to_string.h"
#include "base/types/expected.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/chrome_features.h"
#include "chrome/renderer/actor/journal.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_hit_test_result.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

using base::UmaHistogramEnumeration;
using blink::WebElement;
using blink::WebFrameWidget;
using blink::WebHitTestResult;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebPagePopup;
using blink::WebWidget;

namespace actor {
namespace {

constexpr char kTimeOfUseValidationHistogram[] =
    "Actor.Tools.TimeOfUseValidation";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(TimeOfUseResult)
enum class TimeOfUseResult {
  kValid = 0,
  kWrongNodeAtCoordinate = 1,
  kTargetNodeInteractionPointObscured = 2,
  kTargetNodeMissing = 3,
  kTargetPointOutsideBoundingBox = 4,
  kTargetNodeMissingGeometry = 5,
  kNoValidApcNode = 6,
  kMaxValue = kNoValidApcNode,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:TimeOfUseResult)

WebNode GetNodeFromIdIncludingPopup(const content::RenderFrame& frame,
                                    int32_t node_id) {
  if (base::FeatureList::IsEnabled(
          blink::features::kAIPageContentIncludePopupWindows)) {
    if (WebPagePopup* popup = frame.GetWebView()->GetPagePopup()) {
      const WebNode& found_node = blink::WebNode::FromDomNodeId(node_id);
      if (found_node.GetDocument() == popup->GetDocument()) {
        return found_node;
      }
    }
  }
  return GetNodeFromId(frame, node_id);
}

}  // namespace

WebWidget* ResolvedTarget::GetWidget(const ToolBase& tool) const {
  const WebLocalFrame* web_frame = tool.frame()->GetWebFrame();
  if (!web_frame || !web_frame->FrameWidget()) {
    return nullptr;
  }

  if (!popup_handle.has_value()) {
    return web_frame->FrameWidget();
  }

  // If the frame widget isn't the one that was originally targeted,
  // it must be a popup but we can't directly use it since it may
  // have been closed.
  WebPagePopup* current_popup_widget = web_frame->View()->GetPagePopup();
  if (current_popup_widget &&
      current_popup_widget->GetHandle() == *popup_handle) {
    return current_popup_widget;
  }

  return nullptr;
}

base::TimeDelta ToolBase::ExecutionObservationDelay() const {
  return base::TimeDelta();
}

bool ToolBase::SupportsPaintStability() const {
  return false;
}

ToolBase::ToolBase(content::RenderFrame& frame,
                   TaskId task_id,
                   Journal& journal,
                   mojom::ToolTargetPtr target,
                   mojom::ObservedToolTargetPtr observed_target)
    : frame_(frame),
      task_id_(task_id),
      journal_(journal),
      target_(std::move(target)),
      observed_target_(std::move(observed_target)) {}

ToolBase::~ToolBase() = default;

void ToolBase::Cancel() {}

ToolBase::ResolveResult ToolBase::ResolveTarget(
    const mojom::ToolTarget& target) const {
  ResolvedTarget resolved_target;

  WebPagePopup* popup = frame_->GetWebFrame()->View()->GetPagePopup();
  blink::WebFrameWidget* frame_widget = frame_->GetWebFrame()->FrameWidget();
  CHECK(frame_widget);

  if (target.is_coordinate_dip()) {
    // Check if the coordinate hits a popup widget first. Note, popups can draw
    // outside of the frame so we don't check for the point being within the
    // viewport.
    if (popup && !popup->GetHandle().is_null()) {
      gfx::Vector2d frame_origin_in_screen_dips =
          frame_widget->ViewRect().OffsetFromOrigin();
      gfx::Rect popup_rect_in_frame_dips = popup->ViewRect();
      popup_rect_in_frame_dips.Offset(-frame_origin_in_screen_dips);

      gfx::Point coordinate_dips = target.get_coordinate_dip();
      if (popup_rect_in_frame_dips.Contains(coordinate_dips)) {
        // Convert the point into popup-relative coordinates
        gfx::PointF widget_point = frame_widget->DIPsToBlinkSpace(gfx::PointF(
            coordinate_dips - popup_rect_in_frame_dips.OffsetFromOrigin()));
        return ResolvedTarget{
            .node = popup->HitTestResultAt(widget_point).GetNodeOrPseudoNode(),
            // TODO(bokan) Move DIPsToBlinkSpace onto WebWidget but this
            // shouldn't matter since it's just a scale factor global to the
            // page.
            .widget_point = widget_point,
            .popup_handle = popup->GetHandle()};
      }
    }

    gfx::PointF widget_point = frame_widget->DIPsToBlinkSpace(
        gfx::PointF(target.get_coordinate_dip()));

    if (!IsPointWithinViewport(widget_point, frame_.get())) {
      return base::unexpected(MakeResult(
          mojom::ActionResultCode::kCoordinatesOutOfBounds,
          /*requires_page_stabilization=*/false,
          absl::StrFormat("Point (physical) [%s]", widget_point.ToString())));
    }

    // Perform a hit test to find the node at the coordinates.
    return ResolvedTarget{
        .node =
            frame_widget->HitTestResultAt(widget_point).GetNodeOrPseudoNode(),
        .widget_point = widget_point,
        .popup_handle = std::nullopt};
  }

  CHECK(target.is_dom_node_id());

  WebNode node =
      GetNodeFromIdIncludingPopup(frame_.get(), target.get_dom_node_id());
  if (node.IsNull() || !node.IsConnected()) {
    return base::unexpected(
        MakeResult(mojom::ActionResultCode::kInvalidDomNodeId));
  }

  std::optional<gfx::PointF> node_interaction_point;
  std::optional<WebPagePopup::Handle> popup_handle;
  // This comparison is enough because popups can't contain subframes.
  if (popup && !popup->GetHandle().is_null() &&
      node.GetDocument() == popup->GetDocument()) {
    popup_handle = popup->GetHandle();
    node_interaction_point = InteractionPointFromWebNode(popup, node);
  } else {
    node_interaction_point = InteractionPointFromWebNode(frame_widget, node);
  }

  if (!node_interaction_point.has_value()) {
    return base::unexpected(
        MakeResult(mojom::ActionResultCode::kElementOffscreen,
                   /*requires_page_stabilization=*/false,
                   absl::StrFormat("[Element %s]", base::ToString(node))));
  }

  return ResolvedTarget{.node = node,
                        .widget_point = *node_interaction_point,
                        .popup_handle = popup_handle};
}

ToolBase::ResolveResult ToolBase::ValidateAndResolveTarget() const {
  if (!target_) {
    // TODO(b/450027252): This should return a non-OK error code.
    return base::unexpected(MakeResult(mojom::ActionResultCode::kOk));
  }

  ResolveResult resolved_target = ResolveTarget(*target_);
  if (!resolved_target.has_value()) {
    return base::unexpected(std::move(resolved_target.error()));
  }

  mojom::ActionResultPtr validation =
      ValidateTimeOfUse(resolved_target.value());
  if (!IsOk(*validation)) {
    return base::unexpected(std::move(validation));
  }

  return resolved_target.value();
}

bool ToolBase::EnsureTargetInView() {
  if (!target_) {
    return false;
  }

  // Scrolling a target into view is only supported for node_id targets since
  // TOCTOU checks cannot be applied to the APC captured at the old scroll
  // offset.
  if (target_->is_coordinate_dip()) {
    return false;
  }

  int32_t dom_node_id = target_->get_dom_node_id();
  WebElement node = GetNodeFromIdIncludingPopup(frame_.get(), dom_node_id)
                        .DynamicTo<WebElement>();
  if (node && node.VisibleBoundsInWidget().IsEmpty()) {
    node.ScrollIntoViewIfNeeded();
    return true;
  }

  return false;
}

mojom::ActionResultPtr ToolBase::ValidateTimeOfUse(
    const ResolvedTarget& resolved_target) const {
  const WebNode& target_node = resolved_target.node;

  // For coordinate target, check the observed node matches the live DOM hit
  // test target.
  if (target_->is_coordinate_dip()) {
    if (!observed_target_ || !observed_target_->node_attribute->dom_node_id) {
      journal_->Log(
          task_id_, "TimeOfUseValidation",
          JournalDetailsBuilder().AddError("No valid APC node").Build());
      UmaHistogramEnumeration(kTimeOfUseValidationHistogram,
                              TimeOfUseResult::kNoValidApcNode);
      // TODO(crbug.com/445210509): return error for no apc found.
      return MakeOkResult();
    }

    const WebNode observed_target_node = GetNodeFromIdIncludingPopup(
        *frame_, *observed_target_->node_attribute->dom_node_id);

    if (observed_target_node.IsNull()) {
      journal_->Log(
          task_id_, "TimeOfUseValidation",
          JournalDetailsBuilder()
              .Add("coordinate_dip",
                   base::ToString(target_->get_coordinate_dip()))
              .Add("target_id", target_node.GetDomNodeId())
              .Add("observed_target_id",
                   *observed_target_->node_attribute->dom_node_id)
              .Add("target", NodeToDebugString(target_node))
              .AddError(
                  "Observed target at coordinate is not present in live DOM")
              .Build());
      if (base::FeatureList::IsEnabled(features::kGlicActorToctouValidation)) {
        return MakeResult(
            mojom::ActionResultCode::kObservedTargetElementDestroyed,
            /*requires_page_stabilization=*/false,
            "The observed element at the target location is destroyed");
      }
    }

    // Target node for coordinate target is obtained through blink hit test
    // which includes shadow host elements.
    if (!observed_target_node.ContainsViaFlatTree(&target_node)) {
      journal_->Log(
          task_id_, "TimeOfUseValidation",
          JournalDetailsBuilder()
              .Add("coordinate_dip",
                   base::ToString(target_->get_coordinate_dip()))
              .Add("target_id", target_node.GetDomNodeId())
              .Add("observed_target_id", observed_target_node.GetDomNodeId())
              .Add("target", NodeToDebugString(target_node))
              .Add("observed_target", NodeToDebugString(observed_target_node))
              .AddError("Wrong Node At Location")
              .Build());
      UmaHistogramEnumeration(kTimeOfUseValidationHistogram,
                              TimeOfUseResult::kWrongNodeAtCoordinate);
      if (base::FeatureList::IsEnabled(features::kGlicActorToctouValidation)) {
        return MakeResult(
            mojom::ActionResultCode::kObservedTargetElementChanged,
            /*requires_page_stabilization=*/false,
            "The element at the target location is not the same as "
            "the one observed.");
      } else {
        return MakeOkResult();
      }
    }
  } else {
    CHECK(target_->is_dom_node_id());
    WebWidget* widget = resolved_target.GetWidget(*this);
    CHECK(widget);

    // Check that the interaction point will actually hit
    // on the intended element, i.e. centre point of node is not occluded.
    const WebHitTestResult hit_test_result =
        widget->HitTestResultAt(resolved_target.widget_point);
    const WebElement hit_element = hit_test_result.GetElement();
    // The action target from APC is not as granular as the live DOM hit test.
    // Include shadow host element as the hit test would land on those. Also
    // check if the hit element was pulled in via a Web Components slot.
    if (!target_node.ContainsViaFlatTree(&hit_element)) {
      journal_->Log(task_id_, "TimeOfUseValidation",
                    JournalDetailsBuilder()
                        .Add("target_id", target_node.GetDomNodeId())
                        .Add("hit_node_id", hit_element.GetDomNodeId())
                        .Add("target", NodeToDebugString(target_node))
                        .Add("hit_node", NodeToDebugString(hit_element))
                        .AddError("Node covered by another node")
                        .Build());
      UmaHistogramEnumeration(
          kTimeOfUseValidationHistogram,
          TimeOfUseResult::kTargetNodeInteractionPointObscured);
      if (base::FeatureList::IsEnabled(features::kGlicActorToctouValidation)) {
        return MakeResult(
            mojom::ActionResultCode::kTargetNodeInteractionPointObscured,
            /*requires_page_stabilization=*/false,
            "The element's interaction point is obscured by other elements.");
      } else {
        return MakeOkResult();
      }
    }

    if (!observed_target_ || !observed_target_->node_attribute->dom_node_id) {
      journal_->Log(
          task_id_, "TimeOfUseValidation",
          JournalDetailsBuilder().AddError("No valid APC node").Build());
      UmaHistogramEnumeration(kTimeOfUseValidationHistogram,
                              TimeOfUseResult::kNoValidApcNode);
      // TODO(crbug.com/445210509): return error for no apc found.
      return MakeOkResult();
    }

    if (!observed_target_->node_attribute->geometry) {
      journal_->Log(
          task_id_, "TimeOfUseValidation",
          JournalDetailsBuilder()
              .Add("obs_node_id",
                   *observed_target_->node_attribute->dom_node_id)
              .Add("point", gfx::ToFlooredPoint(resolved_target.widget_point))
              .AddError("No geometry for node")
              .Build());
      // TODO(crbug.com/418280472): return error after retry for failed task
      // is landed.
      UmaHistogramEnumeration(kTimeOfUseValidationHistogram,
                              TimeOfUseResult::kTargetNodeMissingGeometry);
      return MakeOkResult();
    }

    // Check that the interaction point is inside the observed target bounding
    // box from last APC.
    const gfx::Rect observed_bounds =
        observed_target_->node_attribute->geometry->outer_bounding_box;
    if (!observed_bounds.Contains(
            gfx::ToFlooredPoint(resolved_target.widget_point))) {
      journal_->Log(task_id_, "TimeOfUseValidation",
                    JournalDetailsBuilder()
                        .Add("resolved_target_point",
                             gfx::ToFlooredPoint(resolved_target.widget_point))
                        .Add("bounding_box", observed_bounds)
                        .AddError("Point not in box")
                        .Build());
      // TODO(crbug.com/418280472): return error after retry for failed task
      // is landed.
      UmaHistogramEnumeration(kTimeOfUseValidationHistogram,
                              TimeOfUseResult::kTargetPointOutsideBoundingBox);
      return MakeOkResult();
    }
  }

  UmaHistogramEnumeration(kTimeOfUseValidationHistogram,
                          TimeOfUseResult::kValid);
  return MakeOkResult();
}

}  // namespace actor
