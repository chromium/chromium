// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/account_manager/fake_account_manager_dialog.h"

#include <utility>

namespace ash::test {

FakeAccountManagerDialog::FakeAccountManagerDialog() = default;
FakeAccountManagerDialog::~FakeAccountManagerDialog() = default;

void FakeAccountManagerDialog::CloseDialog() {
  if (!close_dialog_closure_.is_null()) {
    std::move(close_dialog_closure_).Run();
  }
  is_dialog_shown_ = false;
}

void FakeAccountManagerDialog::ShowAddAccountDialog(
    const account_manager::AccountAdditionOptions& options,
    base::OnceClosure close_dialog_closure) {
  close_dialog_closure_ = std::move(close_dialog_closure);
  last_add_account_options_ = options;
  show_account_addition_dialog_calls_++;
  is_dialog_shown_ = true;
}

void FakeAccountManagerDialog::ShowReauthAccountDialog(
    const std::string& email,
    base::OnceClosure close_dialog_closure) {
  close_dialog_closure_ = std::move(close_dialog_closure);
  last_reauth_email_ = email;
  show_account_reauthentication_dialog_calls_++;
  is_dialog_shown_ = true;
}

bool FakeAccountManagerDialog::IsDialogShown() const {
  return is_dialog_shown_;
}

}  // namespace ash::test
