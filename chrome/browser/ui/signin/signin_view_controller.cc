// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/signin_view_controller.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/profiles/signin_intercept_first_run_experience_dialog.h"
#include "chrome/browser/ui/signin/signin_modal_dialog.h"
#include "chrome/browser/ui/signin/signin_modal_dialog_impl.h"
#include "chrome/browser/ui/signin/signin_view_controller_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/core_account_id.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/logout_tab_helper.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/signin/chrome_signout_confirmation_prompt.h"
#include "chrome/browser/ui/signin/signin_reauth_view_controller.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/google_api_keys.h"
#include "url/url_constants.h"
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

namespace {

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

// Opens a new tab on |url| or reuses the current tab if it is the NTP.
void ShowTabOverwritingNTP(Browser* browser, const GURL& url) {
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.user_gesture = false;
  params.tabstrip_add_types |= AddTabTypes::ADD_INHERIT_OPENER;

  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (contents) {
    const GURL& contents_url = contents->GetVisibleURL();
    if (contents_url == chrome::kChromeUINewTabURL ||
        search::IsInstantNTP(contents) || contents_url == url::kAboutBlankURL) {
      params.disposition = WindowOpenDisposition::CURRENT_TAB;
    }
  }

  Navigate(&params);
}

// Returns the index of an existing re-usable Dice signin tab, or -1.
int FindDiceSigninTab(TabStripModel* tab_strip, const GURL& signin_url) {
  int tab_count = tab_strip->count();
  for (int tab_index = 0; tab_index < tab_count; ++tab_index) {
    content::WebContents* web_contents = tab_strip->GetWebContentsAt(tab_index);
    DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(web_contents);
    if (tab_helper && tab_helper->signin_url() == signin_url &&
        tab_helper->IsChromeSigninPage()) {
      return tab_index;
    }
  }
  return -1;
}

// Returns the promo action to be used when signing with a new account.
signin_metrics::PromoAction GetPromoActionForNewAccount(
    signin::IdentityManager* identity_manager) {
  return !identity_manager->GetAccountsWithRefreshTokens().empty()
             ? signin_metrics::PromoAction::
                   PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT
             : signin_metrics::PromoAction::
                   PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT;
}

// Called from `SignoutOrReauthWithPrompt()` after the user made a choice on the
// confirmation dialog.
void HandleSignoutConfirmationChoice(
    base::WeakPtr<Browser> browser,
    signin_metrics::AccessPoint reauth_access_point,
    signin_metrics::ProfileSignout profile_signout_source,
    signin_metrics::SourceForRefreshTokenOperation token_signout_source,
    ChromeSignoutConfirmationChoice user_choice) {
  if (!browser) {
    return;
  }

  Profile* profile = browser->profile();
  switch (user_choice) {
    case ChromeSignoutConfirmationChoice::kDismissed:
      return;
    case ChromeSignoutConfirmationChoice::kReauth:
      signin_ui_util::ShowReauthForPrimaryAccountWithAuthError(
          profile, reauth_access_point);
      return;
    case ChromeSignoutConfirmationChoice::kSignout: {
      signin::IdentityManager* identity_manager =
          IdentityManagerFactory::GetForProfile(profile);
      // Sign out from all accounts on the web if needed.
      signin::AccountsInCookieJarInfo accounts_in_cookies =
          identity_manager->GetAccountsInCookieJar();
      if (!accounts_in_cookies.accounts_are_fresh ||
          !accounts_in_cookies.signed_in_accounts.empty()) {
        browser->signin_view_controller()->ShowGaiaLogoutTab(
            token_signout_source);
      }
      if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled(
              switches::ExplicitBrowserSigninPhase::kFull)) {
        // In Uno, Gaia logout tab invalidating the account will lead to a sign
        // in paused state. Unset the primary account to ensure it is removed
        // from chrome. The `AccountReconcilor` will revoke refresh tokens for
        // accounts not in the Gaia cookie on next reconciliation.
        identity_manager->GetPrimaryAccountMutator()
            ->RemovePrimaryAccountButKeepTokens(profile_signout_source);
      }
      return;
    }
  }
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// If this is destroyed before SignalReauthDone is called, will call
// |close_modal_signin_callback_| to stop the ongoing reauth.
class ReauthAbortHandleImpl : public SigninViewController::ReauthAbortHandle {
 public:
  explicit ReauthAbortHandleImpl(base::OnceClosure close_modal_signin_callback);
  ReauthAbortHandleImpl(const ReauthAbortHandleImpl&) = delete;
  ReauthAbortHandleImpl operator=(const ReauthAbortHandleImpl&) = delete;
  ~ReauthAbortHandleImpl() override;

