// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_sign_in_flow_controller.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/profiles/profile_customization_bubble_sync_controller.h"
#include "chrome/browser/ui/views/profiles/profile_customization_bubble_view.h"
#include "chrome/browser/ui/views/profiles/profile_picker_turn_sync_on_delegate.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "ui/base/theme_provider.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"

namespace {

// Shows the customization bubble if possible. The bubble won't be shown if the
// color is enforced by policy or downloaded through Sync. An IPH is shown after
// the bubble, or right away if the bubble cannot be shown.
void ShowCustomizationBubble(SkColor new_profile_color, Browser* browser) {
  if (!browser)
    return;
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view || !browser_view->toolbar_button_provider())
    return;
  views::View* anchor_view =
      browser_view->toolbar_button_provider()->GetAvatarToolbarButton();
  CHECK(anchor_view);

  // Don't show the customization bubble if a valid policy theme is set.
  if (ThemeServiceFactory::GetForProfile(browser->profile())
          ->UsingPolicyTheme()) {
    browser_view->MaybeShowProfileSwitchIPH();
    return;
  }

  if (ProfileCustomizationBubbleSyncController::CanThemeSyncStart(
          browser->profile())) {
    // For sync users, their profile color has not been applied yet. Call a
    // helper class that applies the color and shows the bubble only if there is
    // no conflict with a synced theme / color.
    ProfileCustomizationBubbleSyncController::
        ApplyColorAndShowBubbleWhenNoValueSynced(
            browser->profile(), anchor_view,
            /*suggested_profile_color=*/new_profile_color);
  } else {
    // For non syncing users, simply show the bubble.
    ProfileCustomizationBubbleView::CreateBubble(browser->profile(),
                                                 anchor_view);
  }
}

GURL GetSigninURL(bool dark_mode) {
  GURL signin_url = GaiaUrls::GetInstance()->signin_chrome_sync_dice();
  if (dark_mode)
    signin_url = net::AppendQueryParameter(signin_url, "color_scheme", "dark");
  return signin_url;
}

GURL GetSyncConfirmationLoadingURL() {
  return GURL(chrome::kChromeUISyncConfirmationURL)
      .Resolve(chrome::kChromeUISyncConfirmationLoadingPath);
}

bool IsExternalURL(const GURL& url) {
  // Empty URL is used initially, about:blank is used to stop navigation after
  // sign-in succeeds.
  if (url.is_empty() || url == GURL(url::kAboutBlankURL))
    return false;
  if (gaia::IsGaiaSignonRealm(url.GetOrigin()))
    return false;
  return true;
}

void ContinueSAMLSignin(std::unique_ptr<content::WebContents> saml_wc,
                        Browser* browser) {
  DCHECK(browser);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  // Attach DiceTabHelper to `saml_wc` so that sync consent dialog appears after
  // a successful sign-in.
  DiceTabHelper::CreateForWebContents(saml_wc.get());
  DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(saml_wc.get());
  // Use |redirect_url| and not |continue_url|, so that the DiceTabHelper can
  // redirect to chrome:// URLs such as the NTP.
  tab_helper->InitializeSigninFlow(
      GetSigninURL(browser_view->GetNativeTheme()->ShouldUseDarkColors()),
      signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
      signin_metrics::Reason::kSigninPrimaryAccount,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
      GURL(chrome::kChromeUINewTabURL));

  browser->tab_strip_model()->ReplaceWebContentsAt(0, std::move(saml_wc));

  ProfileMetrics::LogProfileAddSignInFlowOutcome(
      ProfileMetrics::ProfileAddSignInFlowOutcome::kSAML);
}

}  // namespace

ProfilePickerSignInFlowController::ProfilePickerSignInFlowController(
    ProfilePickerWebContentsHost* host,
    Profile* profile,
    SkColor profile_color,
    base::TimeDelta extended_account_info_timeout)
    : host_(host),
      contents_(content::WebContents::Create(
          content::WebContents::CreateParams(profile))),
      profile_(profile),
      profile_keep_alive_(profile_,
                          ProfileKeepAliveOrigin::kProfileCreationFlow),
      profile_color_(profile_color),
      extended_account_info_timeout_(extended_account_info_timeout) {}

