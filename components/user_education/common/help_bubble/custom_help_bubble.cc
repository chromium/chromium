// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/help_bubble/custom_help_bubble.h"

#include "base/logging.h"

namespace user_education {

CustomHelpBubbleUi::CustomHelpBubbleUi() = default;
CustomHelpBubbleUi::~CustomHelpBubbleUi() = default;

base::CallbackListSubscription CustomHelpBubbleUi::AddUserActionCallback(
    UserActionCallback callback) {
  return user_action_callbacks_.Add(std::move(callback));
}

void CustomHelpBubbleUi::NotifyUserAction(UserAction user_action) {
  user_action_callbacks_.Notify(user_action);
}

base::WeakPtr<CustomHelpBubbleUi> CustomHelpBubbleUi::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

CustomHelpBubble::CustomHelpBubble(CustomHelpBubbleUi& bubble)
    : bubble_(bubble.AsWeakPtr()) {}
CustomHelpBubble::~CustomHelpBubble() = default;

}  // namespace user_education
