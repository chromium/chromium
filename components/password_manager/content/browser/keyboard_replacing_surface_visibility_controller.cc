// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/keyboard_replacing_surface_visibility_controller.h"

namespace password_manager {

using State = KeyboardReplacingSurfaceVisibilityController::State;

KeyboardReplacingSurfaceVisibilityController::
    KeyboardReplacingSurfaceVisibilityController() = default;
KeyboardReplacingSurfaceVisibilityController::
    ~KeyboardReplacingSurfaceVisibilityController() = default;

bool KeyboardReplacingSurfaceVisibilityController::CanBeShown() const {
  return state_ == State::kNotShownYet;
}

bool KeyboardReplacingSurfaceVisibilityController::IsVisible() const {
  return state_ == State::kVisible;
}

void KeyboardReplacingSurfaceVisibilityController::SetVisible(
    raw_ptr<content::RenderWidgetHost> widget_host) {
  if (IsVisible()) {
    return;
  }
  widget_host_ = widget_host;
  suppress_callback_ = base::BindRepeating(
      [](base::WeakPtr<KeyboardReplacingSurfaceVisibilityController>
             controller) { return controller->IsVisible(); },
      AsWeakPtr());
  widget_host_->AddSuppressShowingImeCallback(suppress_callback_);
  state_ = State::kVisible;
}

void KeyboardReplacingSurfaceVisibilityController::SetShown() {
  state_ = State::kShownBefore;
}

void KeyboardReplacingSurfaceVisibilityController::Reset() {
  state_ = State::kNotShownYet;
  if (!suppress_callback_.is_null()) {
    widget_host_->RemoveSuppressShowingImeCallback(suppress_callback_);
    suppress_callback_.Reset();
  }
}

}  // namespace password_manager
