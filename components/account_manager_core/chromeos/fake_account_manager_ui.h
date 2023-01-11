// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_FAKE_ACCOUNT_MANAGER_UI_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_FAKE_ACCOUNT_MANAGER_UI_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/account_manager_core/chromeos/account_manager_ui.h"

// Fake implementation of `AccountManagerUI` for tests.
class FakeAccountManagerUI : public account_manager::AccountManagerUI {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnAddAccountDialogShown() {}
    virtual void OnReauthAccountDialogShown() {}
    virtual void OnManageAccountsSettingsShown() {}
  };

  FakeAccountManagerUI();
  FakeAccountManagerUI(const FakeAccountManagerUI&) = delete;
  FakeAccountManagerUI& operator=(const FakeAccountManagerUI&) = delete;
  ~FakeAccountManagerUI() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void SetIsDialogShown(bool is_dialog_shown);
  void CloseDialog();

  int show_account_addition_dialog_calls() const {
    return show_account_addition_dialog_calls_;
  }

  int show_account_reauthentication_dialog_calls() const {
    return show_account_reauthentication_dialog_calls_;
  }

  int show_manage_accounts_settings_calls() const {
    return show_manage_accounts_settings_calls_;
  }

  // AccountManagerUI overrides:
  void ShowAddAccountDialog(
      const account_manager::AccountAdditionOptions& options,
      base::OnceClosure close_dialog_closure) override;
  void ShowReauthAccountDialog(const std::string& email,
                               base::OnceClosure close_dialog_closure) override;
  bool IsDialogShown() override;
  void ShowManageAccountsSettings() override;

 private:
  base::OnceClosure close_dialog_closure_;
  bool is_dialog_shown_ = false;
  int show_account_addition_dialog_calls_ = 0;
  int show_account_reauthentication_dialog_calls_ = 0;
  int show_manage_accounts_settings_calls_ = 0;

  base::ObserverList<Observer> observers_;
};

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_FAKE_ACCOUNT_MANAGER_UI_H_
