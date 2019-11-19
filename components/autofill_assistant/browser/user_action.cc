// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/user_action.h"

#include "base/bind.h"

namespace autofill_assistant {

namespace {

void CallIgnoringContext(base::OnceCallback<void()> callback,
                         std::unique_ptr<TriggerContext> context) {
  std::move(callback).Run();
}

// void Intercept(base::OnceCallback<void(UserAction::Callback, const
// TriggerContext&)> interceptor, UserAction::Callback original,
// std::unique_ptr<TriggerContext>) {
//   std::move(interceptor).Run(std::move(original), context);
// }

}  // namespace

UserAction::UserAction(UserAction&& other) = default;
UserAction::UserAction() = default;
UserAction::~UserAction() = default;
UserAction& UserAction::operator=(UserAction&& other) = default;

// Initializes user action from proto.
UserAction::UserAction(const UserActionProto& action)
    : chip_(action.chip()), direct_action_(action.direct_action()) {}

UserAction::UserAction(const ChipProto& chip_proto,
                       const DirectActionProto& direct_action_proto)
    : chip_(chip_proto), direct_action_(direct_action_proto) {}

void UserAction::SetCallback(base::OnceCallback<void()> callback) {
  callback_ = base::BindOnce(&CallIgnoringContext, std::move(callback));
}

void UserAction::AddInterceptor(
    base::OnceCallback<void(UserAction::Callback,
                            std::unique_ptr<TriggerContext>)> interceptor) {
  if (!callback_)
    return;

  callback_ = base::BindOnce(std::move(interceptor), std::move(callback_));
}

}  // namespace autofill_assistant