ProfilePickerSignInFlowController::~ProfilePickerSignInFlowController() {
  if (contents())
    contents()->SetDelegate(nullptr);

  // Record unfinished signed-in profile creation.
  if (!is_finished_) {
    // TODO(crbug.com/1227699): Schedule the profile for deletion here, it's not
    // needed any more. This triggers a crash if the browser is shutting down
    // completely. Figure a way how to delete the profile only if that does not
    // compete with a shutdown.

    if (IsSigningIn()) {
      ProfileMetrics::LogProfileAddSignInFlowOutcome(
          ProfileMetrics::ProfileAddSignInFlowOutcome::kAbortedBeforeSignIn);
    } else {
      ProfileMetrics::LogProfileAddSignInFlowOutcome(
          ProfileMetrics::ProfileAddSignInFlowOutcome::kAbortedAfterSignIn);
    }
  }
}

void ProfilePickerSignInFlowController::Init() {
  contents()->SetDelegate(this);

  // Create a manager that supports modal dialogs, such as for webauthn.
  web_modal::WebContentsModalDialogManager::CreateForWebContents(contents());
  web_modal::WebContentsModalDialogManager::FromWebContents(contents())
      ->SetDelegate(this);

  // Listen for sign-in getting completed.
  identity_manager_observation_.Observe(
      IdentityManagerFactory::GetForProfile(profile_));

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_->GetPath());
  if (!entry) {
    NOTREACHED();
    return;
  }

  // Record that the sign in process starts (its end is recorded automatically
  // by the instance of DiceTurnSyncOnHelper constructed later on).
  signin_metrics::RecordSigninUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
  signin_metrics::LogSigninAccessPointStarted(
      signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);

  // Apply the default theme to get consistent colors for toolbars (this matters
  // for linux where the 'system' theme is used for new profiles).
  auto* theme_service = ThemeServiceFactory::GetForProfile(profile_);
  theme_service->UseDefaultTheme();

  // Make sure the web contents used for sign-in has proper background to match
  // the toolbar (for dark mode).
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      contents(), GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR));

  // The back button cannot be created from the constructor as ProfilePickerView
  // needs to access the ThemeProvider of `this` in the process.
  host_->CreateToolbarBackButton();

  host_->ShowScreen(contents(), GetSigninURL(host_->ShouldUseDarkColors()),
                    /*show_toolbar=*/true);
}

void ProfilePickerSignInFlowController::Cancel() {
  if (is_finished_)
    return;

  is_finished_ = true;

  // TODO(crbug.com/1227699): Consider moving this into the destructor so that
  // unfinished (and unaborted) flows also get the profile deleted right away.
  g_browser_process->profile_manager()->ScheduleProfileForDeletion(
      profile_->GetPath(), base::DoNothing());
}

void ProfilePickerSignInFlowController::ReloadSignInPage() {
  if (contents() && IsSigningIn()) {
    contents()->GetController().Reload(content::ReloadType::BYPASSING_CACHE,
                                       true);
  }
}

void ProfilePickerSignInFlowController::SetProfileColor(SkColor color) {
  profile_color_ = color;
}

SkColor ProfilePickerSignInFlowController::GetProfileColor() const {
  // The new profile theme may be overridden by an existing policy theme. This
  // check ensures the correct theme is applied to the sync confirmation window.
  auto* theme_service = ThemeServiceFactory::GetForProfile(profile_);
  if (theme_service->UsingPolicyTheme())
    return theme_service->GetPolicyThemeColor();
  return profile_color_;
}

bool ProfilePickerSignInFlowController::IsSigningIn() const {
  // We are in the sign-in flow if the email is not yet determined.
  return email_.empty();
}

