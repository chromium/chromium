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
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/chrome_features.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_hit_test_result.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

using base::UmaHistogramEnumeration;
using blink::WebElement;
using blink::WebNode;

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

}  // namespace

base::TimeDelta ToolBase::ExecutionObservationDelay() const {
  return base::TimeDelta();
}

bool ToolBase::SupportsPaintStability() const {
  return false;
}

ToolBase::ToolBase(content::RenderFrame& frame,
                   Journal::TaskId task_id,
                   Journal& journal,
                   mojom::ToolTargetPtr target,
                   mojom::ObservedToolTargetPtr observed_target)
    : frame_(frame),
      task_id_(task_id),
      journal_(journal),
      target_(std::move(target)),
      observed_target_(std::move(observed_target)) {}

ToolBase::~ToolBase() = default;

base::expected<ToolBase::ResolvedTarget, mojom::ActionResultPtr>
ToolBase::ValidateAndResolveTarget() const {
  if (!target_) {
    return base::unexpected(MakeResult(mojom::ActionResultCode::kOk));
  }

  ResolvedTarget resolved_target;

  if (target_->is_coordinate()) {
    const gfx::PointF coordinate_point(target_->get_coordinate());
    if (!IsPointWithinViewport(coordinate_point, frame_.get())) {
      return base::unexpected(MakeResult(
          mojom::ActionResultCode::kCoordinatesOutOfBounds,
          absl::StrFormat("Point [%s]", coordinate_point.ToString())));
    }
    resolved_target.point = coordinate_point;

    // Perform a hit test to find the node at the coordinates.
    const blink::WebHitTestResult hit_test_result =
        frame_->GetWebFrame()->FrameWidget()->HitTestResultAt(
            resolved_target.point);
    resolved_target.node = hit_test_result.GetNode();
  } else if (target_->is_dom_node_id()) {
    int32_t dom_node_id = target_->get_dom_node_id();
    resolved_target.node = GetNodeFromId(frame_.get(), dom_node_id);
    if (resolved_target.node.IsNull()) {
      return base::unexpected(
          MakeResult(mojom::ActionResultCode::kInvalidDomNodeId));
    }

    std::optional<gfx::PointF> node_interaction_point =
        InteractionPointFromWebNode(resolved_target.node);
    if (!node_interaction_point.has_value()) {
      return base::unexpected(
          MakeResult(mojom::ActionResultCode::kElementOffscreen,
                     absl::StrFormat("[Element %s]",
                                     base::ToString(resolved_target.node))));
    }
    resolved_target.point = *node_interaction_point;
  }

  return ValidateTimeOfUse(resolved_target);
}

void ToolBase::EnsureTargetInView() {
  if (!target_) {
    return;
  }

  // Scrolling a target into view is only supported for node_id targets since
  // TOCTOU checks cannot be applied to the APC captured at the old scroll
  // offset.
  if (target_->is_coordinate()) {
    return;
  }

  int32_t dom_node_id = target_->get_dom_node_id();
  WebElement node =
      GetNodeFromId(frame_.get(), dom_node_id).DynamicTo<WebElement>();
  if (node && !InteractionPointFromWebNode(node).has_value()) {
    node.ScrollIntoViewIfNeeded();
  }
}

