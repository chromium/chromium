// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_sign_in_provider.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/trusted_vault/trusted_vault_encryption_keys_tab_helper.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/safe_browsing/buildflags.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "url/gurl.h"

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/safe_browsing/chrome_password_reuse_detection_manager_client.h"
#endif

namespace {

constexpr char kProfilePickerSignInProviderStepHistogram[] =
    "ProfilePicker.SignInProviderStep";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SigninProviderStep {
  kSwitchToSignin = 0,
  kGaiaBlankPageNavigation = 1,
  kFinishFlowSaml = 2,
  kFinishFlowSyncConfirmation = 3,
  kFinishFlowHistoryOptin = 4,

  kMaxValue = kFinishFlowHistoryOptin
};

bool IsTwoFactorIntersitial(const GURL& url) {
  return base::StartsWith(url.spec(), chrome::kGoogleTwoFactorIntersitialURL);
}

bool IsExternalURL(const GURL& url) {
  // Empty URL is used initially, about:blank is used to stop navigation after
  // sign-in succeeds.
  if (url.is_empty() || url == GURL(url::kAboutBlankURL)) {
    return false;
  }
  if (gaia::HasGaiaSchemeHostPort(url)) {
    return false;
  }
  return true;
}

}  // namespace

BASE_FEATURE(kProfilePickerGaiaBlankContinueUrl,
             base::FEATURE_ENABLED_BY_DEFAULT);

ProfilePickerSignInProvider::ProfilePickerSignInProvider(
    ProfilePickerWebContentsHost* host,
    signin_metrics::AccessPoint signin_access_point,
    const std::string& initial_email,
    base::FilePath profile_path)
    : host_(host),
      signin_access_point_(signin_access_point),
      initial_email_(initial_email),
      profile_path_(profile_path) {}

ProfilePickerSignInProvider::~ProfilePickerSignInProvider() {
  // Handle unfinished signed-in profile creation (i.e. when callback was not
  // called yet).
  if (callback_) {
    if (IsInitialized()) {
      ResetWebContentsDelegates();
    }

    ProfileMetrics::LogProfileAddSignInFlowOutcome(
        ProfileMetrics::ProfileSignedInFlowOutcome::kAbortedBeforeSignIn);
  }
}

void ProfilePickerSignInProvider::SwitchToSignIn(
    StepSwitchFinishedCallback switch_finished_callback,
    SignedInCallback signin_finished_callback) {
  base::UmaHistogramEnumeration(kProfilePickerSignInProviderStepHistogram,
                                SigninProviderStep::kSwitchToSignin);

  // Update the callback even if the profile is already initialized (to respect
  // that the callback may be different).
  callback_ = std::move(signin_finished_callback);

  if (IsInitialized()) {
    // Do not load any url because the desired sign-in screen is still loaded in
    // `contents()`.
    host_->ShowScreen(
        contents(), GURL(),
        base::BindOnce(std::move(switch_finished_callback.value()), true));
    host_->SetNativeToolbarVisible(true);
    return;
  }

  auto profile_init_callback = base::BindOnce(
      &ProfilePickerSignInProvider::OnProfileInitialized,
      weak_ptr_factory_.GetWeakPtr(), std::move(switch_finished_callback));
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_path_.empty()) {
    bool profile_exists = profile_manager->LoadProfileByPath(
        profile_path_, /*incognito=*/false, std::move(profile_init_callback));
    DCHECK(profile_exists);
  } else {
    // Silently create the new profile for browsing on GAIA (so that the sign-in
    // cookies are stored in the right profile).
    ProfileManager::CreateMultiProfileAsync(
        profile_manager->GetProfileAttributesStorage()
            .ChooseNameForNewProfile(),
        profiles::GetPlaceholderAvatarIndex(), /*is_hidden=*/true,
        std::move(profile_init_callback));
  }
}

void ProfilePickerSignInProvider::ReloadSignInPage() {
  if (IsInitialized() && contents()) {
    contents()->GetController().Reload(content::ReloadType::BYPASSING_CACHE,
                                       true);
  }
}