const ui::ThemeProvider* ProfilePickerSignInFlowController::GetThemeProvider()
    const {
  return &ThemeService::GetThemeProviderForProfile(profile_);
}

std::string ProfilePickerSignInFlowController::GetUserDomain() const {
  return gaia::ExtractDomainName(email_);
}

void ProfilePickerSignInFlowController::SwitchToSyncConfirmation() {
  host_->ShowScreen(
      contents(), GURL(chrome::kChromeUISyncConfirmationURL),
      /*show_toolbar=*/false,
      /*enable_navigating_back=*/false,
      /*navigation_finished_closure=*/
      base::BindOnce(
          &ProfilePickerSignInFlowController::SwitchToSyncConfirmationFinished,
          // Unretained is enough as the callback is called by the
          // owner of this instance.
          base::Unretained(this)));
}

void ProfilePickerSignInFlowController::SwitchToProfileSwitch(
    const base::FilePath& profile_path) {
  // The sign-in flow is finished, no profile window should be shown in the end.
  Cancel();

  switch_profile_path_ = profile_path;
  host_->ShowScreenInSystemContents(
      GURL(chrome::kChromeUIProfilePickerUrl).Resolve("profile-switch"),
      /*show_toolbar=*/false,
      /*enable_navigating_back=*/false);
}

void ProfilePickerSignInFlowController::SwitchToEnterpriseProfileWelcome(
    EnterpriseProfileWelcomeUI::ScreenType type,
    base::OnceCallback<void(bool)> proceed_callback) {
  host_->ShowScreen(contents(),
                    GURL(chrome::kChromeUIEnterpriseProfileWelcomeURL),
                    /*show_toolbar=*/false,
                    /*enable_navigating_back=*/false,
                    /*navigation_finished_closure=*/
                    base::BindOnce(&ProfilePickerSignInFlowController::
                                       SwitchToEnterpriseProfileWelcomeFinished,
                                   // Unretained is enough as the callback is
                                   // called by the owner of this instance.
                                   base::Unretained(this), type,
                                   std::move(proceed_callback)));
}

bool ProfilePickerSignInFlowController::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // Ignores context menu.
  return true;
}

void ProfilePickerSignInFlowController::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture,
    bool* was_blocked) {
  NavigateParams params(profile_, target_url, ui::PAGE_TRANSITION_LINK);
  // Open all links as new popups.
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.contents_to_insert = std::move(new_contents);
  params.window_bounds = initial_rect;
  Navigate(&params);
}

bool ProfilePickerSignInFlowController::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return host_->HandleKeyboardEvent(source, event);
}

void ProfilePickerSignInFlowController::NavigationStateChanged(
    content::WebContents* source,
    content::InvalidateTypes changed_flags) {
  if (IsSigningIn() && source == contents_.get() &&
      IsExternalURL(contents_->GetVisibleURL())) {
    FinishAndOpenBrowserForSAML();
  }
}

web_modal::WebContentsModalDialogHost*
ProfilePickerSignInFlowController::GetWebContentsModalDialogHost() {
  return host_;
}

