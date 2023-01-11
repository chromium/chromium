// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/chromeos/fake_account_manager_ui.h"

#include "base/functional/callback.h"

FakeAccountManagerUI::FakeAccountManagerUI() = default;
FakeAccountManagerUI::~FakeAccountManagerUI() = default;

void FakeAccountManagerUI::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeAccountManagerUI::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeAccountManagerUI::SetIsDialogShown(bool is_dialog_shown) {
  is_dialog_shown_ = is_dialog_shown;
}

void FakeAccountManagerUI::CloseDialog() {
  if (!close_dialog_closure_.is_null()) {
    std::move(close_dialog_closure_).Run();
  }
  is_dialog_shown_ = false;
}

void FakeAccountManagerUI::ShowAddAccountDialog(
    const account_manager::AccountAdditionOptions& options,
    base::OnceClosure close_dialog_closure) {
  close_dialog_closure_ = std::move(close_dialog_closure);
  show_account_addition_dialog_calls_++;
  is_dialog_shown_ = true;

  for (auto& obs : observers_)
    obs.OnAddAccountDialogShown();
}

void FakeAccountManagerUI::ShowReauthAccountDialog(
    const std::string& email,
    base::OnceClosure close_dialog_closure) {
  close_dialog_closure_ = std::move(close_dialog_closure);
  show_account_reauthentication_dialog_calls_++;
  is_dialog_shown_ = true;

  for (auto& obs : observers_)
    obs.OnReauthAccountDialogShown();
}

bool FakeAccountManagerUI::IsDialogShown() {
  return is_dialog_shown_;
}

void FakeAccountManagerUI::ShowManageAccountsSettings() {
  show_manage_accounts_settings_calls_++;

  for (auto& obs : observers_)
    obs.OnManageAccountsSettingsShown();
}
