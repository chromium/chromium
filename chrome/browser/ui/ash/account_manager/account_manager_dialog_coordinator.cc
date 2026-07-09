// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/account_manager/account_manager_dialog_coordinator.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/webui/signin/ash/inline_login_dialog.h"
#include "components/account_manager_core/account_addition_options.h"
#include "components/account_manager_core/account_manager_metrics.h"
#include "components/account_manager_core/account_upsertion_result.h"

namespace ash {

namespace {

const account_manager::AccountUpsertionResult&
RecordAccountUpsertionResultStatus(
    const account_manager::AccountUpsertionResult& result) {
  account_manager::RecordAccountUpsertionResultStatus(result.status());
  return result;
}

}  // namespace

AccountManagerDialogCoordinator::AccountManagerDialogCoordinator() = default;

AccountManagerDialogCoordinator::~AccountManagerDialogCoordinator() = default;

void AccountManagerDialogCoordinator::ShowAddAccountDialog(
    account_manager::AccountAdditionSource source,
    account_manager::AccountAdditionOptions options,
    AccountUpsertionFinishedCallback callback) {
  account_manager::RecordAccountAdditionSource(source);
  callback = base::BindOnce(&RecordAccountUpsertionResultStatus)
                 .Then(std::move(callback));

  if (IsDialogShown()) {
    std::move(callback).Run(account_manager::AccountUpsertionResult::FromStatus(
        account_manager::AccountUpsertionResult::Status::kAlreadyInProgress));
    return;
  }

  CHECK(!account_signin_in_progress_);
  account_signin_in_progress_ = true;
  account_upsertion_callback_ = std::move(callback);
  auto close_dialog_closure =
      base::BindOnce(&AccountManagerDialogCoordinator::OnSigninDialogClosed,
                     weak_ptr_factory_.GetWeakPtr());
  if (!show_add_account_dialog_callback_for_testing_.is_null()) {
    CHECK_IS_TEST();
    show_add_account_dialog_callback_for_testing_.Run(
        options, std::move(close_dialog_closure));
    return;
  }
  InlineLoginDialog::Show(options, std::move(close_dialog_closure));
}

void AccountManagerDialogCoordinator::ShowReauthAccountDialog(
    account_manager::AccountAdditionSource source,
    const std::string& email,
    AccountUpsertionFinishedCallback callback) {
  account_manager::RecordAccountAdditionSource(source);
  callback = base::BindOnce(&RecordAccountUpsertionResultStatus)
                 .Then(std::move(callback));

  if (IsDialogShown()) {
    std::move(callback).Run(account_manager::AccountUpsertionResult::FromStatus(
        account_manager::AccountUpsertionResult::Status::kAlreadyInProgress));
    return;
  }

  CHECK(!account_signin_in_progress_);
  account_signin_in_progress_ = true;
  account_upsertion_callback_ = std::move(callback);
  auto close_dialog_closure =
      base::BindOnce(&AccountManagerDialogCoordinator::OnSigninDialogClosed,
                     weak_ptr_factory_.GetWeakPtr());
  if (!show_reauth_account_dialog_callback_for_testing_.is_null()) {
    CHECK_IS_TEST();
    show_reauth_account_dialog_callback_for_testing_.Run(
        email, std::move(close_dialog_closure));
    return;
  }
  InlineLoginDialog::Show(email, std::move(close_dialog_closure));
}

AccountManagerDialogCoordinator::AccountUpsertionFinishedCallback
AccountManagerDialogCoordinator::
    CreateInlineLoginAccountUpsertionFinishedCallback() {
  return base::BindOnce(
      &AccountManagerDialogCoordinator::OnAccountUpsertionFinished,
      weak_ptr_factory_.GetWeakPtr());
}

base::ScopedClosureRunner
AccountManagerDialogCoordinator::InstallDialogCallbacksForTesting(
    ShowAddAccountDialogCallbackForTesting show_add_account_dialog_callback,
    ShowReauthAccountDialogCallbackForTesting
        show_reauth_account_dialog_callback,
    IsDialogShownCallbackForTesting is_dialog_shown_callback) {
  show_add_account_dialog_callback_for_testing_ =
      std::move(show_add_account_dialog_callback);
  show_reauth_account_dialog_callback_for_testing_ =
      std::move(show_reauth_account_dialog_callback);
  is_dialog_shown_callback_for_testing_ = std::move(is_dialog_shown_callback);
  return base::ScopedClosureRunner(base::BindOnce(
      &AccountManagerDialogCoordinator::ResetDialogCallbacksForTesting,
      weak_ptr_factory_.GetWeakPtr()));
}

void AccountManagerDialogCoordinator::ResetDialogCallbacksForTesting() {
  show_add_account_dialog_callback_for_testing_.Reset();
  show_reauth_account_dialog_callback_for_testing_.Reset();
  is_dialog_shown_callback_for_testing_.Reset();
}

void AccountManagerDialogCoordinator::SetDialogFlowFinishedCallback(
    base::RepeatingClosure callback) {
  dialog_flow_finished_callback_ = std::move(callback);
}

void AccountManagerDialogCoordinator::OnAccountUpsertionFinished(
    const account_manager::AccountUpsertionResult& result) {
  if (!account_signin_in_progress_) {
    return;
  }

  FinishUpsertAccount(result);
}

void AccountManagerDialogCoordinator::OnSigninDialogClosed() {
  if (!account_signin_in_progress_) {
    return;
  }

  // Account addition is still in progress. It means that user didn't complete
  // the account addition flow and closed the dialog.
  FinishUpsertAccount(account_manager::AccountUpsertionResult::FromStatus(
      account_manager::AccountUpsertionResult::Status::kCancelledByUser));
}

void AccountManagerDialogCoordinator::FinishUpsertAccount(
    const account_manager::AccountUpsertionResult& result) {
  CHECK(account_signin_in_progress_);
  CHECK(account_upsertion_callback_);

  account_signin_in_progress_ = false;
  std::move(account_upsertion_callback_).Run(result);
  if (!dialog_flow_finished_callback_.is_null()) {
    dialog_flow_finished_callback_.Run();
  }
}

bool AccountManagerDialogCoordinator::IsDialogShown() const {
  if (!is_dialog_shown_callback_for_testing_.is_null()) {
    CHECK_IS_TEST();
    return is_dialog_shown_callback_for_testing_.Run();
  }
  return InlineLoginDialog::IsShown();
}

}  // namespace ash