void ProfilePickerSignInProvider::NavigateBack() {
  if (!IsInitialized() || !contents()) {
    return;
  }

  if (contents()->GetController().CanGoBack()) {
    contents()->GetController().GoBack();
    return;
  }

  // Move from sign-in back to the previous screen of profile creation.
  // Do not load any url because the desired screen is still loaded in the
  // picker contents.
  host_->ShowScreenInPickerContents(GURL(), base::OnceClosure());
  host_->SetNativeToolbarVisible(false);
}

bool ProfilePickerSignInProvider::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Ignores context menu.
  return true;
}

content::WebContents* ProfilePickerSignInProvider::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  // ForceSignin flow should not have any potential link that opens a new
  // browser. Currently the regular sign in flow does not contain any, but the
  // SAML Force Signin flow contains a SAML speedbump page, which still contains
  // external links like "Help", "Privacy" and "Terms" that will attempt to open
  // a browser. As long as those links are accessible, we should not try to open
  // them while Force Signin is enabled.
  // TODO(crbug.com/41493894): Remove this check if the SAML speedbump is
  // removed or if the links on the page are removed.
  if (signin_util::IsForceSigninEnabled()) {
    return nullptr;
  }

  NavigateParams params(profile_, target_url, ui::PAGE_TRANSITION_LINK);
  // Open all links as new popups.
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.contents_to_insert = std::move(new_contents);
  params.window_features = window_features;
  Navigate(&params);
  return nullptr;
}

bool ProfilePickerSignInProvider::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return host_->GetWebContentsDelegate()->HandleKeyboardEvent(source, event);
}

void ProfilePickerSignInProvider::NavigationStateChanged(
    content::WebContents* source,
    content::InvalidateTypes changed_flags) {
  if (source != contents_.get()) {
    return;
  }

  const GURL& visible_url = contents_->GetVisibleURL();
  auto primary_account =
      IdentityManagerFactory::GetForProfile(profile_)->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
  if (IsTwoFactorIntersitial(visible_url) && !primary_account.IsEmpty()) {
    // This intersitial should be skipped while in the profile picker, so we
    // finish flow with the current primary account. The intersitial will be
    // opened in a tab after the profile is created. This is handled by the
    // signed-in flow controller.
    // Posting the task to avoid navigation re-entrancy caused by the
    // next step of the flow causing a navigation.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProfilePickerSignInProvider::FinishFlow,
                       weak_ptr_factory_.GetWeakPtr(), primary_account));
    return;
  }

  if (signin_util::IsForceSigninEnabled()) {
    // SAML with ForceSignin in Profile Picker should follow the regular flow.
    return;
  }

  if (visible_url == GaiaUrls::GetInstance()->blank_page_url()) {
    base::UmaHistogramEnumeration(kProfilePickerSignInProviderStepHistogram,
                                  SigninProviderStep::kGaiaBlankPageNavigation);
    return;
  }

  if (!IsExternalURL(visible_url)) {
    return;
  }

  // Attach DiceTabHelper to `contents_` so that sync consent dialog appears
  // after a successful sign-in.
  base::UmaHistogramEnumeration(kProfilePickerSignInProviderStepHistogram,
                                SigninProviderStep::kFinishFlowSaml);
  DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(contents_.get());
  CHECK(tab_helper);
  InitializeOrUpdateDiceTabHelper(*tab_helper, DiceTabHelperMode::kInBrowser);
  // The rest of the SAML flow logic is handled by the signed-in flow
  // controller.
  FinishFlow(CoreAccountInfo());
}

web_modal::WebContentsModalDialogHost*
ProfilePickerSignInProvider::GetWebContentsModalDialogHost(
    content::WebContents* web_contents) {
  return host_->GetWebContentsModalDialogHost();
}

