// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/parent_access/fake_parent_access_dialog.h"

#include <utility>

namespace ash {

FakeParentAccessDialogProvider::Action
FakeParentAccessDialogProvider::Action::WithResult(
    std::unique_ptr<ParentAccessDialog::Result> result) {
  CHECK(result);
  Action action(Type::kWithResult);
  action.next_result_ = std::move(result);
  return action;
}

FakeParentAccessDialogProvider::Action
FakeParentAccessDialogProvider::Action::CaptureCallback(
    base::OnceCallback<void(ParentAccessDialog::Callback)> callback) {
  CHECK(callback);
  Action action(Type::kCaptureCallback);
  action.callback_ = std::move(callback);
  return action;
}

FakeParentAccessDialogProvider::Action
FakeParentAccessDialogProvider::Action::NotAChildUser() {
  return Action(Type::kNotAChildUser);
}

FakeParentAccessDialogProvider::Action
FakeParentAccessDialogProvider::Action::DialogAlreadyVisible() {
  return Action(Type::kDialogAlreadyVisible);
}

FakeParentAccessDialogProvider::Action::Action(Type type) : type_(type) {}
FakeParentAccessDialogProvider::Action::Action(Action&& other) = default;
FakeParentAccessDialogProvider::Action&
FakeParentAccessDialogProvider::Action::operator=(Action&& other) = default;
FakeParentAccessDialogProvider::Action::~Action() = default;

FakeParentAccessDialogProvider::FakeParentAccessDialogProvider() = default;
FakeParentAccessDialogProvider::~FakeParentAccessDialogProvider() = default;

ParentAccessDialogProvider::ShowError FakeParentAccessDialogProvider::Show(
    parent_access_ui::mojom::ParentAccessParamsPtr params,
    ParentAccessDialog::Callback callback) {
  CHECK(next_action_);

  last_params_ = std::move(params);
  auto action = std::move(next_action_).value();
  next_action_.reset();
  switch (action.type_) {
    case Action::Type::kWithResult:
      CHECK(action.next_result_);
      std::move(callback).Run(std::move(action.next_result_));
      return ShowError::kNone;
    case Action::Type::kCaptureCallback:
      CHECK(action.callback_);
      std::move(action.callback_).Run(std::move(callback));
      return ShowError::kNone;
    case Action::Type::kNotAChildUser:
      return ShowError::kNotAChildUser;
    case Action::Type::kDialogAlreadyVisible:
      return ShowError::kDialogAlreadyVisible;
  }
}

void FakeParentAccessDialogProvider::SetNextAction(Action action) {
  next_action_ = std::move(action);
}

parent_access_ui::mojom::ParentAccessParamsPtr
FakeParentAccessDialogProvider::TakeLastParams() {
  return std::move(last_params_);
}

}  // namespace ash
