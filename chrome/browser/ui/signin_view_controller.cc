// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin_view_controller.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/signin_view_controller_delegate.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/google_api_keys.h"
#include "url/url_constants.h"

namespace {

// Returns the sign-in reason for |mode|.
signin_metrics::Reason GetSigninReasonFromMode(profiles::BubbleViewMode mode) {
  DCHECK(SigninViewController::ShouldShowSigninForMode(mode));
  switch (mode) {
    case profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN:
      return signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT;
    case profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT:
      return signin_metrics::Reason::REASON_ADD_SECONDARY_ACCOUNT;
    case profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH:
      return signin_metrics::Reason::REASON_REAUTHENTICATION;
    default:
      NOTREACHED();
      return signin_metrics::Reason::REASON_UNKNOWN_REASON;
  }
}

// Opens a new tab on |url| or reuses the current tab if it is the NTP.
void ShowTabOverwritingNTP(Browser* browser, const GURL& url) {
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.user_gesture = true;
  params.tabstrip_add_types |= TabStripModel::ADD_INHERIT_OPENER;

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
    signin::IdentityManager* identity_manager,
    signin::AccountConsistencyMethod account_consistency) {
  if (account_consistency != signin::AccountConsistencyMethod::kDice)
    return signin_metrics::PromoAction::PROMO_ACTION_NEW_ACCOUNT_PRE_DICE;

  return !identity_manager->GetAccountsWithRefreshTokens().empty()
             ? signin_metrics::PromoAction::
                   PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT
             : signin_metrics::PromoAction::
                   PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT;
}

}  // namespace

SigninViewController::SigninViewController() : delegate_(nullptr) {}

SigninViewController::~SigninViewController() {
  CloseModalSignin();
}

// static
bool SigninViewController::ShouldShowSigninForMode(
    profiles::BubbleViewMode mode) {
  return mode == profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN ||
         mode == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT ||
         mode == profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH;
}

void SigninViewController::ShowSignin(profiles::BubbleViewMode mode,
                                      Browser* browser,
                                      signin_metrics::AccessPoint access_point,
                                      const GURL& redirect_url) {
  DCHECK(ShouldShowSigninForMode(mode));

  Profile* profile = browser->profile();
  signin::AccountConsistencyMethod account_consistency =
      AccountConsistencyModeManager::GetMethodForProfile(profile);
  std::string email;
  signin_metrics::Reason signin_reason = GetSigninReasonFromMode(mode);
  if (signin_reason == signin_metrics::Reason::REASON_REAUTHENTICATION) {
    auto* manager = IdentityManagerFactory::GetForProfile(profile);
    email = manager->GetPrimaryAccountInfo().email;
  }
  signin_metrics::PromoAction promo_action = GetPromoActionForNewAccount(
      IdentityManagerFactory::GetForProfile(profile), account_consistency);
  ShowDiceSigninTab(browser, signin_reason, access_point, promo_action, email,
                    redirect_url);
}

void SigninViewController::ShowModalSyncConfirmationDialog(Browser* browser) {
  CloseModalSignin();
  // The delegate will delete itself on request of the UI code when the widget
  // is closed.
  delegate_ = SigninViewControllerDelegate::CreateSyncConfirmationDelegate(
      this, browser);
  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::SIGN_IN_SYNC_CONFIRMATION);
}

void SigninViewController::ShowModalSigninErrorDialog(Browser* browser) {
  CloseModalSignin();
  // The delegate will delete itself on request of the UI code when the widget
  // is closed.
  delegate_ =
      SigninViewControllerDelegate::CreateSigninErrorDelegate(this, browser);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::SIGN_IN_ERROR);
}

bool SigninViewController::ShowsModalDialog() {
  return delegate_ != nullptr;
}

void SigninViewController::CloseModalSignin() {
  if (delegate_)
    delegate_->CloseModalSignin();

  DCHECK(!delegate_);
}

void SigninViewController::SetModalSigninHeight(int height) {
  if (delegate_)
    delegate_->ResizeNativeView(height);
}

void SigninViewController::ResetModalSigninDelegate() {
  delegate_ = nullptr;
}

