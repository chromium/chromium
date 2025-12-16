// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/click_dispatcher.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/renderer/actor/tool_base.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/web/web_widget.h"
#include "ui/events/base_event_utils.h"
#include "ui/latency/latency_info.h"

namespace actor {

using ::blink::WebInputEvent;
using ::blink::WebLocalFrame;
using ::blink::WebMouseEvent;
using ::blink::WebWidget;

ClickDispatcher::ClickDispatcher(
    WebMouseEvent::Button button,
    int count,
    const ResolvedTarget& target,
    const ToolBase& tool,
    base::OnceCallback<void(mojom::ActionResultPtr)> on_complete)
    : target_(target), tool_(tool), on_complete_(std::move(on_complete)) {
  WebWidget* widget = target.GetWidget(*tool_);
  CHECK(widget);
  if (base::FeatureList::IsEnabled(features::kGlicActorMoveBeforeClick)) {
    WebMouseEvent mouse_move(WebInputEvent::Type::kMouseMove,
                             WebInputEvent::kNoModifiers,
                             ui::EventTimeForNow());
    // No button for move
    mouse_move.button = WebMouseEvent::Button::kNoButton;
    mouse_move.SetPositionInWidget(target.widget_point);

    // Mouse move is considered optional, so we don't check this result.
    widget->HandleInputEvent(
        blink::WebCoalescedInputEvent(mouse_move, ui::LatencyInfo()));

    base::TimeDelta delay = features::kGlicActorMoveBeforeClickDelay.Get();
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ClickDispatcher::DoMouseDown,
                       weak_ptr_factory_.GetWeakPtr(), button, count, target),
        delay);
  } else {
    DoMouseDown(button, count, target);
  }
}

ClickDispatcher::~ClickDispatcher() = default;

void ClickDispatcher::Cancel() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  on_complete_.Reset();

  if (mouse_up_event_) {
    // If the mouse is down during cancelation, lift the mouse back up.
    DoMouseUpImpl();
  }
}

void ClickDispatcher::DoMouseDown(WebMouseEvent::Button button,
                                  int count,
                                  const ResolvedTarget& target) {
  WebWidget* widget = target.GetWidget(*tool_);
  if (!widget) {
    Finish(MakeResult(mojom::ActionResultCode::kFrameWentAway,
                      /*requires_page_stabilization=*/true,
                      "No widget when dispatching mouse down"));
    return;
  }

  WebMouseEvent mouse_down(WebInputEvent::Type::kMouseDown,
                           WebInputEvent::kNoModifiers, ui::EventTimeForNow());
  mouse_down.button = button;
  mouse_down.click_count = count;
  mouse_down.SetPositionInWidget(target.widget_point);
  // TODO(crbug.com/402082828): Find a way to set screen position.
  //   const gfx::Rect offset =
  //     render_frame_host_->GetRenderWidgetHost()->GetView()->GetViewBounds();
  //   mouse_event_.SetPositionInScreen(point.x() + offset.x(),
  //                                    point.y() + offset.y());

  blink::WebInputEventResult result = widget->HandleInputEvent(
      blink::WebCoalescedInputEvent(mouse_down, ui::LatencyInfo()));

  if (result == blink::WebInputEventResult::kHandledSuppressed) {
    Finish(MakeResult(mojom::ActionResultCode::kClickSuppressed,
                      /*requires_page_stabilization=*/false));
    return;
  }

  mouse_up_event_ = mouse_down;
  mouse_up_event_->SetType(WebInputEvent::Type::kMouseUp);

  const base::TimeDelta delay = features::kGlicActorClickDelay.Get();

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ClickDispatcher::DoMouseUp,
                     weak_ptr_factory_.GetWeakPtr()),
      delay);
}

void ClickDispatcher::DoMouseUp() {
  DoMouseUpImpl();
  Finish(MakeOkResult());
}

void ClickDispatcher::DoMouseUpImpl() {
  WebWidget* widget = target_.GetWidget(*tool_);

  if (!widget) {
    Finish(MakeResult(mojom::ActionResultCode::kFrameWentAway,
                      /*requires_page_stabilization=*/true,
                      "No widget when dispatching mouse up"));
    return;
  }

  mouse_up_event_->SetTimeStamp(ui::EventTimeForNow());
  blink::WebInputEventResult result =
      widget->HandleInputEvent(blink::WebCoalescedInputEvent(
          std::move(*mouse_up_event_), ui::LatencyInfo()));
  mouse_up_event_.reset();
  if (result == blink::WebInputEventResult::kHandledSuppressed) {
    Finish(MakeResult(mojom::ActionResultCode::kClickSuppressed,
                      /*requires_page_stabilization=*/false));
    return;
  }
}

void ClickDispatcher::Finish(mojom::ActionResultPtr result) {
  if (!on_complete_) {
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_complete_), std::move(result)));
  // This instance may now be deleted once the callback runs.
}

}  // namespace actor
