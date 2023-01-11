// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/gesture_router.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chromecast/base/chromecast_switches.h"

namespace chromecast {

GestureRouter::GestureRouter()
    : gesture_source_receiver_(this),
      handler_(nullptr),
      can_go_back_(false),
      can_top_drag_(false),
      can_right_drag_(false),
      managed_mode_(GetSwitchValueBoolean(switches::kManagedMode, false)),
      weak_factory_(this) {}

GestureRouter::~GestureRouter() = default;

bool GestureRouter::CanHandleGesture(GestureType gesture_type) {
  switch (gesture_type) {
    case GestureType::GO_BACK:
      return CanGoBack();
    case GestureType::TAP:
      return true;
    case GestureType::TAP_DOWN:
      return true;
    case GestureType::TOP_DRAG:
      return CanTopDrag();
    case GestureType::RIGHT_DRAG:
      return CanRightDrag();
    default:
      return false;
  }
}

void GestureRouter::GestureProgress(GestureType gesture_type,
                                    const gfx::Point& touch_location) {
  switch (gesture_type) {
    case GestureType::GO_BACK:
      if (!CanGoBack())
        return;
      SendBackGestureProgress(touch_location);
      break;
    case GestureType::TOP_DRAG:
      if (!CanTopDrag())
        return;
      SendTopDragGestureProgress(touch_location);
      break;
    case GestureType::RIGHT_DRAG:
      if (!CanRightDrag())
        return;
      SendRightDragGestureProgress(touch_location);
      break;
    default:
      return;
  }
}

void GestureRouter::CancelGesture(GestureType gesture_type) {
  switch (gesture_type) {
    case GestureType::GO_BACK:
      if (!CanGoBack())
        return;
      SendBackGestureCancel();
      break;
    default:
      return;
  }
}

void GestureRouter::ConsumeGesture(GestureType gesture_type,
                                   GestureHandledCallback handled_callback) {
  switch (gesture_type) {
    case GestureType::NO_GESTURE:
      std::move(handled_callback).Run(false);
      break;
    case GestureType::GO_BACK:
      if (!CanGoBack()) {
        std::move(handled_callback).Run(false);
        return;
      }
      SendBackGesture(std::move(handled_callback));
      break;
    case GestureType::TAP:
      SendTapGesture();
      std::move(handled_callback).Run(true);
      break;
    case GestureType::TAP_DOWN:
      SendTapDownGesture();
      std::move(handled_callback).Run(true);
      break;
    case GestureType::TOP_DRAG:
      SendTopDragGestureDone();
      std::move(handled_callback).Run(true);
      break;
    case GestureType::RIGHT_DRAG:
      SendRightDragGestureDone();
      std::move(handled_callback).Run(true);
      break;
    default:
      std::move(handled_callback).Run(false);
  }
}

void GestureRouter::SetConsumeGestureCallback(ConsumerCallback callback) {
  consumer_callback_ = callback;
}

void GestureRouter::SetHandler(mojom::GestureHandler* handler) {
  handler_ = handler;
}

void GestureRouter::Subscribe(
    mojo::PendingRemote<mojom::GestureHandler> pending_handler_remote) {
  if (handler_remote_.is_bound()) {
    handler_remote_.reset();
  }
  handler_remote_.Bind(std::move(pending_handler_remote));
  SetHandler(handler_remote_.get());
}

void GestureRouter::SetCanGoBack(bool can_go_back) {
  can_go_back_ = can_go_back;
  if (delegate_)
    delegate_->SetCanGoBack(can_go_back_);
}

bool GestureRouter::CanGoBack() const {
  return can_go_back_ && (consumer_callback_ || handler_);
}

void GestureRouter::SendBackGesture(
    base::OnceCallback<void(bool)> was_handled_callback) {
  if (consumer_callback_) {
    consumer_callback_.Run(GestureType::GO_BACK,
                           std::move(was_handled_callback));
    return;
  }
  if (!handler_)
    return;
  handler_->OnBackGesture(std::move(was_handled_callback));
}

void GestureRouter::SendBackGestureProgress(const gfx::Point& touch_location) {
  if (!handler_)
    return;
  handler_->OnBackGestureProgress(touch_location);
}

void GestureRouter::SendBackGestureCancel() {
  if (!handler_)
    return;
  handler_->OnBackGestureCancel();
}

void GestureRouter::SetCanTopDrag(bool can_top_drag) {
  can_top_drag_ = can_top_drag;
}

bool GestureRouter::CanTopDrag() const {
  return handler_ && can_top_drag_ && !managed_mode_;
}

void GestureRouter::SendTopDragGestureDone() {
  if (!handler_)
    return;
  handler_->OnTopDragGestureDone();
}

void GestureRouter::SendTopDragGestureProgress(
    const gfx::Point& touch_location) {
  if (!handler_)
    return;
  handler_->OnTopDragGestureProgress(touch_location);
}

void GestureRouter::SetCanRightDrag(bool can_right_drag) {
  can_right_drag_ = can_right_drag;
}

bool GestureRouter::CanRightDrag() const {
  return handler_ && can_right_drag_;
}

void GestureRouter::SendRightDragGestureDone() {
  if (!handler_)
    return;
  handler_->OnRightDragGestureDone();
}

void GestureRouter::SendRightDragGestureProgress(
    const gfx::Point& touch_location) {
  if (!handler_)
    return;
  handler_->OnRightDragGestureProgress(touch_location);
}

void GestureRouter::SendTapGesture() {
  if (!handler_)
    return;
  handler_->OnTapGesture();
}

void GestureRouter::SendTapDownGesture() {
  if (!handler_)
    return;
  handler_->OnTapDownGesture();
}

void GestureRouter::SetBackGestureDelegate(Delegate* delegate) {
  delegate_ = delegate;
  if (delegate_)
    delegate_->SetCanGoBack(can_go_back_);
}

base::RepeatingCallback<void(mojo::PendingReceiver<mojom::GestureSource>)>
GestureRouter::GetBinder() {
  return base::BindRepeating(&GestureRouter::BindGestureSource,
                             weak_factory_.GetWeakPtr());
}

void GestureRouter::BindGestureSource(
    mojo::PendingReceiver<mojom::GestureSource> request) {
  if (gesture_source_receiver_.is_bound()) {
    LOG(WARNING) << "Gesture router is being re-bound.";
    gesture_source_receiver_.reset();
  }
  gesture_source_receiver_.Bind(std::move(request));
}

}  // namespace chromecast
