// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/click_tool.h"

#include <cstdint>
#include <optional>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/common/actor.mojom-shared.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/renderer/actor/click_dispatcher.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "components/actor/core/actor_logging.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/latency/latency_info.h"

namespace actor {

using ::blink::WebCoalescedInputEvent;
using ::blink::WebElement;
using ::blink::WebFormControlElement;
using ::blink::WebFrameWidget;
using ::blink::WebInputEvent;
using ::blink::WebInputEventResult;
using ::blink::WebMouseEvent;
using ::blink::WebNode;

ClickTool::ClickTool(content::RenderFrame& frame,
                     TaskId task_id,
                     Journal& journal,
                     mojom::ClickActionPtr action,
                     mojom::ToolTargetPtr target,
                     mojom::ObservedToolTargetPtr observed_target)
    : ToolBase(frame,
               task_id,
               journal,
               std::move(target),
               std::move(observed_target)),
      action_(std::move(action)) {}

ClickTool::~ClickTool() = default;

void ClickTool::Execute(ToolFinishedCallback callback) {
  CHECK(validated_target_.has_value())
      << "Execute tool was called before validation";

  const bool use_direct_activation =
      action_->type == mojom::ClickType::kLeftOnOccludedTarget &&
      base::FeatureList::IsEnabled(
          features::kGlicActorOccludedDirectActivation);
  if (use_direct_activation) {
    // The direct path bypasses normal coordinate dispatch. Re-check the live
    // target immediately before sending trusted events so a page cannot change
    // the DOM target after validation and still receive activation.
    auto revalidated_target = ResolveValidatedClickTarget(
        TargetOcclusionMode::kAllowOccludedForDirectActivation);
    if (!revalidated_target.has_value()) {
      std::move(callback).Run(std::move(revalidated_target.error()));
      return;
    }
    ResolvedTarget direct_activation_target =
        std::move(revalidated_target.value());

    journal_->Log(task_id_, "ClickTool::Execute",
                  JournalDetailsBuilder()
                      .Add("point", direct_activation_target.widget_point)
                      .Build());

    WebElement element = direct_activation_target.node.DynamicTo<WebElement>();
    if (element.IsNull()) {
      std::move(callback).Run(MakeResult(
          mojom::ActionResultCode::kArgumentsInvalid,
          /*requires_page_stabilization=*/false,
          "Direct activation requires the resolved target to be an element."));
      return;
    }

    // Blink keeps DOM-owned dispatch semantics inside this accessibility-style
    // simulated click path.
    const bool activated = element.SimulateAccessibilityClick();

    std::move(callback).Run(
        activated ? MakeOkResult()
                  : MakeResult(mojom::ActionResultCode::kClickSuppressed,
                               /*requires_page_stabilization=*/false,
                               "The renderer declined activation of this "
                               "element."));
    return;
  }

  ResolvedTarget target = validated_target_.value();

  journal_->Log(
      task_id_, "ClickTool::Execute",
      JournalDetailsBuilder().Add("point", target.widget_point).Build());

  WebMouseEvent::Button button;
  switch (action_->type) {
    case mojom::ClickType::kLeft: {
      button = WebMouseEvent::Button::kLeft;
      break;
    }
    case mojom::ClickType::kLeftOnOccludedTarget: {
      // Validate() rejects this type when direct activation is disabled, and
      // direct activation returns before this coordinate-dispatch path.
      NOTREACHED();
    }
    case mojom::ClickType::kRight: {
      button = WebMouseEvent::Button::kRight;
      break;
    }
  }
  int click_count;
  switch (action_->count) {
    case mojom::ClickCount::kSingle: {
      click_count = 1;
      break;
    }
    case mojom::ClickCount::kDouble: {
      click_count = 2;
      break;
    }
  }

  // TODO(b/467336183): For debugging; should be removed once this bug is
  // resolved.
  journal_->SendLogBuffer();

  CHECK(!click_dispatcher_);
  click_dispatcher_.emplace(button, click_count, target, *this,
                            std::move(callback));
}

std::string ClickTool::DebugString() const {
  return absl::StrFormat("ClickTool[%s;type(%s);count(%s)]",
                         ToDebugString(target_), base::ToString(action_->type),
                         base::ToString(action_->count));
}

bool ClickTool::SupportsPaintStability() const {
  return true;
}

void ClickTool::Cancel() {
  if (click_dispatcher_) {
    // click_dispatcher_->Cancel() synchronously dispatches DOM events that
    // might destroy the owning frame and this tool. Use a weak pointer to
    // detect if `this` is still valid.
    base::WeakPtr<ClickTool> weak_this = weak_ptr_factory_.GetWeakPtr();
    click_dispatcher_->Cancel();
    if (weak_this) {
      click_dispatcher_.reset();
    }
  }
}

ToolBase::ResolveResult ClickTool::ResolveValidatedClickTarget(
    TargetOcclusionMode occlusion_mode) const {
  auto resolved_target = ValidateAndResolveTarget(occlusion_mode);
  if (!resolved_target.has_value()) {
    return resolved_target;
  }

  // Perform click validation on the resolved node. This is shared by initial
  // validation and the final direct-activation check so last-moment state
  // changes do not skip normal click preconditions.
  const WebNode& node = resolved_target->node;
  if (!node.IsNull()) {
    WebElement element = node.DynamicTo<WebElement>();
    WebFormControlElement form_element =
        node.DynamicTo<WebFormControlElement>();
    if (!form_element.IsNull() && !form_element.IsEnabled()) {
      return base::unexpected(MakeResult(
          mojom::ActionResultCode::kElementDisabled,
          /*requires_page_stabilization=*/false,
          absl::StrFormat("[Element %s]", base::ToString(form_element))));
    }
    if (occlusion_mode ==
            TargetOcclusionMode::kAllowOccludedForDirectActivation &&
        !element.IsNull() && element.IsEffectivelyDisabledOrInert()) {
      return base::unexpected(
          MakeResult(mojom::ActionResultCode::kElementDisabled,
                     /*requires_page_stabilization=*/false,
                     "The target element is disabled, inert, or otherwise "
                     "unavailable."));
    }
  }

  return resolved_target;
}

ValidationResult ClickTool::Validate() {
  CHECK(frame_->GetWebFrame());
  CHECK(frame_->GetWebFrame()->FrameWidget());

  const bool has_occluded_click_type =
      action_->type == mojom::ClickType::kLeftOnOccludedTarget;
  const bool direct_activation_requested = has_occluded_click_type;
  if (direct_activation_requested &&
      !base::FeatureList::IsEnabled(
          features::kGlicActorOccludedDirectActivation)) {
    return ValidationResult(
        MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                   /*requires_page_stabilization=*/false,
                   "Direct activation for occluded targets is disabled."));
  }

  if (direct_activation_requested &&
      action_->count != mojom::ClickCount::kSingle) {
    return ValidationResult(
        MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                   /*requires_page_stabilization=*/false,
                   "Direct activation only supports single left clicks."));
  }

  // Direct activation is server-approved for occluded DOM targets. Renderer
  // validation still proves the target is live and tied to the APC observation,
  // but it does not duplicate server-side modeless-panel eligibility checks.
  auto resolved_target = ResolveValidatedClickTarget(
      direct_activation_requested
          ? TargetOcclusionMode::kAllowOccludedForDirectActivation
          : TargetOcclusionMode::kRequireUnoccluded);
  if (!resolved_target.has_value()) {
    return ValidationResult(std::move(resolved_target.error()));
  }

  validated_target_ = std::move(resolved_target.value());
  return ValidationResult(MakeOkResult(), validated_target_->widget_point);
}

}  // namespace actor
