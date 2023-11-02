// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/profile_picker_lacros_sign_in_provider.h"

#include "base/check.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"

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

void ProfilePickerLacrosSignInProvider::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
      signin::PrimaryAccountChangeEvent::Type::kSet) {
    return;
  }

  identity_manager_observation_.Reset();
  OnProfileSignedIn();
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

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);

  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    OnProfileSignedIn();
    return;
  }

  // Listen for sign-in getting completed.
  profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
      profile_, ProfileKeepAliveOrigin::kProfileCreationFlow);
  identity_manager_observation_.Observe(identity_manager);
}

void ProfilePickerLacrosSignInProvider::OnProfileSignedIn() {
  std::move(callback_).Run(profile_.get());
  // The object gets deleted now.
}