void ProfilePickerSignInFlowController::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  DCHECK(!account_info.IsEmpty());
  email_ = account_info.email;

  base::OnceClosure sync_consent_completed_closure =
      base::BindOnce(&ProfilePickerSignInFlowController::FinishAndOpenBrowser,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::BindOnce(&ShowCustomizationBubble, profile_color_),
                     /*enterprise_sync_consent_needed=*/false);

  // Stop with the sign-in navigation and show a spinner instead. The spinner
  // will be shown until DiceTurnSyncOnHelper (below) figures out whether it's a
  // managed account and whether sync is disabled by policies (which in some
  // cases involves fetching policies and can take a couple of seconds).
  host_->ShowScreen(contents(), GetSyncConfirmationLoadingURL(),
                    /*show_toolbar=*/false, /*enable_navigating_back=*/false);

  // Set up a timeout for extended account info (which cancels any existing
  // timeout closure).
  extended_account_info_timeout_closure_.Reset(base::BindOnce(
      &ProfilePickerSignInFlowController::OnExtendedAccountInfoTimeout,
      weak_ptr_factory_.GetWeakPtr(), account_info));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, extended_account_info_timeout_closure_.callback(),
      extended_account_info_timeout_);

  // DiceTurnSyncOnHelper deletes itself once done.
  new DiceTurnSyncOnHelper(
      profile_, signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
      signin_metrics::Reason::kSigninPrimaryAccount, account_info.account_id,
      DiceTurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
      std::make_unique<ProfilePickerTurnSyncOnDelegate>(
          weak_ptr_factory_.GetWeakPtr(), profile_),
      std::move(sync_consent_completed_closure));
}

void ProfilePickerSignInFlowController::OnExtendedAccountInfoUpdated(
    const AccountInfo& account_info) {
  if (!account_info.IsValid())
    return;
  name_for_signed_in_profile_ =
      profiles::GetDefaultNameForNewSignedInProfile(account_info);
  OnProfileNameAvailable();
  // Extended info arrived on time, no need for the timeout callback any more.
  extended_account_info_timeout_closure_.Cancel();
}

void ProfilePickerSignInFlowController::OnExtendedAccountInfoTimeout(
    const CoreAccountInfo& account) {
  name_for_signed_in_profile_ =
      profiles::GetDefaultNameForNewSignedInProfileWithIncompleteInfo(account);
  OnProfileNameAvailable();
}

void ProfilePickerSignInFlowController::OnProfileNameAvailable() {
  // Stop listening to further changes.
  DCHECK(identity_manager_observation_.IsObservingSource(
      IdentityManagerFactory::GetForProfile(profile_)));
  identity_manager_observation_.Reset();

  if (on_profile_name_available_)
    std::move(on_profile_name_available_).Run();
}

void ProfilePickerSignInFlowController::SwitchToSyncConfirmationFinished() {
  // Initialize the WebUI page once we know it's committed.
  SyncConfirmationUI* sync_confirmation_ui =
      static_cast<SyncConfirmationUI*>(contents()->GetWebUI()->GetController());

  sync_confirmation_ui->InitializeMessageHandlerForCreationFlow(
      GetProfileColor());
}

void ProfilePickerSignInFlowController::
    SwitchToEnterpriseProfileWelcomeFinished(
        EnterpriseProfileWelcomeUI::ScreenType type,
        base::OnceCallback<void(bool)> proceed_callback) {
  // Initialize the WebUI page once we know it's committed.
  EnterpriseProfileWelcomeUI* enterprise_profile_welcome_ui =
      contents()
          ->GetWebUI()
          ->GetController()
          ->GetAs<EnterpriseProfileWelcomeUI>();
  enterprise_profile_welcome_ui->Initialize(/*browser=*/nullptr, type,
                                            GetUserDomain(), GetProfileColor(),
                                            std::move(proceed_callback));
}

void ProfilePickerSignInFlowController::FinishAndOpenBrowser(
    BrowserOpenedCallback callback,
    bool enterprise_sync_consent_needed) {
  // Do nothing if the sign-in flow is aborted or if this has already been
  // called. Note that this can get called first time from a special case
  // handling (such as the Settings link) and than second time when the
  // DiceTurnSyncOnHelper finishes.
  if (is_finished_)
    return;
  is_finished_ = true;

  if (name_for_signed_in_profile_.empty()) {
    on_profile_name_available_ = base::BindOnce(
        &ProfilePickerSignInFlowController::FinishAndOpenBrowserImpl,
        weak_ptr_factory_.GetWeakPtr(), std::move(callback),
        enterprise_sync_consent_needed);
    return;
  }

  FinishAndOpenBrowserImpl(std::move(callback), enterprise_sync_consent_needed);
}

