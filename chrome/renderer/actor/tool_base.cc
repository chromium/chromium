// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/tool_base.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/expected.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_hit_test_result.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace actor {

using blink::WebNode;

base::TimeDelta ToolBase::ExecutionObservationDelay() const {
  return base::TimeDelta();
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
    resolved_target.node = hit_test_result.GetElement();

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

base::expected<ToolBase::ResolvedTarget, mojom::ActionResultPtr>
ToolBase::ValidateTimeOfUse(const ResolvedTarget& resolved_target) const {
  if (!observed_target_ || !observed_target_->node_attribute->dom_node_id) {
    return resolved_target;
  }

  const blink::WebNode& target_node = resolved_target.node;

  // For coordinate target, check the observed node matches the live DOM hit
  // test target.
  if (target_->is_coordinate()) {
    if (target_node.GetDomNodeId() !=
        *observed_target_->node_attribute->dom_node_id) {
      journal_->Log(
          task_id_, "TimeOfUseValidation",
          base::StrCat({"Observed Target Node:",
                        base::NumberToString(
                            *observed_target_->node_attribute->dom_node_id),
                        " Hit Test Node:",
                        base::NumberToString(target_node.GetDomNodeId())}));
      return base::unexpected(
          MakeResult(mojom::ActionResultCode::kObservedTargetElementChanged,
                     "The element at the target location is not the same as "
                     "the one observed."));
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
    if (target_node.Contains(&hit_element)) {
      journal_->Log(
          task_id_, "TimeOfUseValidation",
          base::StrCat({"Observed Target Node:",
                        base::NumberToString(
                            *observed_target_->node_attribute->dom_node_id),
                        " Hit Test Node:",
                        base::NumberToString(target_node.GetDomNodeId())}));
      // TODO(crbug.com/418280472): return error after retry for failed task is
      // landed.
    }

    // Check that the interaction point is inside the observed target bounding
    // box.
    const gfx::Rect observed_bounds =
        observed_target_->node_attribute->geometry->outer_bounding_box;
    if (!observed_bounds.Contains(gfx::ToFlooredPoint(resolved_target.point))) {
      journal_->Log(
          task_id_, "TimeOfUseValidation",
          base::StrCat(
              {"Target interaction point:",
               base::ToString(gfx::ToFlooredPoint(resolved_target.point)),
               " Observed bounding box:", base::ToString(observed_bounds)}));
      // TODO(crbug.com/418280472): return error after retry for failed task is
      // landed.
    }
  }

  return resolved_target;
}

}  // namespace actor
