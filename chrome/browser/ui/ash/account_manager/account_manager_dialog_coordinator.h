// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_DIALOG_COORDINATOR_H_
#define CHROME_BROWSER_UI_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_DIALOG_COORDINATOR_H_

#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "components/account_manager_core/account_addition_options.h"
#include "components/account_manager_core/account_upsertion_result.h"
#include "components/keyed_service/core/keyed_service.h"

namespace account_manager {
enum class AccountAdditionSource : int;
}  // namespace account_manager

namespace ash {

// Opens the ChromeOS account dialogs ("add a Google account" and "re-sign in
// to an existing one") for whichever Ash feature asked, and reports the result
// back to it. Lives per profile as a KeyedService, and records the add source
// and the final result for metrics so callers don't each have to.
class AccountManagerDialogCoordinator : public KeyedService {
 public:
  using AccountUpsertionFinishedCallback =
      base::OnceCallback<void(const account_manager::AccountUpsertionResult&)>;
  using ShowAddAccountDialogCallbackForTesting = base::RepeatingCallback<void(
      const account_manager::AccountAdditionOptions& options,
      base::OnceClosure close_dialog_closure)>;
  using ShowReauthAccountDialogCallbackForTesting =
      base::RepeatingCallback<void(const std::string& email,
                                   base::OnceClosure close_dialog_closure)>;
  using IsDialogShownCallbackForTesting = base::RepeatingCallback<bool()>;

  AccountManagerDialogCoordinator();
  AccountManagerDialogCoordinator(const AccountManagerDialogCoordinator&) =
      delete;
  AccountManagerDialogCoordinator& operator=(
      const AccountManagerDialogCoordinator&) = delete;
  ~AccountManagerDialogCoordinator() override;

  // Opens the add-account dialog and runs |callback| with the outcome once the
  // flow ends (a real result, or kCancelledByUser if the user just closes it).
  // |source| is recorded for metrics. If a dialog is already open, |callback|
  // runs right away with kAlreadyInProgress.
  void ShowAddAccountDialog(account_manager::AccountAdditionSource source,
                            account_manager::AccountAdditionOptions options,
                            AccountUpsertionFinishedCallback callback);

  // Like ShowAddAccountDialog, but re-authenticates the existing account
  // |email| instead of adding a new one.
  void ShowReauthAccountDialog(account_manager::AccountAdditionSource source,
                               const std::string& email,
                               AccountUpsertionFinishedCallback callback);

  // Hands back the callback the inline-login flow calls when it finishes. Going
  // through the coordinator lets it report inline-login's own result rather
  // than the plain "dialog closed" one.
  //
  // TODO(crbug.com/365741912, crbug.com/365902693): Remove this temporary
  // callback once the dialog can report its own result when it closes.
  AccountUpsertionFinishedCallback
  CreateInlineLoginAccountUpsertionFinishedCallback();

  // Swaps the real dialog UI out for test callbacks. Keep the returned closure
  // alive for as long as the fakes should stay installed; running it (or
  // letting it go out of scope) restores the real UI.
  [[nodiscard]] base::ScopedClosureRunner InstallDialogCallbacksForTesting(
      ShowAddAccountDialogCallbackForTesting show_add_account_dialog_callback,
      ShowReauthAccountDialogCallbackForTesting
          show_reauth_account_dialog_callback,
      IsDialogShownCallbackForTesting is_dialog_shown_callback);

  // Sets a callback run each time a dialog flow finishes. AccountReconcilor
  // uses it to re-sync cookies once the dialog closes.
  void SetDialogFlowFinishedCallback(base::RepeatingClosure callback);

 private:
  void OnAccountUpsertionFinished(
      const account_manager::AccountUpsertionResult& result);
  void OnSigninDialogClosed();
  void FinishUpsertAccount(
      const account_manager::AccountUpsertionResult& result);

  bool IsDialogShown() const;

  // Undoes InstallDialogCallbacksForTesting; run via the closure it hands back.
  void ResetDialogCallbacksForTesting();

  AccountUpsertionFinishedCallback account_upsertion_callback_;
  base::RepeatingClosure dialog_flow_finished_callback_;
  bool account_signin_in_progress_ = false;

  ShowAddAccountDialogCallbackForTesting
      show_add_account_dialog_callback_for_testing_;
  ShowReauthAccountDialogCallbackForTesting
      show_reauth_account_dialog_callback_for_testing_;
  IsDialogShownCallbackForTesting is_dialog_shown_callback_for_testing_;

  base::WeakPtrFactory<AccountManagerDialogCoordinator> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_DIALOG_COORDINATOR_H_
