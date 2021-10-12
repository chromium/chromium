// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_window.h"

namespace chromecast {

CastContentWindow::CastContentWindow(base::WeakPtr<Delegate> delegate,
                                     mojom::CastWebViewParamsPtr params)
    : delegate_(delegate), params_(std::move(params)) {
  RegisterBackGestureRouter(gesture_router());
}

CastContentWindow::~CastContentWindow() = default;

void CastContentWindow::SetCastWebContents(CastWebContents* cast_web_contents) {
  cast_web_contents_ = cast_web_contents;
  // Must provide binder callbacks with WeakPtr since CastContentWindow + these
  // interface implementations are destroyed before CastWebContents.
  cast_web_contents_->local_interfaces()->AddBinder(base::BindRepeating(
      &CastContentWindow::BindReceiver, weak_factory_.GetWeakPtr()));
  cast_web_contents_->local_interfaces()->AddBinder(base::BindRepeating(
      &CastContentWindow::BindActivityWindow, weak_factory_.GetWeakPtr()));
  cast_web_contents_->local_interfaces()->AddBinder(
      gesture_router()->GetBinder());
}

void CastContentWindow::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void CastContentWindow::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void CastContentWindow::BindReceiver(
    mojo::PendingReceiver<mojom::CastContentWindow> receiver) {
  receiver_.Bind(std::move(receiver));
}

void CastContentWindow::BindActivityWindow(
    mojo::PendingReceiver<mojom::ActivityWindow> receiver) {
  activity_window_receiver_.Bind(std::move(receiver));
}

void CastContentWindow::Show() {
  RequestVisibility(VisibilityPriority::STICKY_ACTIVITY);
}

void CastContentWindow::Hide() {
  RequestMoveOut();
}

mojom::MediaControlUi* CastContentWindow::media_controls() {
  return nullptr;
}

}  // namespace chromecast
