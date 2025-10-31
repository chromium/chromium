// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/drag_and_release_tool.h"

#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/chrome_features.h"
#include "chrome/renderer/actor/tool_utils.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point_f.h"

namespace actor {

using ::blink::WebCoalescedInputEvent;
using ::blink::WebInputEvent;
using ::blink::WebInputEventResult;
using ::blink::WebLocalFrame;
using ::blink::WebMouseEvent;
using ::blink::WebWidget;
using ::blink::mojom::EventType;

DragAndReleaseTool::DragAndReleaseTool(
    content::RenderFrame& frame,
    TaskId task_id,
    Journal& journal,
    mojom::DragAndReleaseActionPtr action,
    mojom::ToolTargetPtr target,
    mojom::ObservedToolTargetPtr observed_target)
    : ToolBase(frame,
               task_id,
               journal,
               std::move(target),
               std::move(observed_target)),
      action_(std::move(action)) {}

DragAndReleaseTool::~DragAndReleaseTool() = default;

void DragAndReleaseTool::Execute(ToolFinishedCallback callback) {
  ValidatedResult validated_result = Validate();
  if (!validated_result.has_value()) {
    std::move(callback).Run(std::move(validated_result.error()));
    return;
  }

  ResolvedTarget from_target = validated_result->from;
  ResolvedTarget to_target = validated_result->to;

  journal_->Log(task_id_, "DragAndReleaseTool::Execute",
                JournalDetailsBuilder()
                    .Add("from", from_target.widget_point)
                    .Add("to", to_target.widget_point)
                    .Build());

  WebWidget* widget = from_target.GetWidget(*this);
  CHECK(widget);

  // TODO(crbug.com/409333494): How should partial success be returned.

  // Move and press down the mouse on the from_point.
  if (!InjectMouseEvent(*widget, from_target.widget_point,
                        EventType::kMouseMove,
                        WebMouseEvent::Button::kNoButton)) {
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kDragAndReleaseFromMoveSuppressed));
    return;
  }

  // Re-check widget after each input since it could be destroyed as part of
  // input handling.
  widget = from_target.GetWidget(*this);
  if (!widget) {
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kFrameWentAway));
    return;
  }

  if (!InjectMouseEvent(*widget, from_target.widget_point,
                        EventType::kMouseDown, WebMouseEvent::Button::kLeft)) {
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kDragAndReleaseDownSuppressed,
                   /*requires_page_stabilization=*/true));
    return;
  }

  widget = from_target.GetWidget(*this);
  if (!widget) {
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kFrameWentAway));
    return;
  }

  // Move and release the mouse on the to_point.
  if (!InjectMouseEvent(*widget, to_target.widget_point, EventType::kMouseMove,
                        WebMouseEvent::Button::kLeft)) {
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kDragAndReleaseToMoveSuppressed,
                   /*requires_page_stabilization=*/true));
    return;
  }

  widget = from_target.GetWidget(*this);
  if (!widget) {
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kFrameWentAway));
    return;
  }

  if (!InjectMouseEvent(*widget, to_target.widget_point, EventType::kMouseUp,
                        WebMouseEvent::Button::kLeft)) {
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kDragAndReleaseUpSuppressed,
                   /*requires_page_stabilization=*/true));
    return;
  }

  std::move(callback).Run(MakeOkResult());
}

std::string DragAndReleaseTool::DebugString() const {
  return absl::StrFormat("DragAndReleaseTool[from-%s -> to-%s]",
                         ToDebugString(target_),
                         ToDebugString(action_->to_target));
}

DragAndReleaseTool::ValidatedResult DragAndReleaseTool::Validate() const {
  CHECK(frame_->GetWebFrame());
  CHECK(frame_->GetWebFrame()->FrameWidget());

  const mojom::ToolTargetPtr& from_target = target_;
  const mojom::ToolTargetPtr& to_target = action_->to_target;

  CHECK(from_target);
  CHECK(to_target);

  ResolveResult resolved_from = ResolveTarget(*from_target);
  ResolveResult resolved_to = ResolveTarget(*to_target);

  if (!resolved_from.has_value()) {
    return base::unexpected(std::move(resolved_from.error()));
  }

  if (!resolved_to.has_value()) {
    return base::unexpected(std::move(resolved_to.error()));
  }

  if (resolved_from->GetWidget(*this) != resolved_to->GetWidget(*this)) {
    // Drag across widgets (i.e. between frame and popup) isn't currently
    // supported.
    return base::unexpected(MakeErrorResult());
  }

  // TODO(b/450018073): This should be checking the targets for time-of-use
  // validity.

  return DragParams{resolved_from.value(), resolved_to.value()};
}

bool DragAndReleaseTool::InjectMouseEvent(WebWidget& widget,
                                          gfx::PointF& position_in_widget,
                                          WebInputEvent::Type type,
                                          WebMouseEvent::Button button) {
  WebMouseEvent mouse_event(type, WebInputEvent::kNoModifiers,
                            ui::EventTimeForNow());
  mouse_event.SetPositionInWidget(position_in_widget);
  mouse_event.button = button;

  if (type == WebInputEvent::Type::kMouseDown ||
      type == WebInputEvent::Type::kMouseUp) {
    mouse_event.click_count = 1;
  }

  if (base::FeatureList::IsEnabled(features::kGlicActorUseDragModifiers)) {
    mouse_event.UpdateEventModifiersToMatchButton();
    if (type == WebInputEvent::Type::kMouseMove) {
      switch (button) {
        case blink::WebMouseEvent::Button::kNoButton:
          break;
        case blink::WebMouseEvent::Button::kLeft:
          mouse_event.SetModifiers(WebInputEvent::Modifiers::kLeftButtonDown);
          break;
        default:
          NOTREACHED();
      }
    }
  }

  WebInputEventResult result = widget.HandleInputEvent(
      WebCoalescedInputEvent(mouse_event, ui::LatencyInfo()));
  return result != WebInputEventResult::kHandledSuppressed;
}

}  // namespace actor
