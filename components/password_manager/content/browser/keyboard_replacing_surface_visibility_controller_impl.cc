// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/keyboard_replacing_surface_visibility_controller_impl.h"

#include "components/password_manager/core/common/password_manager_features.h"

namespace password_manager {

using State = KeyboardReplacingSurfaceVisibilityController::State;

KeyboardReplacingSurfaceVisibilityControllerImpl::
    KeyboardReplacingSurfaceVisibilityControllerImpl() = default;
KeyboardReplacingSurfaceVisibilityControllerImpl::
    ~KeyboardReplacingSurfaceVisibilityControllerImpl() = default;

bool KeyboardReplacingSurfaceVisibilityControllerImpl::CanBeShown() const {
  return state_ == State::kNotShownYet;
}

bool KeyboardReplacingSurfaceVisibilityControllerImpl::IsVisible() const {
  return state_ == State::kVisible;
}

void KeyboardReplacingSurfaceVisibilityControllerImpl::SetVisible(
    raw_ptr<content::RenderWidgetHost> widget_host) {
  if (IsVisible()) {
    return;
  }
  if (base::FeatureList::IsEnabled(
          features::kPasswordSuggestionBottomSheetV2)) {
    widget_host_ = widget_host;
    suppress_callback_ = base::BindRepeating(
        [](base::WeakPtr<KeyboardReplacingSurfaceVisibilityController>
               controller) { return controller->IsVisible(); },
        AsWeakPtr());
    widget_host_->AddSuppressShowingImeCallback(suppress_callback_);
  }
  state_ = State::kVisible;
}

void KeyboardReplacingSurfaceVisibilityControllerImpl::SetShown() {
  state_ = State::kShownBefore;
}

void KeyboardReplacingSurfaceVisibilityControllerImpl::Reset() {
  state_ = State::kNotShownYet;
  if (!suppress_callback_.is_null()) {
    widget_host_->RemoveSuppressShowingImeCallback(suppress_callback_);
    suppress_callback_.Reset();
  }
}

}  // namespace password_manager
