// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/api/toast_specification.h"

#include <memory>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/types/pass_key.h"
#include "ui/base/models/simple_menu_model.h"

ToastSpecification::Builder::Builder(const gfx::VectorIcon& icon,
                                     int body_string_id)
    : toast_specification_(
          std::make_unique<ToastSpecification>(base::PassKey<Builder>(),
                                               icon,
                                               body_string_id)) {}

ToastSpecification::Builder::~Builder() {
  // Verify that ToastSpecification::Builder::Build() has been called
  // so the toast specification is completely built.
  CHECK(!toast_specification_);
}

ToastSpecification::Builder& ToastSpecification::Builder::AddCloseButton() {
  toast_specification_->AddCloseButton();
  return *this;
}

ToastSpecification::Builder& ToastSpecification::Builder::AddActionButton(
    int action_button_string_id,
    base::RepeatingClosure closure) {
  toast_specification_->AddActionButton(action_button_string_id,
                                        std::move(closure));
  return *this;
}

ToastSpecification::Builder& ToastSpecification::Builder::AddMenu(
    std::unique_ptr<ui::SimpleMenuModel> menu_model) {
  toast_specification_->AddMenu(std::move(menu_model));
  return *this;
}

ToastSpecification::Builder& ToastSpecification::Builder::AddGlobalScoped() {
  toast_specification_->AddGlobalScope();
  return *this;
}

ToastSpecification::Builder& ToastSpecification::Builder::AddPersistance() {
  toast_specification_->AddPersistance();
  return *this;
}

std::unique_ptr<ToastSpecification> ToastSpecification::Builder::Build() {
  // Persistent toast is global scoped by default since it should only be
  // dismissed when explicitly told to do so.
  if (toast_specification_->is_persistent_toast()) {
    AddGlobalScoped();
  }

  ValidateSpecification();
  return std::move(toast_specification_);
}

void ToastSpecification::Builder::ValidateSpecification() {
  // Toasts with an action button must have a close button and not a menu.
  if (toast_specification_->action_button_string_id().has_value()) {
    CHECK(toast_specification_->has_close_button());
    CHECK(!toast_specification_->menu_model());
  }

  // Toasts with a menu can't have a close button. If this behavior is needed,
  // discuss with UX how to design this in a way that supports both.
  if (toast_specification_->menu_model()) {
    CHECK(!toast_specification_->has_close_button());
  }
}

ToastSpecification::ToastSpecification(
    base::PassKey<ToastSpecification::Builder>,
    const gfx::VectorIcon& icon,
    int string_id)
    : icon_(icon), body_string_id_(string_id) {}

ToastSpecification::~ToastSpecification() = default;

void ToastSpecification::AddCloseButton() {
  has_close_button_ = true;
}

void ToastSpecification::AddActionButton(int string_id,
                                         base::RepeatingClosure closure) {
  CHECK(!closure.is_null());
  action_button_string_id_ = string_id;
  action_button_closure_ = std::move(closure);
}

void ToastSpecification::AddMenu(
    std::unique_ptr<ui::SimpleMenuModel> menu_model) {
  menu_model_ = std::move(menu_model);
}

void ToastSpecification::AddGlobalScope() {
  is_global_scope_ = true;
}

void ToastSpecification::AddPersistance() {
  is_persistent_toast_ = true;
}
