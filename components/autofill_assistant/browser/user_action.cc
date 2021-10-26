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

}  // namespace

UserAction::UserAction(UserAction&& other) = default;
UserAction::UserAction() = default;
UserAction::~UserAction() = default;
UserAction& UserAction::operator=(UserAction&& other) = default;

// Initializes user action from proto.
UserAction::UserAction(const UserActionProto& action)
    : chip_(action.chip()),
      enabled_(action.enabled()),
      identifier_(action.identifier()) {}

UserAction::UserAction(const ChipProto& chip_proto,
                       bool enabled,
                       const std::string& identifier)
    : chip_(chip_proto), enabled_(enabled), identifier_(identifier) {}

void UserAction::SetCallback(base::OnceCallback<void()> callback) {
  callback_ = base::BindOnce(&CallIgnoringContext, std::move(callback));
}

}  // namespace autofill_assistant
