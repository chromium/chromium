// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/profile_picker_lacros_sign_in_provider.h"

#include "base/check.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"

ProfilePickerLacrosSignInProvider::ProfilePickerLacrosSignInProvider() =
    default;

ProfilePickerLacrosSignInProvider::~ProfilePickerLacrosSignInProvider() =
    default;

void ProfilePickerLacrosSignInProvider::
    ShowAddAccountDialogAndCreateSignedInProfile(SignedInCallback callback) {
  DCHECK(callback);
  DCHECK(!callback_);
  callback_ = std::move(callback);

  g_browser_process->profile_manager()
      ->GetAccountProfileMapper()
      ->ShowAddAccountDialogAndCreateNewProfile(
          account_manager::AccountManagerFacade::AccountAdditionSource::
              kChromeProfileCreation,
          base::BindOnce(
              &ProfilePickerLacrosSignInProvider::OnLacrosProfileCreated,
              weak_ptr_factory_.GetWeakPtr()));
}

void ProfilePickerLacrosSignInProvider::
    CreateSignedInProfileWithExistingAccount(const std::string& gaia_id,
                                             SignedInCallback callback) {
  DCHECK(callback);
  DCHECK(!callback_);
  DCHECK(!gaia_id.empty());
  callback_ = std::move(callback);

  g_browser_process->profile_manager()
      ->GetAccountProfileMapper()
      ->CreateNewProfileWithAccount(
          account_manager::AccountKey(gaia_id,
                                      account_manager::AccountType::kGaia),
          base::BindOnce(
              &ProfilePickerLacrosSignInProvider::OnLacrosProfileCreated,
              weak_ptr_factory_.GetWeakPtr()));
}

void ProfilePickerLacrosSignInProvider::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  identity_manager_observation_.Reset();
  OnLacrosAccountLoaded(account_info);
}

void ProfilePickerLacrosSignInProvider::OnLacrosProfileCreated(
    const absl::optional<AccountProfileMapper::AddAccountResult>& result) {
  if (!result || result->profile_path.empty()) {
    // Sign-in or profile creation failed.
    std::move(callback_).Run(nullptr);
    // The object gets deleted now.
    return;
  }

  Profile* profile = g_browser_process->profile_manager()->GetProfileByPath(
      result->profile_path);
  DCHECK(profile);
  profile_ = profile;
  profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
      profile_, ProfileKeepAliveOrigin::kProfileCreationFlow);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  auto accounts = identity_manager->GetAccountsWithRefreshTokens();
  if (!accounts.empty()) {
    DCHECK_EQ(accounts.size(), 1u);
    OnLacrosAccountLoaded(accounts[0]);
    return;
  }

  // Listen for sign-in getting completed.
  identity_manager_observation_.Observe(identity_manager);
}

void ProfilePickerLacrosSignInProvider::OnLacrosAccountLoaded(
    const CoreAccountInfo& account) {
  DCHECK(!account.IsEmpty());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  identity_manager->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account.account_id, signin::ConsentLevel::kSignin);

  std::move(callback_).Run(profile_);
  // The object gets deleted now.
}
