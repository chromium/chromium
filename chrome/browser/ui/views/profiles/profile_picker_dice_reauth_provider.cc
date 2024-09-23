// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_dice_reauth_provider.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/dice_response_handler.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/force_signin_verifier.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_urls.h"

namespace {

void RecordReauthResult(ProfilePickerReauthResult result) {
  base::UmaHistogramEnumeration("ProfilePicker.ReauthResult", result);
}

GURL GetLoadingScreenURL() {
  GURL url = GURL(chrome::kChromeUISyncConfirmationURL);
  return url.Resolve(chrome::kChromeUISyncConfirmationLoadingPath);
}

GURL GetReauthURL(const std::string& email_to_reauth, GURL continue_url) {
  // By default `kForceSigninReauthInProfilePickerUseAddSession` is false.
  // This param will only be used as a fallback in case /AccountChooser (result
  // of `signin::GetChromeReauthURL()`) does not always return a valid refresh
  // token, which would cause the reauth to hang. As of now, extensive manual
  // testing did not show any regression with this usage.
  // /AddSession (result of `signin::GetAddAccountURLForDice()`) guarantees a
  // refresh token, however it's UI is less accurate for a reauth.
  return kForceSigninReauthInProfilePickerUseAddSession.Get()
             // /AddSession (fallback)
             ? signin::GetAddAccountURLForDice(email_to_reauth, continue_url)
             // /AccountChooser (default)
             : signin::GetChromeReauthURL(
                   {.email = email_to_reauth, .continue_url = continue_url});
}

ForceSigninUIError ComputeReauthUIError(ProfilePickerReauthResult result,
                                        const std::string& reauth_email) {
  switch (result) {
    case ProfilePickerReauthResult::kSuccess:
    case ProfilePickerReauthResult::kSuccessTokenAlreadyValid:
      return ForceSigninUIError::ErrorNone();
    case ProfilePickerReauthResult::kErrorUsedNewEmail:
    case ProfilePickerReauthResult::kErrorUsedOtherSignedInEmail:
      return ForceSigninUIError::ReauthWrongAccount(reauth_email);
    case ProfilePickerReauthResult::kTimeoutForceSigninVerifierCheck:
    case ProfilePickerReauthResult::kTimeoutSigninError:
      return ForceSigninUIError::ReauthTimeout();
  }
}

}  // namespace

ProfilePickerDiceReauthProvider::ProfilePickerDiceReauthProvider(
    ProfilePickerWebContentsHost* host,
    Profile* profile,
    const std::string& gaia_id_to_reauth,
    const std::string& email_to_reauth,
    base::OnceCallback<void(bool, const ForceSigninUIError&)>
        on_reauth_completed)
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

  contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(&*profile_));
  host_->ShowScreen(contents_.get(), GetLoadingScreenURL());

  timer_.Start(
      FROM_HERE, base::Seconds(kDiceTokenFetchTimeoutSeconds),
      base::BindOnce(
          &ProfilePickerDiceReauthProvider::OnForceSigninVerifierTimeOut,
          base::Unretained(this)));

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

void ProfilePickerDiceReauthProvider::OnForceSigninVerifierTimeOut() {
  // TODO(crbug.com/40280498): Improve the error message if this timeout
  // occurs. Currently the error that will be displayed is the one that is shown
  // if the wrong account is being reauth-ed.
  Finish(false, ProfilePickerReauthResult::kTimeoutForceSigninVerifierCheck);
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
  // Stop the timeout check for the ForceSigninVerifier. The response happened.
  timer_.Stop();

  // If the token is valid, we do not need to reauth and proceed to finish
  // with success directly.
  if (token_is_valid) {
    Finish(true, ProfilePickerReauthResult::kSuccessTokenAlreadyValid);
    return;
  }

  ShowReauth();
}