  // Nullifies |close_modal_signin_callback_|.
  void SignalReauthDone();

 private:
  base::OnceClosure close_modal_signin_callback_;
};

ReauthAbortHandleImpl::ReauthAbortHandleImpl(
    base::OnceClosure close_modal_signin_callback)
    : close_modal_signin_callback_(std::move(close_modal_signin_callback)) {
  DCHECK(close_modal_signin_callback_);
}

ReauthAbortHandleImpl::~ReauthAbortHandleImpl() {
  if (close_modal_signin_callback_) {
    std::move(close_modal_signin_callback_).Run();
  }
}

void ReauthAbortHandleImpl::SignalReauthDone() {
  close_modal_signin_callback_.Reset();
}

}  // namespace

SigninViewController::SigninViewController(Browser* browser)
    : browser_(browser) {}

SigninViewController::~SigninViewController() {
  CloseModalSignin();
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void SigninViewController::ShowSignin(signin_metrics::AccessPoint access_point,
                                      const GURL& redirect_url) {
  Profile* profile = browser_->profile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  signin_metrics::PromoAction promo_action =
      GetPromoActionForNewAccount(identity_manager);
  ShowDiceSigninTab(signin_metrics::Reason::kSigninPrimaryAccount, access_point,
                    promo_action, /*email_hint=*/std::string(), redirect_url);
}

void SigninViewController::ShowModalInterceptFirstRunExperienceDialog(
    const CoreAccountId& account_id,
    bool is_forced_intercept) {
  CloseModalSignin();
  auto fre_dialog = std::make_unique<SigninInterceptFirstRunExperienceDialog>(
      browser_, account_id, is_forced_intercept,
      GetOnModalDialogClosedCallback());
  SigninInterceptFirstRunExperienceDialog* raw_dialog = fre_dialog.get();
  // Casts pointer to a base class.
  dialog_ = std::move(fre_dialog);
  raw_dialog->Show();
}

void SigninViewController::SignoutOrReauthWithPrompt(
    signin_metrics::AccessPoint reauth_access_point,
    signin_metrics::ProfileSignout profile_signout_source,
    signin_metrics::SourceForRefreshTokenOperation token_signout_source) {
  Profile* profile = browser_->profile();
  CHECK(profile->IsRegularProfile());
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  base::OnceCallback<void(syncer::ModelTypeSet)> signout_prompt_with_datatypes =
      base::BindOnce(
          &SigninViewController::SignoutOrReauthWithPromptWithUnsyncedDataTypes,
          weak_ptr_factory_.GetWeakPtr(), reauth_access_point,
          profile_signout_source, token_signout_source);
  // Fetch the unsynced datatypes, as this is required to decide whether the
  // confirmation prompt is needed.
  if (sync_service &&
      switches::IsExplicitBrowserSigninUIOnDesktopEnabled(
          switches::ExplicitBrowserSigninPhase::kFull) &&
      profile->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin)) {
    sync_service->GetTypesWithUnsyncedData(
        syncer::TypesRequiringUnsyncedDataCheckOnSignout(),
        std::move(signout_prompt_with_datatypes));
    return;
  }
  // Dice users don't see the prompt, pass empty datatypes.
  std::move(signout_prompt_with_datatypes).Run(syncer::ModelTypeSet());
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
void SigninViewController::ShowModalProfileCustomizationDialog(
    bool is_local_profile_creation) {
  CloseModalSignin();
  dialog_ = std::make_unique<SigninModalDialogImpl>(
      SigninViewControllerDelegate::CreateProfileCustomizationDelegate(
          browser_, is_local_profile_creation,
          /*show_profile_switch_iph=*/true),
      GetOnModalDialogClosedCallback());
}

void SigninViewController::ShowModalSigninEmailConfirmationDialog(
    const std::string& last_email,
    const std::string& email,
    SigninEmailConfirmationDialog::Callback callback) {
  CloseModalSignin();
  content::WebContents* active_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  dialog_ = std::make_unique<SigninModalDialogImpl>(
      SigninEmailConfirmationDialog::AskForConfirmation(
          active_contents, browser_->profile(), last_email, email,
          std::move(callback)),
      GetOnModalDialogClosedCallback());
}

std::unique_ptr<SigninViewController::ReauthAbortHandle>
SigninViewController::ShowReauthPrompt(
    const CoreAccountId& account_id,
    signin_metrics::ReauthAccessPoint access_point,
    base::OnceCallback<void(signin::ReauthResult)> reauth_callback) {
  CloseModalSignin();

  auto abort_handle = std::make_unique<ReauthAbortHandleImpl>(base::BindOnce(
      &SigninViewController::CloseModalSignin, weak_ptr_factory_.GetWeakPtr()));

  // Wrap |reauth_callback| so that it also signals to |reauth_abort_handle|
  // when executed. The handle outlives the callback because it calls
  // CloseModalSignin on destruction, and this runs the callback (with a
  // "cancelled" result). So base::Unretained can be used.
  auto wrapped_reauth_callback = base::BindOnce(
      [](ReauthAbortHandleImpl* handle,
         base::OnceCallback<void(signin::ReauthResult)> cb,
         signin::ReauthResult result) {
        handle->SignalReauthDone();
        std::move(cb).Run(result);
      },
      base::Unretained(abort_handle.get()), std::move(reauth_callback));

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser_->profile());
  // For now, Reauth is restricted to the primary account only.
  // TODO(crbug.com/1083429): add support for secondary accounts.
  CoreAccountId primary_account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);

  if (account_id != primary_account_id) {
    signin_ui_util::RecordTransactionalReauthResult(
        access_point, signin::ReauthResult::kAccountNotSignedIn);
    std::move(wrapped_reauth_callback)
        .Run(signin::ReauthResult::kAccountNotSignedIn);
    return abort_handle;
  }

  dialog_ = std::make_unique<SigninReauthViewController>(
      browser_, account_id, access_point, GetOnModalDialogClosedCallback(),
      std::move(wrapped_reauth_callback));
  return abort_handle;
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

