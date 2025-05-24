// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/help_bubble/custom_help_bubble.h"

#include "base/logging.h"

namespace user_education {

CustomHelpBubbleUi::CustomHelpBubbleUi()
    : user_action_callbacks_(
          std::make_unique<base::OnceCallbackList<void(UserAction)>>()) {}
CustomHelpBubbleUi::~CustomHelpBubbleUi() = default;

base::CallbackListSubscription CustomHelpBubbleUi::AddUserActionCallback(
    UserActionCallback callback) {
  CHECK(user_action_callbacks_) << "Cannot observe after action sent.";
  return user_action_callbacks_->Add(std::move(callback));
}

void CustomHelpBubbleUi::NotifyUserAction(UserAction user_action) {
  if (!user_action_callbacks_) {
    // An action has already been sent.
    return;
  }
  // Need to move to a local because `this` might be deleted during callbacks.
  std::unique_ptr<base::OnceCallbackList<void(UserAction)>> temp =
      std::move(user_action_callbacks_);
  temp->Notify(user_action);
}

base::WeakPtr<CustomHelpBubbleUi> CustomHelpBubbleUi::GetCustomUiAsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

CustomHelpBubble::CustomHelpBubble(CustomHelpBubbleUi& bubble)
    : bubble_(bubble.GetCustomUiAsWeakPtr()) {}
CustomHelpBubble::~CustomHelpBubble() = default;

}  // namespace user_education