void SigninViewController::ShowDiceSigninTab(
    Browser* browser,
    signin_metrics::Reason signin_reason,
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action,
    const std::string& email_hint,
    const GURL& redirect_url) {
#if DCHECK_IS_ON()
  if (!signin::DiceMethodGreaterOrEqual(
          AccountConsistencyModeManager::GetMethodForProfile(
              browser->profile()),
          signin::AccountConsistencyMethod::kDiceMigration)) {
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

  // If redirect_url is empty, we would like to redirect to the NTP, but it's
  // not possible through the continue_url, because Gaia cannot redirect to
  // chrome:// URLs. Use the google base URL instead here, and the DiceTabHelper
  // may do the redirect to the NTP later.
  // Note: Gaia rejects some continue URLs as invalid and responds with HTTP
  // error 400. This seems to happen in particular if the continue URL is not a
  // Google-owned domain. Chrome cannot enforce that only valid URLs are used,
  // because the set of valid URLs is not specified.
  std::string continue_url =
      (redirect_url.is_empty() || !redirect_url.SchemeIsHTTPOrHTTPS())
          ? UIThreadSearchTermsData().GoogleBaseURLValue()
          : redirect_url.spec();

  GURL signin_url =
      signin_reason == signin_metrics::Reason::REASON_ADD_SECONDARY_ACCOUNT
          ? signin::GetAddAccountURLForDice(email_hint, continue_url)
          : signin::GetChromeSyncURLForDice(email_hint, continue_url);

  content::WebContents* active_contents = nullptr;
  if (access_point == signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE) {
    active_contents = browser->tab_strip_model()->GetActiveWebContents();
    content::OpenURLParams params(signin_url, content::Referrer(),
                                  WindowOpenDisposition::CURRENT_TAB,
                                  ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
    active_contents->OpenURL(params);
  } else {
    // Check if there is already a signin-tab open.
    TabStripModel* tab_strip = browser->tab_strip_model();
    int dice_tab_index = FindDiceSigninTab(tab_strip, signin_url);
    if (dice_tab_index != -1) {
      if (access_point !=
          signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS) {
        // Extensions do not activate the tab to prevent misbehaving
        // extensions to keep focusing the signin tab.
        tab_strip->ActivateTabAt(dice_tab_index,
                                 {TabStripModel::GestureType::kOther});
      }
      // Do not create a new signin tab, because there is already one.
      return;
    }

    ShowTabOverwritingNTP(browser, signin_url);
    active_contents = browser->tab_strip_model()->GetActiveWebContents();
  }

  DCHECK(active_contents);
  DCHECK_EQ(signin_url, active_contents->GetVisibleURL());
  DiceTabHelper::CreateForWebContents(active_contents);
  DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(active_contents);

  // Use |redirect_url| and not |continue_url|, so that the DiceTabHelper can
  // redirect to chrome:// URLs such as the NTP.
  tab_helper->InitializeSigninFlow(signin_url, access_point, signin_reason,
                                   promo_action, redirect_url);
}

void SigninViewController::ShowDiceEnableSyncTab(
    Browser* browser,
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action,
    const std::string& email_hint) {
  signin_metrics::Reason reason =
      signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT;
  std::string email_to_use = email_hint;
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser->profile());
  if (identity_manager->HasPrimaryAccount()) {
    reason = signin_metrics::Reason::REASON_REAUTHENTICATION;
    email_to_use = identity_manager->GetPrimaryAccountInfo().email;
    DCHECK(email_hint.empty() || gaia::AreEmailsSame(email_hint, email_to_use));
  }
  ShowDiceSigninTab(browser, reason, access_point, promo_action, email_to_use);
}

void SigninViewController::ShowDiceAddAccountTab(
    Browser* browser,
    signin_metrics::AccessPoint access_point,
    const std::string& email_hint) {
  ShowDiceSigninTab(
      browser, signin_metrics::Reason::REASON_ADD_SECONDARY_ACCOUNT,
      access_point, signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
      email_hint);
}

content::WebContents*
SigninViewController::GetModalDialogWebContentsForTesting() {
  DCHECK(delegate_);
  return delegate_->GetWebContents();
}