void ProfilePickerSignInProvider::OnProfileInitialized(
    StepSwitchFinishedCallback switch_finished_callback,
    Profile* new_profile) {
  if (!new_profile) {
    std::move(switch_finished_callback.value()).Run(false);
    return;
  }
  DCHECK(!profile_);
  DCHECK(!contents());

  profile_ = new_profile;
  profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
      profile_, ProfileKeepAliveOrigin::kProfileCreationFlow);

  contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(profile_));
  contents()->SetDelegate(this);

  // Create a manager that supports modal dialogs, such as for webauthn.
  web_modal::WebContentsModalDialogManager::CreateForWebContents(contents());
  web_modal::WebContentsModalDialogManager::FromWebContents(contents())
      ->SetDelegate(this);

  // To allow passing encryption keys during interactions with the page,
  // instantiate TrustedVaultEncryptionKeysTabHelper.
  TrustedVaultEncryptionKeysTabHelper::CreateForWebContents(contents());

  // Record that the sign in process starts. Its end is recorded automatically
  // when the primary account is set.
  signin_metrics::RecordSigninUserActionForAccessPoint(signin_access_point_);
  signin_metrics::LogSigninAccessPointStarted(
      signin_access_point_,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);

  // Apply the default theme to get consistent colors for toolbars in newly
  // created profiles (this matters for linux where the 'system' theme is used
  // for new profiles).
  if (profile_path_.empty()) {
    auto* theme_service = ThemeServiceFactory::GetForProfile(profile_);
    theme_service->UseDefaultTheme();
  }

  // Make sure the web contents used for sign-in has proper background to match
  // the toolbar (for dark mode).
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      contents(), host_->GetPreferredBackgroundColor());

  base::OnceClosure navigation_finished_closure =
      base::BindOnce(&ProfilePickerWebContentsHost::SetNativeToolbarVisible,
                     // Unretained is enough as the callback is called by the
                     // host itself.
                     base::Unretained(host_), /*visible=*/true)
          .Then(base::BindOnce(std::move(switch_finished_callback.value()),
                               true));
  host_->ShowScreen(contents(), BuildSigninURL(),
                    std::move(navigation_finished_closure));
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  ChromePasswordReuseDetectionManagerClient::CreateForProfilePickerWebContents(
      contents());
#endif
  // Attach a `DiceTabHelper` to the `WebContents` to trigger the completion
  // of the step.
  DiceTabHelper::CreateForWebContents(contents());
  DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(contents());
  CHECK(tab_helper);
  InitializeOrUpdateDiceTabHelper(*tab_helper, DiceTabHelperMode::kInPicker);
}

bool ProfilePickerSignInProvider::IsInitialized() const {
  return profile_ != nullptr;
}

void ProfilePickerSignInProvider::FinishFlow(
    const CoreAccountInfo& account_info) {
  DCHECK(IsInitialized());
  host_->SetNativeToolbarVisible(false);
  ResetWebContentsDelegates();
  std::move(callback_).Run(profile_.get(), account_info, std::move(contents_),
                           SigninUIError::Ok());
}

void ProfilePickerSignInProvider::FinishFlowInPickerWithSyncConfirmation(
    Profile* profile,
    signin_metrics::AccessPoint /*access_point*/,
    signin_metrics::PromoAction /*promo_action*/,
    content::WebContents* /*contents*/,
    const CoreAccountInfo& account_info) {
  CHECK_EQ(profile, profile_.get());
  base::UmaHistogramEnumeration(
      kProfilePickerSignInProviderStepHistogram,
      SigninProviderStep::kFinishFlowSyncConfirmation);
  FinishFlow(account_info);
}

void ProfilePickerSignInProvider::FinishFlowInPickerWithHistorySyncOptin(
    Profile* profile,
    content::WebContents* /*contents*/,
    const CoreAccountInfo& account_info,
    signin_metrics::AccessPoint /*access_point*/) {
  CHECK_EQ(profile, profile_.get());
  base::UmaHistogramEnumeration(kProfilePickerSignInProviderStepHistogram,
                                SigninProviderStep::kFinishFlowHistoryOptin);
  FinishFlow(account_info);
}