void ProfilePickerSignInFlowController::FinishAndOpenBrowserImpl(
    BrowserOpenedCallback callback,
    bool enterprise_sync_consent_needed) {
  DCHECK(!name_for_signed_in_profile_.empty());

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_->GetPath());
  if (!entry) {
    NOTREACHED();
    return;
  }

  entry->SetIsOmitted(false);
  if (!profile_->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles)) {
    // Unmark this profile ephemeral so that it isn't deleted upon next startup.
    // Profiles should never be made non-ephemeral if ephemeral mode is forced
    // by policy.
    entry->SetIsEphemeral(false);
  }
  entry->SetLocalProfileName(name_for_signed_in_profile_,
                             /*is_default_name=*/false);
  ProfileMetrics::LogProfileAddNewUser(
      ProfileMetrics::ADD_NEW_PROFILE_PICKER_SIGNED_IN);

  // If sync is not enabled (and will not likely be enabled with an enterprise
  // consent), apply a new color to the profile (otherwise, a more complicated
  // logic gets triggered in ShowCustomizationBubble()).
  if (!enterprise_sync_consent_needed &&
      !ProfileCustomizationBubbleSyncController::CanThemeSyncStart(profile_)) {
    auto* theme_service = ThemeServiceFactory::GetForProfile(profile_);
    theme_service->BuildAutogeneratedThemeFromColor(profile_color_);
  }

  // Skip the FRE for this profile as it's replaced by profile creation flow.
  profile_->GetPrefs()->SetBoolean(prefs::kHasSeenWelcomePage, true);

  // TODO(crbug.com/1126913): Change the callback of
  // profiles::OpenBrowserWindowForProfile() to be a OnceCallback as it is only
  // called once.
  profiles::OpenBrowserWindowForProfile(
      base::BindOnce(&ProfilePickerSignInFlowController::OnBrowserOpened,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      /*always_create=*/false,   // Don't create a window if one already exists.
      /*is_new_profile=*/false,  // Don't create a first run window.
      /*unblock_extensions=*/false,  // There is no need to unblock all
                                     // extensions because we only open browser
                                     // window if the Profile is not locked.
                                     // Hence there is no extension blocked.
      profile_, Profile::CREATE_STATUS_INITIALIZED);
}

void ProfilePickerSignInFlowController::FinishAndOpenBrowserForSAML() {
  // First, free up `contents()` to be moved to a new browser window.
  host_->ShowScreenInSystemContents(
      GURL(url::kAboutBlankURL),
      /*show_toolbar=*/false, /*enable_navigating_back=*/false,
      /*navigation_finished_closure=*/
      base::BindOnce(
          &ProfilePickerSignInFlowController::OnSignInContentsFreedUp,
          // Unretained is enough as the callback is called by a
          // member of `host_` that outlives `this`.
          base::Unretained(this)));
}

void ProfilePickerSignInFlowController::OnSignInContentsFreedUp() {
  DCHECK(!is_finished_);
  is_finished_ = true;

  DCHECK(name_for_signed_in_profile_.empty());
  name_for_signed_in_profile_ =
      profiles::GetDefaultNameForNewEnterpriseProfile();
  contents_->SetDelegate(nullptr);
  FinishAndOpenBrowserImpl(
      base::BindOnce(&ContinueSAMLSignin, std::move(contents_)),
      /*enterprise_sync_consent_needed=*/true);
}

void ProfilePickerSignInFlowController::OnBrowserOpened(
    BrowserOpenedCallback finish_flow_callback,
    Profile* profile,
    Profile::CreateStatus profile_create_status) {
  CHECK_EQ(profile, profile_);

  // Hide the flow window. This posts a task on the message loop to destroy the
  // window incl. this view.
  host_->Clear();

  if (!finish_flow_callback)
    return;

  Browser* browser = chrome::FindLastActiveWithProfile(profile);
  CHECK(browser);
  std::move(finish_flow_callback).Run(browser);
}
