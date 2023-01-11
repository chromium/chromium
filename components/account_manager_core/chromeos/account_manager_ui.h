// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_ACCOUNT_MANAGER_UI_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_ACCOUNT_MANAGER_UI_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "components/account_manager_core/account_addition_options.h"

namespace account_manager {

// This interface is used by `AccountManagerMojoService` to show system UI
// (system dialogs, OS Settings etc.)
class COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE) AccountManagerUI {
 public:
  AccountManagerUI();
  AccountManagerUI(const AccountManagerUI&) = delete;
  AccountManagerUI& operator=(const AccountManagerUI&) = delete;
  virtual ~AccountManagerUI();

  // Show system dialog for account addition.
  // `options` are parameters that define the dialog UI.
  // `close_dialog_closure` callback will be called when dialog is closed.
  virtual void ShowAddAccountDialog(const AccountAdditionOptions& options,
                                    base::OnceClosure close_dialog_closure) = 0;

  // Show system dialog for account reauthentication.
  // `email` is the email of account that will be reauthenticated.
  // `close_dialog_closure` callback will be called when dialog is closed.
  virtual void ShowReauthAccountDialog(
      const std::string& email,
      base::OnceClosure close_dialog_closure) = 0;

  virtual bool IsDialogShown() = 0;

  // Show OS Settings > Accounts.
  virtual void ShowManageAccountsSettings() = 0;
};

}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_ACCOUNT_MANAGER_UI_H_
