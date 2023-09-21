// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_dice_reauth_provider.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/force_signin_verifier.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"

ProfilePickerDiceReauthProvider::ProfilePickerDiceReauthProvider(
    ProfilePickerWebContentsHost* host,
    Profile* profile,
    const std::string& gaia_id_to_reauth,
    const std::string& email_to_reauth,
    base::OnceCallback<void(bool)> on_reauth_completed)
    : host_(*host),
      profile_(*profile),
      identity_manager_(*IdentityManagerFactory::GetForProfile(profile)),
      gaia_id_to_reauth_(gaia_id_to_reauth),
      email_to_reauth_(email_to_reauth),
      on_reauth_completed_(std::move(on_reauth_completed)) {
  DCHECK(!gaia_id_to_reauth_.empty());
  DCHECK(!email_to_reauth_.empty());
}

ProfilePickerDiceReauthProvider::~ProfilePickerDiceReauthProvider() = default;

void ProfilePickerDiceReauthProvider::SwitchToReauth() {
  CHECK(!contents_);

  profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
      &*profile_, ProfileKeepAliveOrigin::kProfileCreationFlow);
  scoped_identity_manager_observation_.Observe(&*identity_manager_);

  // TODO(https://crbug.com/1478217): Add a loading screen + timer in order not
  // to potentially hang.

  // Attempt to create the `force_signin_verifier_` here, otherwise it will be
  // done after the refresh tokens are loaded in `OnRefreshTokensLoaded()`.
  // This is the first step to the reauth flow.
  TryCreateForceSigninVerifier();
}

void ProfilePickerDiceReauthProvider::OnRefreshTokensLoaded() {
  // If the verifier was not created before, we should create it now after the
  // refresh tokens were properly loaded.
  TryCreateForceSigninVerifier();
}

void ProfilePickerDiceReauthProvider::TryCreateForceSigninVerifier() {
  if (!force_signin_verifier_ && identity_manager_->AreRefreshTokensLoaded()) {
    force_signin_verifier_ = std::make_unique<ForceSigninVerifier>(
        &*profile_, &*identity_manager_,
        base::BindOnce(&ProfilePickerDiceReauthProvider::OnTokenFetchComplete,
                       base::Unretained(this)));
  }
}

void ProfilePickerDiceReauthProvider::OnTokenFetchComplete(
    bool token_is_valid) {
  // If the token is valid, we do not need to reauth and proceed to finish
  // with success directly.
  if (token_is_valid) {
    Finish(true);
    return;
  }

  ShowReauth();
}

void ProfilePickerDiceReauthProvider::ShowReauth() {
  CHECK(!contents_);

  contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(&*profile_));

  // Show the back button, the reactions are handled by the host itself.
  host_->ShowScreen(
      contents_.get(), signin::GetChromeReauthURL((email_to_reauth_)),
      base::BindOnce(&ProfilePickerWebContentsHost::SetNativeToolbarVisible,
                     // Unretained is enough as the callback is called by the
                     // host itself.
                     base::Unretained(host_), /*visible=*/true));
}

void ProfilePickerDiceReauthProvider::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (!identity_manager_->AreRefreshTokensLoaded() || !force_signin_verifier_) {
    return;
  }

  // TODO(https://crbug.com/1478217): Handle the case where a user chooses an
  // already existing signed in account; the `OnRefreshTokenUpdatedForAccount()`
  // will not be called.

  // Store the account name that is being reauthed (since the account name can
  // be changed, we need this information to compare it later with the original
  // account).
  bool success = gaia_id_to_reauth_ == account_info.gaia;

  // If the email reauth-ed is not the same as the intended email, we do not
  // want the user to proceed with success. Since at this point this would be a
  // new sign in, the account should be signed out.
  if (!success) {
    identity_manager_->GetAccountsMutator()->RemoveAccount(
        account_info.account_id,
        signin_metrics::SourceForRefreshTokenOperation::
            kForceSigninReauthWithDifferentAccount);
  }

  Finish(success);
}

void ProfilePickerDiceReauthProvider::Finish(bool success) {
  scoped_identity_manager_observation_.Reset();
  // Hide the toolbar in case it was visible after showing the reauth page.
  host_->SetNativeToolbarVisible(false);

  std::move(on_reauth_completed_).Run(success);
}