void ProfilePickerDiceReauthProvider::ShowReauth() {
  // Recreating the web contents so that the loading screen is not part of the
  // history when pressing the back button.
  contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(&*profile_));
  // Show the back button, the reactions are handled by the host itself.
  // Use the continue_url to know that the user finalized the reauth flow, in
  // case no refresh token were generated.
  GURL reauth_url =
      GetReauthURL(email_to_reauth_, GaiaUrls::GetInstance()->blank_page_url());
  host_->ShowScreen(
      contents_.get(), reauth_url,
      base::BindOnce(&ProfilePickerWebContentsHost::SetNativeToolbarVisible,
                     // Unretained is enough as the callback is called by the
                     // host itself.
                     base::Unretained(host_), /*visible=*/true));
  // Listen to the changes of the web contents to know if we got to the
  // `continue_url`.
  content::WebContentsObserver::Observe(contents());

  // Creating the DiceTabHelper to detect the new sign in events.
  // This will be used to make sure that the expected account is signed in, and
  // not using a potentially already existing signed in account.
  // On signout error, we proceed with a failure.
  DiceTabHelper::CreateForWebContents(contents());
  DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(contents());
  tab_helper->InitializeSigninFlow(
      reauth_url, signin_metrics::AccessPoint::ACCESS_POINT_FORCED_SIGNIN,
      signin_metrics::Reason::kReauthentication,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO, GURL(), false,
      DiceTabHelper::EnableSyncCallback(),
      base::BindRepeating(
          &ProfilePickerDiceReauthProvider::OnDiceSigninHeaderReceived,
          base::Unretained(this)),
      base::BindRepeating(&ProfilePickerDiceReauthProvider::OnSigninError,
                          base::Unretained(this)));
}

void ProfilePickerDiceReauthProvider::OnDiceSigninHeaderReceived() {
  signin_event_received_ = true;
}

void ProfilePickerDiceReauthProvider::OnSigninError(
    Profile* profile,
    content::WebContents* web_contents,
    const SigninUIError& error) {
  Finish(false, ProfilePickerReauthResult::kTimeoutSigninError);
}

void ProfilePickerDiceReauthProvider::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // If we get to the continue URL without exiting previously, then we wait for
  // potential valid refresh token. If by the delay set, we obtain no token we
  // consider it as a failure.
  // This case can happen when a user uses a previously signed in account
  // through the reauth Gaia page, no token will be emitted.
  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  if (navigation_handle->GetURL().ReplaceComponents(replacements) ==
      GaiaUrls::GetInstance()->blank_page_url()) {
    content::WebContentsObserver::Observe(nullptr);
    host_->SetNativeToolbarVisible(false);

    // If no sign in event was received but we reached the continue URL, then
    // the user used an already signed in account to reauth. The reauth did not
    // happen with the right address. Proceed with a failure (keeping the
    // account signed in).
    if (!signin_event_received_) {
      Finish(false, ProfilePickerReauthResult::kErrorUsedOtherSignedInEmail);
      return;
    }

    // Otherwise we just wait for the refersh token to be valid with a timeout
    // check through the `DiceTabHelper` and the `ProcessDiceHeaderDelegateImpl`
    // after fetching the tokens.
    // Show a loading screen while waiting.
    host_->ShowScreen(contents_.get(), GetLoadingScreenURL());
  }
}

void ProfilePickerDiceReauthProvider::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (!identity_manager_->AreRefreshTokensLoaded() || !force_signin_verifier_) {
    return;
  }

  // Compare the account name that is being reauthed (since the account name can
  // be changed) with the one for which we received a refresh token to know if
  // the reauth was as expected or not.
  // The case where a user uses a previously signed in account which does not
  // generate a token is handled in `DidFinishNavigation()` with the help of the
  // `continue_url` and the `DiceTabHelper`.
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

  Finish(success, success ? ProfilePickerReauthResult::kSuccess
                          : ProfilePickerReauthResult::kErrorUsedNewEmail);
}

void ProfilePickerDiceReauthProvider::Finish(bool success,
                                             ProfilePickerReauthResult result) {
  RecordReauthResult(result);
  scoped_identity_manager_observation_.Reset();
  content::WebContentsObserver::Observe(nullptr);
  // Hide the toolbar in case it was visible after showing the reauth page.
  host_->SetNativeToolbarVisible(false);

  ForceSigninUIError error = ComputeReauthUIError(result, email_to_reauth_);
  std::move(on_reauth_completed_).Run(success, error);
}