void SigninViewController::ShowModalSyncConfirmationDialog(
    bool is_signin_intercept) {
  CloseModalSignin();
  dialog_ = std::make_unique<SigninModalDialogImpl>(
      SigninViewControllerDelegate::CreateSyncConfirmationDelegate(
          browser_, is_signin_intercept),
      GetOnModalDialogClosedCallback());
}

void SigninViewController::ShowModalManagedUserNoticeDialog(
    const AccountInfo& account_info,
    bool is_oidc_account,
    bool force_new_profile,
    bool show_link_data_option,
    signin::SigninChoiceCallback callback) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
  CloseModalSignin();
  dialog_ = std::make_unique<SigninModalDialogImpl>(
      SigninViewControllerDelegate::CreateManagedUserNoticeDelegate(
          browser_, account_info, is_oidc_account, force_new_profile,
          show_link_data_option, std::move(callback)),
      GetOnModalDialogClosedCallback());
#else
  NOTREACHED() << "Managed user notice dialog modal not supported";
#endif
}

void SigninViewController::ShowModalSigninErrorDialog() {
  CloseModalSignin();
  dialog_ = std::make_unique<SigninModalDialogImpl>(
      SigninViewControllerDelegate::CreateSigninErrorDelegate(browser_),
      GetOnModalDialogClosedCallback());
}

bool SigninViewController::ShowsModalDialog() {
  return dialog_ != nullptr;
}

void SigninViewController::CloseModalSignin() {
  if (dialog_) {
    dialog_->CloseModalDialog();
  }

  DCHECK(!dialog_);
}

void SigninViewController::SetModalSigninHeight(int height) {
  if (dialog_) {
    dialog_->ResizeNativeView(height);
  }
}