void ProfilePickerSignInProvider::ShowSigninError(
    Profile* profile,
    content::WebContents* contents,
    const SigninUIError& error) {
  if (!base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    return;
  }

  if (signin_util::IsForceSigninEnabled() &&
      error.type() ==
          SigninUIError::Type::kUsernameNotAllowedByPatternFromPrefs) {
    host_->Reset(StepSwitchFinishedCallback(base::BindOnce(
        &ProfilePickerWebContentsHost::ShowForceSigninErrorDialog,
        base::Unretained(host_),
        ForceSigninUIError::SigninPatternNotMatching(
            base::UTF16ToUTF8(error.email())))));
    return;
  }

  if (error.type() ==
      SigninUIError::Type::kAccountAlreadyUsedByAnotherProfile) {
    GURL profile_switch_url(chrome::kChromeUIProfilePickerUrl);
    profile_switch_url = profile_switch_url.Resolve("profile-switch");
    // Appends the `profile_path` to be retrieved in the web page.
    profile_switch_url =
        net::AppendQueryParameter(profile_switch_url, "profileSwitchPath",
                                  base::ToString(error.another_profile_path()));

    host_->ShowScreenInPickerContents(profile_switch_url, base::OnceClosure());
    return;
  }

  std::move(callback_).Run(profile_.get(), CoreAccountInfo(),
                           std::move(contents_), error);
}

void ProfilePickerSignInProvider::ResetWebContentsDelegates() {
  contents()->SetDelegate(nullptr);
  web_modal::WebContentsModalDialogManager::FromWebContents(contents())
      ->SetDelegate(nullptr);
}

GURL ProfilePickerSignInProvider::BuildSigninURL() const {
  // Use the Emebedded flow if we are in the context of ForceSignin.
  signin::Flow signin_flow = signin_util::IsForceSigninEnabled()
                                 ? signin::Flow::EMBEDDED_PROMO
                                 : signin::Flow::PROMO;

  GURL continue_url;
  if (base::FeatureList::IsEnabled(kProfilePickerGaiaBlankContinueUrl)) {
    // Do not navigate out of the Gaia domain, to avoid triggering the SAML
    // flow.
    continue_url = GaiaUrls::GetInstance()->blank_page_url();
  }

  return signin::GetChromeSyncURLForDice({
      .email = initial_email_,
      .continue_url = std::move(continue_url),
      .request_dark_scheme = host_->ShouldUseDarkColors(),
      .flow = signin_flow,
  });
}

void ProfilePickerSignInProvider::InitializeOrUpdateDiceTabHelper(
    DiceTabHelper& helper,
    DiceTabHelperMode mode) {
  switch (mode) {
    case DiceTabHelperMode::kInPicker:
      // This is the default case. The signin flow starts in the picker,
      // assuming that this is not SAML. If the user uses a SAML account, a
      // browser window will open, and the `DiceTabHelper` will be reinitialized
      // with the `kInBrowser` mode.
      helper.InitializeSigninFlow(
          BuildSigninURL(), signin_access_point_,
          signin_metrics::Reason::kSigninPrimaryAccount,
          signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
          // Use |redirect_url| and not |continue_url|, so that the
          // DiceTabHelper can redirect to chrome:// URLs such as the NTP.
          /*redirect_url=*/GURL(),
          /*record_signin_started_metrics=*/true,
          base::BindRepeating(&ProfilePickerSignInProvider::
                                  FinishFlowInPickerWithSyncConfirmation,
                              weak_ptr_factory_.GetWeakPtr()),
          base::BindRepeating(&ProfilePickerSignInProvider::
                                  FinishFlowInPickerWithHistorySyncOptin,
                              weak_ptr_factory_.GetWeakPtr()),
          DiceTabHelper::OnSigninHeaderReceived(),
          base::BindRepeating(&ProfilePickerSignInProvider::ShowSigninError,
                              weak_ptr_factory_.GetWeakPtr()));
      tab_helper_is_initialized_ = true;
      return;
    case DiceTabHelperMode::kInBrowser:
      CHECK(tab_helper_is_initialized_);
      // This is used when a SAML flow is detected (through a navigation outside
      // of Gaia).
      // Also triggered when a navigation outside of Gaia occurs (e.g. account
      // recovery in crbug.com/29524688).
      helper.UpdateSyncCallback(
          DiceTabHelper::GetEnableSyncCallbackForBrowser());
      helper.UpdateHistorySyncOptinCallback(
          DiceTabHelper::GetHistorySyncOptinCallbackForBrowser());
      helper.UpdateSigninErrorCallback(
          DiceTabHelper::GetShowSigninErrorCallbackForBrowser());
      helper.UpdateRedirectUrl(GURL(chrome::kChromeUINewTabURL));
      return;
  }
}
