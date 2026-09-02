// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/tool_base.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/to_string.h"
#include "base/types/expected.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/renderer/actor/journal.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_hit_test_result.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

using base::UmaHistogramEnumeration;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebFrameWidget;
using blink::WebHitTestResult;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebPagePopup;
using blink::WebWidget;

namespace actor {
namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(TimeOfUseResult)
enum class TimeOfUseResult {
  // The target passed the final live validation before dispatch.
  kValid = 0,
  // A normal coordinate click would hit a different node than the target.
  kWrongNodeAtCoordinate = 1,
  // The target point is covered in a path that requires unobscured input.
  kTargetNodeInteractionPointObscured = 2,
  // The previously resolved target node is no longer available.
  kTargetNodeMissing = 3,
  // The live target point no longer matches the observed APC bounds.
  kTargetPointOutsideBoundingBox = 4,
  // The observed APC node exists but has no geometry to compare against.
  kTargetNodeMissingGeometry = 5,
  // The latest APC snapshot does not contain a valid node for the target.
  kNoValidApcNode = 6,
  kMaxValue = kNoValidApcNode,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:TimeOfUseResult)

WebElement GetElementOrNearestElementAncestor(WebNode node) {
  // Hit testing can return text. Validation APIs live on elements. If the walk
  // reaches the document instead, this returns null and validation is skipped.
  while (!node.IsNull() && !node.IsElementNode()) {
    node = node.ParentNode();
  }
  return node.DynamicTo<WebElement>();
}

bool HasEmptyClientRects(WebElement& element) {
  // ClientRectsInWidget() reports this element's own CSS client rects. A
  // structural zero-area wrapper can have none even when its descendants are
  // visible, so descendants are checked separately below.
  for (const gfx::Rect& rect : element.ClientRectsInWidget()) {
    if (!rect.IsEmpty()) {
      return false;
    }
  }
  return true;
}

std::optional<gfx::PointF> FindClickPointOnDescendantOfZeroAreaTarget(
    WebWidget& target_widget,
    const WebNode& target_node,
    const gfx::Rect& allowed_click_bounds) {
  WebElement target_element = target_node.DynamicTo<WebElement>();
  if (target_element.IsNull()) {
    return std::nullopt;
  }

  // This exception is only for structural wrappers with no client area of
  // their own. A clipped or offscreen positive-area element must continue
  // through the normal interaction-point path.
  if (!HasEmptyClientRects(target_element)) {
    return std::nullopt;
  }

  // Find a current visible descendant point inside the target's observed
  // visible bounds whose live hit-test result remains in the target's flat
  // tree.
  const gfx::Rect viewport(target_widget.VisibleViewportSize());
  WebNode descendant = target_node.NextInFlatTree(target_node);
  while (!descendant.IsNull()) {
    WebElement element = descendant.DynamicTo<WebElement>();
    if (element.IsNull()) {
      descendant = descendant.NextInFlatTree(target_node);
      continue;
    }
    bool has_visible_geometry = false;
    for (const gfx::Rect& rect : element.ClientRectsInWidget()) {
      gfx::Rect visible_rect = rect;
      visible_rect.Intersect(viewport);
      if (visible_rect.IsEmpty()) {
        continue;
      }
      has_visible_geometry = true;
      const gfx::Point candidate = visible_rect.CenterPoint();
      // The click must remain within the target bounds from the last APC
      // snapshot.
      if (!allowed_click_bounds.Contains(candidate)) {
        continue;
      }
      WebNode live_hit = target_widget.HitTestResultAt(gfx::PointF(candidate))
                             .GetNodeOrPseudoNode();
      if (!live_hit.IsNull() && target_node.ContainsViaFlatTree(&live_hit)) {
        return gfx::PointF(candidate);
      }
    }
    // Once an element supplies visible geometry, do not search deeper in that
    // branch. This keeps the search on the nearest rendered descendants.
    descendant = has_visible_geometry
                     ? descendant.NextSkippingChildrenInFlatTree(target_node)
                     : descendant.NextInFlatTree(target_node);
  }
  return std::nullopt;
}

enum class WebElementAuthorBarrierReason {
  kNone = 0,
  kInert,
  kScrollLocked,
};

const char* ToString(WebElementAuthorBarrierReason reason) {
  switch (reason) {
    case WebElementAuthorBarrierReason::kNone:
      return "None";
    case WebElementAuthorBarrierReason::kInert:
      return "Inert";
    case WebElementAuthorBarrierReason::kScrollLocked:
      return "ScrollLocked";
  }
  return "Unknown";
}

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

bool ComputedValueEquals(WebElement element,
                         std::string_view property_name,
                         std::string_view expected_value) {
  if (element.IsNull()) {
    return false;
  }

  return element.GetComputedValue(blink::WebString::FromUtf8(property_name))
      .Equals(expected_value);
}

bool IsFixedOrAbsolutePositioned(WebElement element) {
  return ComputedValueEquals(element, "position", "fixed") ||
         ComputedValueEquals(element, "position", "absolute");
}

bool IsScrollLocked(blink::WebDocument document) {
  if (document.IsNull()) {
    return false;
  }

  return ComputedValueEquals(document.DocumentElement(), "overflow-y",
                             "hidden") &&
         ComputedValueEquals(document.Body(), "overflow-y", "hidden");
}

bool HasFixedOrAbsoluteAncestor(WebNode node) {
  // Walk up the flat tree so slotted overlay children see shadow-root panels.
  for (WebNode current = node; !current.IsNull();
       current = current.ParentInFlatTree()) {
    WebElement element = current.DynamicTo<WebElement>();
    if (!element.IsNull() && IsFixedOrAbsolutePositioned(element)) {
      return true;
    }
  }
  return false;
}

WebElementAuthorBarrierReason GetWebElementAuthorBarrierReason(
    WebElement element) {
  for (WebNode current = element; !current.IsNull();
       current = current.ParentInFlatTree()) {
    WebElement ancestor = current.DynamicTo<WebElement>();
    if (ancestor.IsNull()) {
      continue;
    }

    if (ancestor.HasAttribute("inert")) {
      return WebElementAuthorBarrierReason::kInert;
    }
  }

  // Do not check aria-modal here. Correct handling needs to know whether the
  // target is outside the active aria-modal subtree, which requires a
  // document-wide scan. That validation belongs in APC/higher layers; see the
  // prototype at crrev.com/c/8007486.

  // Direct activation uses accessibility-style clicks, so ARIA can block it.
  if (element.InteractionDisallowedReason(/*check_aria=*/true).has_value()) {
    return WebElementAuthorBarrierReason::kInert;
  }

  // Scroll lock usually means an overlay owns interaction. Allow direct
  // activation only if the target is inside such a foreground container.
  if (IsScrollLocked(element.GetDocument()) &&
      !HasFixedOrAbsoluteAncestor(element.ParentInFlatTree())) {
    return WebElementAuthorBarrierReason::kScrollLocked;
  }

  return WebElementAuthorBarrierReason::kNone;
}

WebElement GetHitElementInTargetDocument(WebElement hit_element,
                                         const WebDocument& target_document) {
  // A remote iframe hit already points to its element in the target document.
  // A same-process hit points inside the iframe, so move up once to that
  // element. Nested iframes are not supported here.
  if (hit_element.GetDocument() != target_document) {
    WebLocalFrame* frame = hit_element.GetDocument().GetFrame();
    if (!frame) {
      return WebElement();
    }

    hit_element = frame->FrameOwnerElement();
    if (hit_element.IsNull() || hit_element.GetDocument() != target_document) {
      return WebElement();
    }
  }

  return hit_element;
}

WebElement GetFixedOrAbsolutePanel(WebElement hit_element) {
  for (WebNode current = hit_element; !current.IsNull();
       current = current.ParentInFlatTree()) {
    WebElement element = current.DynamicTo<WebElement>();
    if (element.IsNull()) {
      continue;
    }

    if (!IsFixedOrAbsolutePositioned(element)) {
      continue;
    }

    return element;
  }

  return WebElement();
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

  if (!node_interaction_point &&
      base::FeatureList::IsEnabled(
          features::kGlicActorDomIdClicksOnZeroAreaTargets) &&
      observed_target_ &&
      observed_target_->node_attribute->dom_node_id ==
          target.get_dom_node_id() &&
      observed_target_->node_attribute->geometry) {
    // Restrict fallback clicks to bounds observed for this target in the last
    // APC snapshot.
    const gfx::Rect& allowed_click_bounds =
        observed_target_->node_attribute->geometry->visible_bounding_box;
    WebWidget* target_widget = popup_handle
                                   ? static_cast<WebWidget*>(popup)
                                   : static_cast<WebWidget*>(frame_widget);
    node_interaction_point = FindClickPointOnDescendantOfZeroAreaTarget(
        *target_widget, node, allowed_click_bounds);
  }

  if (!node_interaction_point) {
    return base::unexpected(
        MakeResult(mojom::ActionResultCode::kElementOffscreen,
                   /*requires_page_stabilization=*/false,
                   absl::StrFormat("[Element %s]", base::ToString(node))));
  }

  return ResolvedTarget{.node = node,
                        .widget_point = *node_interaction_point,
                        .popup_handle = popup_handle};
}

ToolBase::ResolveResult ToolBase::ValidateAndResolveTarget(
    TargetOcclusionMode occlusion_mode) const {
  if (!target_) {
    // TODO(b/450027252): This should return a non-OK error code.
    return base::unexpected(MakeResult(mojom::ActionResultCode::kOk));
  }

  const bool allows_occluded_direct_activation =
      occlusion_mode == TargetOcclusionMode::kAllowOccludedForDirectActivation;

  if (allows_occluded_direct_activation && target_->is_coordinate_dip()) {
    return base::unexpected(MakeResult(
        mojom::ActionResultCode::kArgumentsInvalid,
        /*requires_page_stabilization=*/false,
        "Occluded target clicks require a DOM node target, not coordinates."));
  }

  ResolveResult resolved_target = ResolveTarget(*target_);
  if (!resolved_target.has_value()) {
    return base::unexpected(std::move(resolved_target.error()));
  }

  if (allows_occluded_direct_activation) {
    const WebLocalFrame* target_frame =
        resolved_target->node.GetDocument().GetFrame();
    if (!target_frame || !target_frame->IsOutermostMainFrame()) {
      // Same-process iframe targets can resolve through the local root before
      // hit-test validation runs. Check the resolved target document here so
      // all subframe targets fail with the direct-activation argument error.
      return base::unexpected(MakeResult(
          mojom::ActionResultCode::kArgumentsInvalid,
          /*requires_page_stabilization=*/false,
          "Direct activation for occluded targets is only supported in the "
          "main frame."));
    }
  }

  mojom::ActionResultPtr validation =
      ValidateTimeOfUse(resolved_target.value(), occlusion_mode);
  if (!IsOk(*validation)) {
    return base::unexpected(std::move(validation));
  }

  return resolved_target.value();
}

std::optional<blink::WebElementInteractionDisallowedReason>
ToolBase::GetInteractionDisallowedReason(
    const ResolvedTarget& resolved_target,
    bool check_aria,
    bool reject_non_disabled_reasons) const {
  WebElement validation_element =
      GetTargetElementForValidation(resolved_target);
  if (validation_element.IsNull()) {
    return std::nullopt;
  }

  if (IsDisabledFormControlTarget(validation_element)) {
    return blink::WebElementInteractionDisallowedReason::kDisabled;
  }

  if (!reject_non_disabled_reasons) {
    return std::nullopt;
  }

  return GetInteractionDisallowedReason(validation_element, check_aria);
}

bool ToolBase::IsDisabledFormControlTarget(
    const WebElement& validation_element) const {
  if (validation_element.IsNull()) {
    return false;
  }

  WebFormControlElement form_control =
      validation_element.DynamicTo<WebFormControlElement>();
  return !form_control.IsNull() && !form_control.IsEnabled();
}

std::optional<blink::WebElementInteractionDisallowedReason>
ToolBase::GetInteractionDisallowedReason(const WebElement& validation_element,
                                         bool check_aria) const {
  if (validation_element.IsNull()) {
    return std::nullopt;
  }

  return validation_element.InteractionDisallowedReason(check_aria);
}

WebElement ToolBase::GetTargetElementForValidation(
    const ResolvedTarget& resolved_target) const {
  if (target_->is_coordinate_dip() && observed_target_ &&
      observed_target_->node_attribute->dom_node_id) {
    WebNode observed_node = GetNodeFromIdIncludingPopup(
        frame_.get(), *observed_target_->node_attribute->dom_node_id);

    // Click and type coordinate targets both arrive here after hit testing.
    // When TOCTOU matched the coordinate to an observed APC node, validate the
    // observed element instead of an internal hit-test descendant.
    // `ValidateTimeOfUse()` already proves this containment for normal
    // coordinate targets. Recheck it here so this helper remains safe if
    // callers use it before or after validation changes.
    if (!observed_node.IsNull() &&
        observed_node.ContainsViaFlatTree(&resolved_target.node)) {
      return GetElementOrNearestElementAncestor(observed_node);
    }
  }

  WebElement resolved_element =
      GetElementOrNearestElementAncestor(resolved_target.node);
  if (target_->is_coordinate_dip() && !resolved_element.IsNull() &&
      resolved_target.node.IsInUserAgentShadowRoot()) {
    WebElement shadow_host = resolved_element.OwnerShadowHost();
    if (!shadow_host.IsNull()) {
      return shadow_host;
    }
  }

  return resolved_element;
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
  if (!node) {
    return false;
  }

  // Bounds queries force lifecycle/layout updates in Blink. Guard against the
  // possibility of layout side effects detaching the owning frame and
  // destroying this tool while executing on the stack.
  base::WeakPtr<ToolBase> weak_this = weak_ptr_factory_.GetWeakPtr();
  if (!node.VisibleBoundsInWidget().IsEmpty()) {
    return false;
  }
  if (!weak_this) {
    return true;
  }

  gfx::Rect bounds_before = node.BoundsInWidget();
  if (!weak_this) {
    return true;
  }

  // Revealing auto-expandable ancestors synchronously dispatches DOM events
  // (`beforematch`) that might run script detaching the owning frame and
  // destroying this tool.
  node.RevealAutoExpandableAncestors();
  if (!weak_this) {
    return true;
  }
  node.ScrollIntoViewIfNeeded();
  if (!weak_this) {
    return true;
  }

  blink::WebLocalFrame* local_frame = node.GetDocument().GetFrame();
  gfx::Rect viewport_bounds =
      local_frame ? local_frame->VisibleContentRect() : gfx::Rect();

  journal_->Log(task_id_, "ScrollIntoView",
                JournalDetailsBuilder()
                    .Add("target", node)
                    .Add("CurBounds", bounds_before)
                    .Add("NewBounds", node.BoundsInWidget())
                    .Add("NewBoundsVisible", node.VisibleBoundsInWidget())
                    .Add("ViewportBounds", viewport_bounds)
                    .Build());

  return true;
}

mojom::ActionResultPtr ToolBase::ValidateTimeOfUse(
    const ResolvedTarget& resolved_target,
    TargetOcclusionMode occlusion_mode) const {
  static constexpr char kTimeOfUseValidationHistogram[] =
      "Actor.Tools.TimeOfUseValidation";
  static constexpr char kTimeOfUseReValidationHistogram[] =
      "Actor.Tools.TimeOfUseReValidation";
  static constexpr char kTimeOfUseValidationJournalName[] =
      "TimeOfUseValidation";
  static constexpr char kTimeOfUseReValidationJournalName[] =
      "TimeOfUseReValidation";

  const WebNode& target_node = resolved_target.node;

  const char* histogram_name = is_revalidation_
                                   ? kTimeOfUseReValidationHistogram
                                   : kTimeOfUseValidationHistogram;
  const char* journal_name = is_revalidation_
                                 ? kTimeOfUseReValidationJournalName
                                 : kTimeOfUseValidationJournalName;

  // For coordinate target, check the observed node matches the live DOM hit
  // test target.
  if (target_->is_coordinate_dip()) {
    if (!observed_target_ || !observed_target_->node_attribute->dom_node_id) {
      journal_->Log(
          task_id_, journal_name,
          JournalDetailsBuilder().AddError("No valid APC node").Build());
      UmaHistogramEnumeration(histogram_name, TimeOfUseResult::kNoValidApcNode);
      if (base::FeatureList::IsEnabled(features::kGlicActorToctouValidation)) {
        return MakeResult(
            mojom::ActionResultCode::kFrameLocationChangedSinceObservation,
            /*requires_page_stabilization=*/false,
            "No prior observation available for TOCTOU validation");
      }
      return MakeOkResult();
    }

    const WebNode observed_target_node = GetNodeFromIdIncludingPopup(
        *frame_, *observed_target_->node_attribute->dom_node_id);

    if (observed_target_node.IsNull()) {
      journal_->Log(
          task_id_, journal_name,
          JournalDetailsBuilder()
              .Add("coordinate_dip",
                   base::ToString(target_->get_coordinate_dip()))
              .Add("target_id",
                   target_node.IsNull() ? 0 : target_node.GetDomNodeId())
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
      return MakeOkResult();
    }

    if (target_node.IsNull()) {
      journal_->Log(
          task_id_, journal_name,
          JournalDetailsBuilder()
              .Add("coordinate_dip",
                   base::ToString(target_->get_coordinate_dip()))
              .Add("observed_target_id",
                   observed_target_->node_attribute->dom_node_id.value_or(0))
              .AddError("No target node at coordinate")
              .Build());
      if (base::FeatureList::IsEnabled(features::kGlicActorToctouValidation)) {
        return MakeResult(
            mojom::ActionResultCode::kObservedTargetElementDestroyed,
            /*requires_page_stabilization=*/false,
            "The element at the target location is destroyed");
      }
      return MakeOkResult();
    }

    // Target node for coordinate target is obtained through blink hit test
    // which includes shadow host elements.
    if (!observed_target_node.ContainsViaFlatTree(&target_node)) {
      journal_->Log(
          task_id_, journal_name,
          JournalDetailsBuilder()
              .Add("coordinate_dip",
                   base::ToString(target_->get_coordinate_dip()))
              .Add("target_id", target_node.GetDomNodeId())
              .Add("observed_target_id", observed_target_node.GetDomNodeId())
              .Add("target", NodeToDebugString(target_node))
              .Add("observed_target", NodeToDebugString(observed_target_node))
              .AddError("Wrong Node At Location")
              .Build());
      UmaHistogramEnumeration(histogram_name,
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

    const bool allows_occluded_direct_activation =
        occlusion_mode ==
        TargetOcclusionMode::kAllowOccludedForDirectActivation;

    if (allows_occluded_direct_activation) {
      WebElement target_element = target_node.DynamicTo<WebElement>();
      if (target_element.IsNull() || !target_element.IsConnected()) {
        journal_->Log(task_id_, journal_name,
                      JournalDetailsBuilder()
                          .Add("target_id", target_node.GetDomNodeId())
                          .Add("target", NodeToDebugString(target_node))
                          .AddError("Direct-activation target is not present "
                                    "in the live DOM")
                          .Build());
        UmaHistogramEnumeration(histogram_name,
                                TimeOfUseResult::kTargetNodeMissing);
        return MakeResult(
            mojom::ActionResultCode::kFrameLocationChangedSinceObservation,
            /*requires_page_stabilization=*/false,
            "The direct-activation target is no longer connected.");
      }

      blink::WebDocument target_document = target_element.GetDocument();
      WebLocalFrame* target_frame =
          target_document.IsNull() ? nullptr : target_document.GetFrame();
      if (!target_frame || !target_document.IsActive()) {
        journal_->Log(task_id_, journal_name,
                      JournalDetailsBuilder()
                          .Add("target_id", target_element.GetDomNodeId())
                          .Add("target", NodeToDebugString(target_element))
                          .AddError("Direct-activation target frame is not "
                                    "active")
                          .Build());
        UmaHistogramEnumeration(histogram_name,
                                TimeOfUseResult::kTargetNodeMissing);
        return MakeResult(
            mojom::ActionResultCode::kFrameLocationChangedSinceObservation,
            /*requires_page_stabilization=*/false,
            "The direct-activation target frame is no longer active.");
      }

      if (!target_frame->IsOutermostMainFrame()) {
        journal_->Log(task_id_, journal_name,
                      JournalDetailsBuilder()
                          .Add("target_id", target_element.GetDomNodeId())
                          .Add("target", NodeToDebugString(target_element))
                          .AddError("Direct activation for occluded targets is "
                                    "not in the main frame")
                          .Build());
        return MakeResult(
            mojom::ActionResultCode::kArgumentsInvalid,
            /*requires_page_stabilization=*/false,
            "Direct activation for occluded targets is only supported in the "
            "main frame.");
      }

      if (!IsPointWithinViewport(resolved_target.widget_point, frame_.get())) {
        journal_->Log(
            task_id_, journal_name,
            JournalDetailsBuilder()
                .Add("target_id", target_element.GetDomNodeId())
                .Add("point", gfx::ToFlooredPoint(resolved_target.widget_point))
                .Add("target", NodeToDebugString(target_element))
                .AddError("Direct-activation target point is outside the "
                          "viewport")
                .Build());
        return MakeResult(
            mojom::ActionResultCode::kFrameLocationChangedSinceObservation,
            /*requires_page_stabilization=*/false,
            "The direct-activation target point is outside the viewport.");
      }

      const WebElementAuthorBarrierReason author_barrier_reason =
          GetWebElementAuthorBarrierReason(target_element);
      if (author_barrier_reason != WebElementAuthorBarrierReason::kNone) {
        journal_->Log(task_id_, journal_name,
                      JournalDetailsBuilder()
                          .Add("target_id", target_element.GetDomNodeId())
                          .Add("target", NodeToDebugString(target_element))
                          .Add("reason", ToString(author_barrier_reason))
                          .AddError("Page-authored state blocks direct "
                                    "activation of this target")
                          .Build());
        UmaHistogramEnumeration(
            histogram_name,
            TimeOfUseResult::kTargetNodeInteractionPointObscured);
        return MakeResult(
            mojom::ActionResultCode::kTargetNodeInteractionPointObscured,
            /*requires_page_stabilization=*/false,
            "Page-authored state blocks direct activation of this target.");
      }

      WebWidget* widget = resolved_target.GetWidget(*this);
      if (!widget) {
        journal_->Log(task_id_, journal_name,
                      JournalDetailsBuilder()
                          .Add("target_id", target_element.GetDomNodeId())
                          .Add("target", NodeToDebugString(target_element))
                          .AddError("Direct-activation target widget is not "
                                    "available")
                          .Build());
        return MakeResult(
            mojom::ActionResultCode::kFrameLocationChangedSinceObservation,
            /*requires_page_stabilization=*/false,
            "The direct-activation target widget is no longer available.");
      }

      const WebHitTestResult hit_test_result =
          widget->HitTestResultAt(resolved_target.widget_point);
      const WebElement hit_element = hit_test_result.GetElement();
      if (hit_element.IsNull()) {
        journal_->Log(task_id_, journal_name,
                      JournalDetailsBuilder()
                          .Add("target_id", target_element.GetDomNodeId())
                          .Add("target", NodeToDebugString(target_element))
                          .AddError("Direct-activation hit test returned no "
                                    "element")
                          .Build());
        UmaHistogramEnumeration(
            histogram_name,
            TimeOfUseResult::kTargetNodeInteractionPointObscured);
        return MakeResult(
            mojom::ActionResultCode::kTargetNodeInteractionPointObscured,
            /*requires_page_stabilization=*/false,
            "The target's interaction point is not covered by an eligible "
            "panel.");
      }

      // If the hit is inside a same-process iframe, use its frame owner in the
      // target document so we can test whether it belongs to the target
      // subtree. Merely being in the same document is not enough; an unrelated
      // element there may still be the occluding panel.
      const WebElement hit_element_in_target_document =
          GetHitElementInTargetDocument(hit_element, target_document);

      // If the live hit is the target or one of its flat-tree children, the
      // page no longer needs click-behind handling.
      if (!hit_element_in_target_document.IsNull() &&
          target_node.ContainsViaFlatTree(&hit_element_in_target_document)) {
        journal_->Log(task_id_, journal_name,
                      JournalDetailsBuilder()
                          .Add("target_id", target_element.GetDomNodeId())
                          .Add("hit_node_id",
                               hit_element_in_target_document.GetDomNodeId())
                          .Add("target", NodeToDebugString(target_element))
                          .Add("hit_node", NodeToDebugString(
                                               hit_element_in_target_document))
                          .AddError("Direct-activation target is no longer "
                                    "occluded")
                          .Build());
        UmaHistogramEnumeration(
            histogram_name,
            TimeOfUseResult::kTargetNodeInteractionPointObscured);
        return MakeResult(
            mojom::ActionResultCode::kTargetNodeInteractionPointObscured,
            /*requires_page_stabilization=*/false,
            "The target is no longer occluded; use a normal click instead.");
      }

      // Server-side APC selection already decided whether this occluder is a
      // bypassable modeless panel. The client revalidates that the live hit or
      // its top-document iframe owner is inside a fixed/absolute panel.
      const WebElement panel =
          GetFixedOrAbsolutePanel(hit_element_in_target_document);
      if (panel.IsNull()) {
        JournalDetailsBuilder builder;
        builder.Add("target_id", target_element.GetDomNodeId())
            .Add("hit_node_id", hit_element.GetDomNodeId())
            .Add("target", NodeToDebugString(target_element))
            .Add("hit_node", NodeToDebugString(hit_element));
        if (!hit_element_in_target_document.IsNull() &&
            hit_element.GetDocument() != target_document) {
          builder
              .Add("target_document_hit_node_id",
                   hit_element_in_target_document.GetDomNodeId())
              .Add("target_document_hit_node",
                   NodeToDebugString(hit_element_in_target_document));
        }
        builder.Add("status", "NoEligiblePanel")
            .AddError(
                "Direct-activation target is not covered by an eligible "
                "fixed or absolute panel");
        journal_->Log(task_id_, journal_name, std::move(builder).Build());
        UmaHistogramEnumeration(
            histogram_name,
            TimeOfUseResult::kTargetNodeInteractionPointObscured);
        return MakeResult(
            mojom::ActionResultCode::kTargetNodeInteractionPointObscured,
            /*requires_page_stabilization=*/false,
            "The target's interaction point is not covered by an eligible "
            "out-of-flow panel.");
      }
    } else {
      WebWidget* widget = resolved_target.GetWidget(*this);
      CHECK(widget);

      // Check that the interaction point will actually hit on the intended
      // element, i.e. center point of node is not occluded.
      const WebHitTestResult hit_test_result =
          widget->HitTestResultAt(resolved_target.widget_point);
      const WebElement hit_element = hit_test_result.GetElement();
      if (hit_element.IsNull()) {
        journal_->Log(task_id_, journal_name,
                      JournalDetailsBuilder()
                          .Add("target_id", target_node.GetDomNodeId())
                          .Add("target", NodeToDebugString(target_node))
                          .AddError("Hit test returned no element")
                          .Build());
        UmaHistogramEnumeration(
            histogram_name,
            TimeOfUseResult::kTargetNodeInteractionPointObscured);
        if (base::FeatureList::IsEnabled(
                features::kGlicActorToctouValidation)) {
          return MakeResult(
              mojom::ActionResultCode::kTargetNodeInteractionPointObscured,
              /*requires_page_stabilization=*/false,
              "The element's interaction point is obscured by other elements.");
        } else {
          return MakeOkResult();
        }
      }

      // The action target from APC is not as granular as the live DOM hit
      // test. Include shadow host element as the hit test would land on those.
      // Also check if the hit element was pulled in via a Web Components slot.
      if (!target_node.ContainsViaFlatTree(&hit_element)) {
        journal_->Log(task_id_, journal_name,
                      JournalDetailsBuilder()
                          .Add("target_id", target_node.GetDomNodeId())
                          .Add("hit_node_id", hit_element.GetDomNodeId())
                          .Add("target", NodeToDebugString(target_node))
                          .Add("hit_node", NodeToDebugString(hit_element))
                          .AddError("Node covered by another node")
                          .Build());
        UmaHistogramEnumeration(
            histogram_name,
            TimeOfUseResult::kTargetNodeInteractionPointObscured);
        if (base::FeatureList::IsEnabled(
                features::kGlicActorToctouValidation)) {
          return MakeResult(
              mojom::ActionResultCode::kTargetNodeInteractionPointObscured,
              /*requires_page_stabilization=*/false,
              "The element's interaction point is obscured by other elements.");
        } else {
          return MakeOkResult();
        }
      }
    }

    if (!observed_target_ || !observed_target_->node_attribute->dom_node_id) {
      journal_->Log(
          task_id_, journal_name,
          JournalDetailsBuilder().AddError("No valid APC node").Build());
      UmaHistogramEnumeration(histogram_name, TimeOfUseResult::kNoValidApcNode);
      if (allows_occluded_direct_activation ||
          base::FeatureList::IsEnabled(features::kGlicActorToctouValidation)) {
        // Direct activation bypasses hit testing. It must prove the DOM node
        // came from APC even when the broader TOCTOU rollout is disabled.
        return MakeResult(
            mojom::ActionResultCode::kFrameLocationChangedSinceObservation,
            /*requires_page_stabilization=*/false,
            "No prior observation available for TOCTOU validation");
      }
      return MakeOkResult();
    }

    if (!observed_target_->node_attribute->geometry) {
      journal_->Log(
          task_id_, journal_name,
          JournalDetailsBuilder()
              .Add("obs_node_id",
                   *observed_target_->node_attribute->dom_node_id)
              .Add("point", gfx::ToFlooredPoint(resolved_target.widget_point))
              .AddError("No geometry for node")
              .Build());
      // TODO(crbug.com/418280472): return error after retry for failed task
      // is landed.
      UmaHistogramEnumeration(histogram_name,
                              TimeOfUseResult::kTargetNodeMissingGeometry);
      if (allows_occluded_direct_activation) {
        // Direct activation bypasses hit testing. Without APC geometry, the
        // request is malformed because it is not tied to an observed on-page
        // target.
        return MakeResult(
            mojom::ActionResultCode::kArgumentsInvalid,
            /*requires_page_stabilization=*/false,
            "Direct activation requires APC geometry for the target.");
      }
      return MakeOkResult();
    }

    // Check that the interaction point is inside the observed target bounding
    // box from last APC.
    const gfx::Rect observed_bounds =
        observed_target_->node_attribute->geometry->outer_bounding_box;
    if (!observed_bounds.Contains(
            gfx::ToFlooredPoint(resolved_target.widget_point))) {
      journal_->Log(task_id_, journal_name,
                    JournalDetailsBuilder()
                        .Add("resolved_target_point",
                             gfx::ToFlooredPoint(resolved_target.widget_point))
                        .Add("bounding_box", observed_bounds)
                        .AddError("Point not in box")
                        .Build());
      // TODO(crbug.com/418280472): return error after retry for failed task
      // is landed.
      UmaHistogramEnumeration(histogram_name,
                              TimeOfUseResult::kTargetPointOutsideBoundingBox);
      if (allows_occluded_direct_activation) {
        // Direct activation bypasses coordinate dispatch. A stale APC box means
        // the model-selected DOM id no longer matches the live target point.
        return MakeResult(
            mojom::ActionResultCode::kFrameLocationChangedSinceObservation,
            /*requires_page_stabilization=*/false,
            "The target point no longer matches the observed APC bounds.");
      }
      return MakeOkResult();
    }
  }

  UmaHistogramEnumeration(histogram_name, TimeOfUseResult::kValid);
  return MakeOkResult();
}

ValidationResult::ValidationResult(mojom::ActionResultPtr result,
                                   std::optional<gfx::PointF> target_point)
    : result(std::move(result)), target_point(target_point) {}

ValidationResult::~ValidationResult() = default;

ValidationResult::ValidationResult(ValidationResult&&) = default;

ValidationResult& ValidationResult::operator=(ValidationResult&&) = default;

}  // namespace actor