void SigninViewController::OnModalDialogClosed() {
  dialog_.reset();
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void SigninViewController::ShowDiceSigninTab(
    signin_metrics::Reason signin_reason,
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action,
    const std::string& email_hint,
    const GURL& redirect_url) {
#if DCHECK_IS_ON()
  if (!AccountConsistencyModeManager::IsDiceEnabledForProfile(
          browser_->profile())) {
    // Developers often fall into the trap of not configuring the OAuth client
    // ID and client secret and then attempt to sign in to Chromium, which
    // fail as the account consistency is disabled. Explicitly check that the
    // OAuth client ID are configured when developers attempt to sign in to
    // Chromium.
    DCHECK(google_apis::HasOAuthClientConfigured())
        << "You must configure the OAuth client ID and client secret in order "
           "to sign in to Chromium. See instruction at "
           "https://www.chromium.org/developers/how-tos/api-keys";

    // Account consistency mode does not support signing in to Chrome due to
    // some other unexpected reason. Signing in to Chrome is not supported.
    NOTREACHED()
        << "OAuth client ID and client secret is configured, but "
           "the account consistency mode does not support signing in to "
           "Chromium.";
  }
#endif

  // We would like to redirect to the NTP, but it's not possible through the
  // `continue_url`, because Gaia cannot redirect to chrome:// URLs. Use the
  // google base URL instead here, and the `DiceTabHelper` redirect to the NTP
  // later.
  // Note: Gaia rejects some continue URLs as invalid and responds with HTTP
  // error 400. This seems to happen in particular if the continue URL is not a
  // Google-owned domain. Chrome cannot enforce that only valid URLs are used,
  // because the set of valid URLs is not specified.
  GURL continue_url =
      (redirect_url.is_empty() || !redirect_url.SchemeIsHTTPOrHTTPS())
          ? GURL(UIThreadSearchTermsData().GoogleBaseURLValue())
          : redirect_url;

  GURL signin_url =
      (signin_reason == signin_metrics::Reason::kAddSecondaryAccount ||
       signin_reason == signin_metrics::Reason::kReauthentication)
          ? signin::GetAddAccountURLForDice(email_hint, continue_url)
          : signin::GetChromeSyncURLForDice({email_hint, continue_url});

  content::WebContents* active_contents = nullptr;
  if (access_point == signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE) {
    active_contents = browser_->tab_strip_model()->GetActiveWebContents();
    content::OpenURLParams params(signin_url, content::Referrer(),
                                  WindowOpenDisposition::CURRENT_TAB,
                                  ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
    active_contents->OpenURL(params, /*navigation_handle_callback=*/{});
  } else {
    // Check if there is already a signin-tab open.
    TabStripModel* tab_strip = browser_->tab_strip_model();
    int dice_tab_index = FindDiceSigninTab(tab_strip, signin_url);
    if (dice_tab_index != -1) {
      if (access_point !=
          signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS) {
        // Extensions do not activate the tab to prevent misbehaving
        // extensions to keep focusing the signin tab.
        tab_strip->ActivateTabAt(
            dice_tab_index,
            TabStripUserGestureDetails(
                TabStripUserGestureDetails::GestureType::kOther));
      }
      // Do not create a new signin tab, because there is already one.
      return;
    }

    ShowTabOverwritingNTP(browser_, signin_url);
    active_contents = browser_->tab_strip_model()->GetActiveWebContents();
  }

  // Checks that we have right contents, in which the signin page is being
  // loaded. Note that we need to check the original URL, being mindful of
  // possible redirects, but also the navigation hasn't happened yet.
  DCHECK(active_contents);
  DCHECK_EQ(
      signin_url,
      active_contents->GetController().GetVisibleEntry()->GetUserTypedURL());
  DiceTabHelper::CreateForWebContents(active_contents);
  DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(active_contents);

  // Use |redirect_url| and not |continue_url|, so that the DiceTabHelper can
  // redirect to chrome:// URLs such as the NTP.
  tab_helper->InitializeSigninFlow(
      signin_url, access_point, signin_reason, promo_action, redirect_url,
      /*record_signin_started_metrics=*/true,
      DiceTabHelper::GetEnableSyncCallbackForBrowser(),
      DiceTabHelper::OnSigninHeaderReceived(),
      DiceTabHelper::GetShowSigninErrorCallbackForBrowser());
}

void SigninViewController::ShowDiceEnableSyncTab(
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action,
    const std::string& email_hint) {
  signin_metrics::Reason reason = signin_metrics::Reason::kSigninPrimaryAccount;
  std::string email_to_use = email_hint;
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser_->profile());
  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    // Avoids asking for the Sync consent as it has been already given.
    reason = signin_metrics::Reason::kReauthentication;
    email_to_use =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
            .email;
    DCHECK(email_hint.empty() || gaia::AreEmailsSame(email_hint, email_to_use));
  }
  ShowDiceSigninTab(reason, access_point, promo_action, email_to_use,
                    GURL(chrome::kChromeUINewTabURL));
}

