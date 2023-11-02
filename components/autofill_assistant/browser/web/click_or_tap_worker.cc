// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/click_or_tap_worker.h"

#include <vector>

#include "base/time/time.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"

namespace autofill_assistant {

ClickOrTapWorker::ClickOrTapWorker(DevtoolsClient* devtools_client)
    : devtools_client_(devtools_client) {}
ClickOrTapWorker::~ClickOrTapWorker() = default;

void ClickOrTapWorker::Start(const ElementFinderResult& element,
                             ClickType click_type,
                             Callback callback) {
  DCHECK(click_type == ClickType::CLICK || click_type == ClickType::TAP);
  click_type_ = click_type;

  DCHECK(!callback_);
  callback_ = std::move(callback);

  DCHECK(!element_position_getter_);
  node_frame_id_ = element.node_frame_id();
  element_position_getter_ = std::make_unique<ElementPositionGetter>(
      devtools_client_, /* max_rounds= */ 1,
      /* check_interval= */ base::Milliseconds(0), node_frame_id_);

  element_position_getter_->Start(
      element.render_frame_host(), element.object_id(),
      base::BindOnce(&ClickOrTapWorker::OnGetCoordinates,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ClickOrTapWorker::OnGetCoordinates(const ClientStatus& status) {
  if (!status.ok()) {
    VLOG(1) << __func__ << " Failed to get element position.";
    std::move(callback_).Run(status);
    return;
  }

  int x = element_position_getter_->x();
  int y = element_position_getter_->y();

  if (click_type_ == ClickType::CLICK) {
    devtools_client_->GetInput()->DispatchMouseEvent(
        input::DispatchMouseEventParams::Builder()
            .SetX(x)
            .SetY(y)
            .SetClickCount(1)
            .SetButton(input::MouseButton::LEFT)
            .SetType(input::DispatchMouseEventType::MOUSE_PRESSED)
            .Build(),
        node_frame_id_,
        base::BindOnce(&ClickOrTapWorker::OnDispatchPressMouseEvent,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (click_type_ == ClickType::TAP) {
    std::vector<std::unique_ptr<::autofill_assistant::input::TouchPoint>>
        touch_points;
    touch_points.emplace_back(
        input::TouchPoint::Builder().SetX(x).SetY(y).Build());
    devtools_client_->GetInput()->DispatchTouchEvent(
        input::DispatchTouchEventParams::Builder()
            .SetType(input::DispatchTouchEventType::TOUCH_START)
            .SetTouchPoints(std::move(touch_points))
            .Build(),
        node_frame_id_,
        base::BindOnce(&ClickOrTapWorker::OnDispatchTouchEventStart,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  NOTREACHED();
  std::move(callback_).Run(ClientStatus(INVALID_ACTION));
}

void ClickOrTapWorker::OnDispatchPressMouseEvent(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<input::DispatchMouseEventResult> result) {
  if (!result) {
    VLOG(1) << __func__
            << " Failed to dispatch mouse left button pressed event.";
    std::move(callback_).Run(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }

  int x = element_position_getter_->x();
  int y = element_position_getter_->y();

  devtools_client_->GetInput()->DispatchMouseEvent(
      input::DispatchMouseEventParams::Builder()
          .SetX(x)
          .SetY(y)
          .SetClickCount(1)
          .SetButton(input::MouseButton::LEFT)
          .SetType(input::DispatchMouseEventType::MOUSE_RELEASED)
          .Build(),
      node_frame_id_,
      base::BindOnce(&ClickOrTapWorker::OnDispatchReleaseMouseEvent,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ClickOrTapWorker::OnDispatchReleaseMouseEvent(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<input::DispatchMouseEventResult> result) {
  if (!result) {
    VLOG(1) << __func__ << " Failed to dispatch release mouse event.";
    std::move(callback_).Run(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }
  std::move(callback_).Run(OkClientStatus());
}

void ClickOrTapWorker::OnDispatchTouchEventStart(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<input::DispatchTouchEventResult> result) {
  if (!result) {
    VLOG(1) << __func__ << " Failed to dispatch touch start event.";
    std::move(callback_).Run(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }

  std::vector<std::unique_ptr<::autofill_assistant::input::TouchPoint>>
      touch_points;
  devtools_client_->GetInput()->DispatchTouchEvent(
      input::DispatchTouchEventParams::Builder()
          .SetType(input::DispatchTouchEventType::TOUCH_END)
          .SetTouchPoints(std::move(touch_points))
          .Build(),
      node_frame_id_,
      base::BindOnce(&ClickOrTapWorker::OnDispatchTouchEventEnd,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ClickOrTapWorker::OnDispatchTouchEventEnd(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<input::DispatchTouchEventResult> result) {
  if (!result) {
    VLOG(1) << __func__ << " Failed to dispatch touch end event.";
    std::move(callback_).Run(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }
  std::move(callback_).Run(OkClientStatus());
}

}  // namespace autofill_assistant