base::expected<ToolBase::ResolvedTarget, mojom::ActionResultPtr>
ToolBase::ValidateTimeOfUse(const ResolvedTarget& resolved_target) const {
  const blink::WebNode& target_node = resolved_target.node;

  // For coordinate target, check the observed node matches the live DOM hit
  // test target.
  if (target_->is_coordinate()) {
    if (!observed_target_ || !observed_target_->node_attribute->dom_node_id) {
      journal_->Log(
          task_id_, "TimeOfUseValidation",
          JournalDetailsBuilder().AddError("No valid APC node").Build());
      UmaHistogramEnumeration(kTimeOfUseValidationHistogram,
                              TimeOfUseResult::kNoValidApcNode);
      // TODO(crbug.com/445210509): return error for no apc found.
      return resolved_target;
    }

    if (target_node.GetDomNodeId() !=
        *observed_target_->node_attribute->dom_node_id) {
      journal_->Log(task_id_, "TimeOfUseValidation",
                    JournalDetailsBuilder()
                        .Add("obs_node_id",
                             *observed_target_->node_attribute->dom_node_id)
                        .Add("target_node_id", target_node.GetDomNodeId())
                        .Add("target", NodeToDebugSring(target_node))
                        .AddError("Wrong Node At Location")
                        .Build());
      UmaHistogramEnumeration(kTimeOfUseValidationHistogram,
                              TimeOfUseResult::kWrongNodeAtCoordinate);
      if (base::FeatureList::IsEnabled(features::kGlicActorToctouValidation)) {
        return base::unexpected(
            MakeResult(mojom::ActionResultCode::kObservedTargetElementChanged,
                       "The element at the target location is not the same as "
                       "the one observed."));
      } else {
        return resolved_target;
      }
    }
  } else {
    CHECK(target_->is_dom_node_id());
    // Check that the interaction point will actually hit
    // on the intended element, i.e. centre point of node is not occluded.
    const blink::WebHitTestResult hit_test_result =
        frame_->GetWebFrame()->FrameWidget()->HitTestResultAt(
            resolved_target.point);
    const blink::WebElement hit_element = hit_test_result.GetElement();
    // The action target from APC is not as granular as the live DOM hit test.
    // Include shadow host element as the hit test would land on those.
    if (!target_node.ContainsIncludingHostElements(&hit_element)) {
      journal_->Log(task_id_, "TimeOfUseValidation",
                    JournalDetailsBuilder()
                        .Add("target_id", target_node.GetDomNodeId())
                        .Add("hit_node_id", hit_element.GetDomNodeId())
                        .Add("target", NodeToDebugSring(target_node))
                        .Add("hit_node", NodeToDebugSring(hit_element))
                        .AddError("Node covered by another node")
                        .Build());
      UmaHistogramEnumeration(
          kTimeOfUseValidationHistogram,
          TimeOfUseResult::kTargetNodeInteractionPointObscured);
      return base::unexpected(MakeResult(
          mojom::ActionResultCode::kTargetNodeInteractionPointObscured,
          "The element's interaction point is obscured by other elements."));
    }

    if (!observed_target_ || !observed_target_->node_attribute->dom_node_id) {
      journal_->Log(
          task_id_, "TimeOfUseValidation",
          JournalDetailsBuilder().AddError("No valid APC node").Build());
      UmaHistogramEnumeration(kTimeOfUseValidationHistogram,
                              TimeOfUseResult::kNoValidApcNode);
      // TODO(crbug.com/445210509): return error for no apc found.
      return resolved_target;
    }

    if (!observed_target_->node_attribute->geometry) {
      journal_->Log(
          task_id_, "TimeOfUseValidation",
          JournalDetailsBuilder()
              .Add("obs_node_id",
                   *observed_target_->node_attribute->dom_node_id)
              .Add("point", gfx::ToFlooredPoint(resolved_target.point))
              .AddError("No geometry for node")
              .Build());
      // TODO(crbug.com/418280472): return error after retry for failed task is
      // landed.
      UmaHistogramEnumeration(kTimeOfUseValidationHistogram,
                              TimeOfUseResult::kTargetNodeMissingGeometry);
      return resolved_target;
    }

    // Check that the interaction point is inside the observed target bounding
    // box from last APC.
    const gfx::Rect observed_bounds =
        observed_target_->node_attribute->geometry->outer_bounding_box;
    if (!observed_bounds.Contains(gfx::ToFlooredPoint(resolved_target.point))) {
      journal_->Log(task_id_, "TimeOfUseValidation",
                    JournalDetailsBuilder()
                        .Add("resolved_target_point",
                             gfx::ToFlooredPoint(resolved_target.point))
                        .Add("bounding_box", observed_bounds)
                        .AddError("Point not in box")
                        .Build());
      // TODO(crbug.com/418280472): return error after retry for failed task is
      // landed.
      UmaHistogramEnumeration(kTimeOfUseValidationHistogram,
                              TimeOfUseResult::kTargetPointOutsideBoundingBox);
      return resolved_target;
    }
  }

  UmaHistogramEnumeration(kTimeOfUseValidationHistogram,
                          TimeOfUseResult::kValid);
  return resolved_target;
}

}  // namespace actor
