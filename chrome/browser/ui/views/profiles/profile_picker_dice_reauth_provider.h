// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_DICE_REAUTH_PROVIDER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_DICE_REAUTH_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "components/signin/public/identity_manager/identity_manager.h"

struct CoreAccountInfo;
class ForceSigninVerifier;
class Profile;
class ProfilePickerWebContentsHost;

namespace content {
class WebContents;
}

// This object handles the reauth of the main account of a Profile.
// The flow starts with the call to `SwitchToReauth()` and goes as follow:
// - Extract the primary account for which the reauth will be attempted.
// - Wait for the refresh tokens to be loaded.
// - Call to the ForceSigninVerifier to check the main account token status.
// - If the token is valid, there is no need to reauth, and we finish with
// success.
// - If it is not, we show the reauth gaia page and await for user to reauth.
// - Get the account that has been reauthed through
// `OnRefreshTokenUpdatedForAccount()`.
// - We finish the flow by replying to the callback based on the success of the
// last step.
class ProfilePickerDiceReauthProvider
    : public signin::IdentityManager::Observer {
 public:
  explicit ProfilePickerDiceReauthProvider(
      ProfilePickerWebContentsHost* host,
      Profile* profile,
      const std::string& email_to_reauth,
      const std::string& gaia_id_to_reauth,
      base::OnceCallback<void(bool)> on_reauth_completed);
  ~ProfilePickerDiceReauthProvider() override;

  ProfilePickerDiceReauthProvider(const ProfilePickerDiceReauthProvider&) =
      delete;
  ProfilePickerDiceReauthProvider& operator=(
      const ProfilePickerDiceReauthProvider&) = delete;

  content::WebContents* contents() const { return contents_.get(); }

  // Start the reauth process.
  void SwitchToReauth();

  // signin::IdentityManager::Observer:
  void OnRefreshTokensLoaded() override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;

 private:
  // Attempt to create the ForceSigninVerifier, refresh tokens should be loaded.
  void TryCreateForceSigninVerifier();

  // Callback to the ForceSigninVerifier after fetching the tokens.
  void OnTokenFetchComplete(bool token_is_valid);

  // Display the reauth URL in `host_`.
  void ShowReauth();

  // Finish the reauth step on the Gaia side, and return to the caller
  // through the `on_reauth_completed_`.
  void Finish(bool success);

  const raw_ref<ProfilePickerWebContentsHost> host_;
  raw_ref<Profile> profile_;
  raw_ref<signin::IdentityManager> identity_manager_;
  const std::string gaia_id_to_reauth_;
  const std::string email_to_reauth_;
  base::OnceCallback<void(bool)> on_reauth_completed_;

  // Prevent `profile_` from being destroyed first.
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;

  // The web contents backed by `profile_`. This is used for displaying the
  // sign-in flow.
  std::unique_ptr<content::WebContents> contents_;
  std::unique_ptr<ForceSigninVerifier> force_signin_verifier_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_identity_manager_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_DICE_REAUTH_PROVIDER_H_