void SigninViewController::ShowDiceAddAccountTab(
    signin_metrics::AccessPoint access_point,
    const std::string& email_hint) {
  signin_metrics::Reason reason = signin_metrics::Reason::kAddSecondaryAccount;
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser_->profile());
  if (!email_hint.empty() &&
      !identity_manager->FindExtendedAccountInfoByEmailAddress(email_hint)
           .IsEmpty()) {
    // Use more precise `signin_metrics::Reason` if we know that it's a reauth.
    // This only has an impact on metrics.
    reason = signin_metrics::Reason::kReauthentication;
  }

  ShowDiceSigninTab(reason, access_point,
                    signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
                    email_hint, /*redirect_url=*/GURL());
}

void SigninViewController::ShowGaiaLogoutTab(
    signin_metrics::SourceForRefreshTokenOperation source) {
  // Since the user may be triggering navigation from another UI element such as
  // a menu, ensure the web contents (and therefore the page that is about to be
  // shown) is focused. (See crbug/926492 for motivation.)
  auto* const contents = browser_->tab_strip_model()->GetActiveWebContents();
  if (contents) {
    contents->Focus();
  }

  // Pass a continue URL when the Web Signin Intercept bubble is shown, so that
  // the bubble and the app picker do not overlap. If the bubble is not shown,
  // open the app picker in case the user is lost.
  GURL logout_url =
      switches::IsExplicitBrowserSigninUIOnDesktopEnabled(
          switches::ExplicitBrowserSigninPhase::kExperimental)
          ? GaiaUrls::GetInstance()->LogOutURLWithContinueURL(GURL())
          : GaiaUrls::GetInstance()->service_logout_url();
  // Do not use a singleton tab. A new tab should be opened even if there is
  // already a logout tab.
  ShowTabOverwritingNTP(browser_, logout_url);

  // Monitor the logout and fallback to local signout if it fails. The
  // LogoutTabHelper deletes itself.
  content::WebContents* logout_tab_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  DCHECK(logout_tab_contents);
  LogoutTabHelper::CreateForWebContents(logout_tab_contents);
}

void SigninViewController::SignoutOrReauthWithPromptWithUnsyncedDataTypes(
    signin_metrics::AccessPoint reauth_access_point,
    signin_metrics::ProfileSignout profile_signout_source,
    signin_metrics::SourceForRefreshTokenOperation token_signout_source,
    syncer::ModelTypeSet unsynced_datatypes) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser_->profile());
  CoreAccountId primary_account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (primary_account_id.empty()) {
    return;
  }

  // Show the confirmation prompt if there is data pending upload.
  bool should_show_confirmation_prompt = !unsynced_datatypes.empty();
  base::OnceCallback<void(ChromeSignoutConfirmationChoice)> callback =
      base::BindOnce(&HandleSignoutConfirmationChoice, browser_->AsWeakPtr(),
                     reauth_access_point, profile_signout_source,
                     token_signout_source);

  if (should_show_confirmation_prompt) {
    CHECK(!primary_account_id.empty());
    bool needs_reauth =
        !identity_manager->HasAccountWithRefreshToken(primary_account_id) ||
        identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
            primary_account_id);
    ChromeSignoutConfirmationPromptVariant prompt_variant =
        needs_reauth ? ChromeSignoutConfirmationPromptVariant::
                           kUnsyncedDataWithReauthButton
                     : ChromeSignoutConfirmationPromptVariant::kUnsyncedData;

    // Show confirmation prompt where the user can reauth or sign out.
    ShowChromeSignoutConfirmationPrompt(*browser_, prompt_variant,
                                        std::move(callback));
  } else {
    // Sign out immediately
    std::move(callback).Run(ChromeSignoutConfirmationChoice::kSignout);
  }
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

content::WebContents*
SigninViewController::GetModalDialogWebContentsForTesting() {
  DCHECK(dialog_);
  return dialog_->GetModalDialogWebContentsForTesting();  // IN-TEST
}

SigninModalDialog* SigninViewController::GetModalDialogForTesting() {
  return dialog_.get();
}

base::OnceClosure SigninViewController::GetOnModalDialogClosedCallback() {
  return base::BindOnce(
      &SigninViewController::OnModalDialogClosed,
      base::Unretained(this)  // `base::Unretained()` is safe because
                              // `dialog_` is owned by `this`.
  );
}
