// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_helper_chromeos.h"

#include "ash/components/account_manager/account_manager.h"
#include "ash/components/account_manager/account_manager_ash.h"
#include "components/account_manager_core/account.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

namespace chromeos {

SigninHelper::SigninHelper(
    ash::AccountManager* account_manager,
    crosapi::AccountManagerAsh* account_manager_ash,
    const base::RepeatingClosure& close_dialog_closure,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& gaia_id,
    const std::string& email,
    const std::string& auth_code,
    const std::string& signin_scoped_device_id)
    : account_manager_(account_manager),
      account_manager_ash_(account_manager_ash),
      close_dialog_closure_(close_dialog_closure),
      email_(email),
      url_loader_factory_(std::move(url_loader_factory)),
      gaia_auth_fetcher_(this, gaia::GaiaSource::kChrome, url_loader_factory_) {
  account_key_ =
      account_manager::AccountKey{gaia_id, account_manager::AccountType::kGaia};

  DCHECK(!signin_scoped_device_id.empty());
  gaia_auth_fetcher_.StartAuthCodeForOAuth2TokenExchangeWithDeviceId(
      auth_code, signin_scoped_device_id);
}

SigninHelper::~SigninHelper() = default;

void SigninHelper::OnClientOAuthSuccess(const ClientOAuthResult& result) {
  // Flow of control after this call:
  // |AccountManager::UpsertAccount| updates / inserts the account and calls
  // its |Observer|s, one of which is
  // |ProfileOAuth2TokenServiceDelegateChromeOS|.
  // |ProfileOAuth2TokenServiceDelegateChromeOS::OnTokenUpserted| seeds the
  // Gaia id and email id for this account in |AccountTrackerService| and
  // invokes |FireRefreshTokenAvailable|. This causes the account to propagate
  // throughout the Identity Service chain, including in
  // |AccountFetcherService|. |AccountFetcherService::OnRefreshTokenAvailable|
  // invokes |AccountTrackerService::StartTrackingAccount|, triggers a fetch
  // for the account information from Gaia and updates this information into
  // |AccountTrackerService|. At this point the account will be fully added to
  // the system.
  UpsertAccount(result.refresh_token);

  CloseDialogAndExit();
}

void SigninHelper::OnClientOAuthFailure(const GoogleServiceAuthError& error) {
  // TODO(sinhak): Display an error.

  // Notify `AccountManagerAsh` about account addition failure and send `error`.
  account_manager_ash_->OnAccountAdditionFinished(
      account_manager::AccountAdditionResult(
          account_manager::AccountAdditionResult::Status::kNetworkError,
          error));
  CloseDialogAndExit();
}

void SigninHelper::UpsertAccount(const std::string& refresh_token) {
  account_manager_->UpsertAccount(account_key_, email_, refresh_token);

  // Notify `AccountManagerAsh` about successful account addition and send
  // the account.
  account_manager_ash_->OnAccountAdditionFinished(
      account_manager::AccountAdditionResult(
          account_manager::AccountAdditionResult::Status::kSuccess,
          account_manager::Account{account_key_, email_}));
}

void SigninHelper::CloseDialogAndExit() {
  close_dialog_closure_.Run();
  Exit();
}

void SigninHelper::Exit() {
  base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

ash::AccountManager* SigninHelper::GetAccountManager() {
  return account_manager_;
}

std::string SigninHelper::GetEmail() {
  return email_;
}

scoped_refptr<network::SharedURLLoaderFactory>
SigninHelper::GetUrlLoaderFactory() {
  return url_loader_factory_;
}
}  // namespace chromeos
