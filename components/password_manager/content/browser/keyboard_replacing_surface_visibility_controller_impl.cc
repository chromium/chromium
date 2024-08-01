// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/keyboard_replacing_surface_visibility_controller_impl.h"

#include "components/password_manager/content/browser/content_password_manager_driver.h"
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
    base::WeakPtr<password_manager::ContentPasswordManagerDriver>
        frame_driver) {
  if (IsVisible()) {
    return;
  }
  if (base::FeatureList::IsEnabled(
          features::kPasswordSuggestionBottomSheetV2)) {
    frame_driver_ = std::move(frame_driver);
    suppress_callback_ = base::BindRepeating(
        [](base::WeakPtr<KeyboardReplacingSurfaceVisibilityController>
               controller) { return controller->IsVisible(); },
        AsWeakPtr());
    frame_driver_->render_frame_host()
        ->GetRenderWidgetHost()
        ->AddSuppressShowingImeCallback(suppress_callback_);
  }
  state_ = State::kVisible;
}

void KeyboardReplacingSurfaceVisibilityControllerImpl::SetShown() {
  state_ = State::kShownBefore;
}

void KeyboardReplacingSurfaceVisibilityControllerImpl::Reset() {
  state_ = State::kNotShownYet;
  if (!suppress_callback_.is_null() && frame_driver_) {
    frame_driver_->render_frame_host()
        ->GetRenderWidgetHost()
        ->RemoveSuppressShowingImeCallback(suppress_callback_,
                                           /*trigger_ime=*/false);
  }
  suppress_callback_.Reset();
}

base::WeakPtr<KeyboardReplacingSurfaceVisibilityController>
KeyboardReplacingSurfaceVisibilityControllerImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace password_manager
