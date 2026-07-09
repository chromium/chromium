// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ACCOUNT_MANAGER_FAKE_ACCOUNT_MANAGER_DIALOG_H_
#define CHROME_BROWSER_UI_ASH_ACCOUNT_MANAGER_FAKE_ACCOUNT_MANAGER_DIALOG_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "components/account_manager_core/account_addition_options.h"

namespace ash::test {

// A stand-in for the real Account Manager dialog UI, with no Profile of its
// own. That's the point: AccountManagerDialogCoordinator's unit tests build the
// coordinator directly (no Profile, no KeyedService factory) and drive it
// through one of these. Browser tests use ScopedFakeAccountManagerDialog, which
// wraps this and does have a profile.
class FakeAccountManagerDialog {
 public:
  FakeAccountManagerDialog();
  FakeAccountManagerDialog(const FakeAccountManagerDialog&) = delete;
  FakeAccountManagerDialog& operator=(const FakeAccountManagerDialog&) = delete;
  ~FakeAccountManagerDialog();

  // Runs the close closure from the most recent Show*Dialog call, i.e. pretends
  // the dialog finished. This is how a test drives the coordinator's flow to
  // its end.
  void CloseDialog();

  int show_account_addition_dialog_calls() const {
    return show_account_addition_dialog_calls_;
  }

  const std::optional<account_manager::AccountAdditionOptions>&
  last_add_account_options() const {
    return last_add_account_options_;
  }

  int show_account_reauthentication_dialog_calls() const {
    return show_account_reauthentication_dialog_calls_;
  }

  const std::optional<std::string>& last_reauth_email() const {
    return last_reauth_email_;
  }

  // The three hooks below stand in for the real dialog UI. Bind them into
  // AccountManagerDialogCoordinator::InstallDialogCallbacksForTesting. They
  // just record the call and stash the arguments for the accessors above.
  void ShowAddAccountDialog(
      const account_manager::AccountAdditionOptions& options,
      base::OnceClosure close_dialog_closure);
  void ShowReauthAccountDialog(const std::string& email,
                               base::OnceClosure close_dialog_closure);
  bool IsDialogShown() const;

 private:
  base::OnceClosure close_dialog_closure_;
  bool is_dialog_shown_ = false;
  int show_account_addition_dialog_calls_ = 0;
  int show_account_reauthentication_dialog_calls_ = 0;
  std::optional<account_manager::AccountAdditionOptions>
      last_add_account_options_;
  std::optional<std::string> last_reauth_email_;
};

}  // namespace ash::test

#endif  // CHROME_BROWSER_UI_ASH_ACCOUNT_MANAGER_FAKE_ACCOUNT_MANAGER_DIALOG_H_
