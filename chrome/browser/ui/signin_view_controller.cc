// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin_view_controller.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
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
#include "components/signin/core/browser/profile_management_switches.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "url/url_constants.h"

namespace {

#if !defined(OS_CHROMEOS)
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
int FindDiceSigninTab(TabStripModel* tab_strip) {
  int tab_count = tab_strip->count();
  for (int tab_index = 0; tab_index < tab_count; ++tab_index) {
    content::WebContents* web_contents = tab_strip->GetWebContentsAt(tab_index);
    DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(web_contents);
    if (tab_helper && tab_helper->IsChromeSigninPage())
      return tab_index;
  }
  return -1;
}

// Returns the promo action to be used when signing with a new account.
signin_metrics::PromoAction GetPromoActionForNewAccount(
    AccountTrackerService* account_tracker,
    signin::AccountConsistencyMethod account_consistency) {
  if (account_consistency != signin::AccountConsistencyMethod::kDice)
    return signin_metrics::PromoAction::PROMO_ACTION_NEW_ACCOUNT_PRE_DICE;

  return account_tracker->GetAccounts().size() > 0
             ? signin_metrics::PromoAction::
                   PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT
             : signin_metrics::PromoAction::
                   PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT;
}

#endif

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

#if defined(OS_CHROMEOS)
  ShowModalSigninDialog(mode, browser, access_point);
#else   // defined(OS_CHROMEOS)
  Profile* profile = browser->profile();
  signin::AccountConsistencyMethod account_consistency =
      AccountConsistencyModeManager::GetMethodForProfile(profile);
  if (signin::DiceMethodGreaterOrEqual(
          account_consistency,
          signin::AccountConsistencyMethod::kDiceMigration)) {
    std::string email;
    if (GetSigninReasonFromMode(mode) ==
        signin_metrics::Reason::REASON_REAUTHENTICATION) {
      auto* manager = IdentityManagerFactory::GetForProfile(profile);
      email = manager->GetPrimaryAccountInfo().email;
    }
    signin_metrics::PromoAction promo_action = GetPromoActionForNewAccount(
        AccountTrackerServiceFactory::GetForProfile(profile),
        account_consistency);
    ShowDiceSigninTab(mode, browser, access_point, promo_action, email,
                      redirect_url);
  } else {
    ShowModalSigninDialog(mode, browser, access_point);
  }
#endif  // defined(OS_CHROMEOS)
}

void SigninViewController::ShowModalSigninDialog(
    profiles::BubbleViewMode mode,
    Browser* browser,
    signin_metrics::AccessPoint access_point) {
  CloseModalSignin();
  // The delegate will delete itself on request of the UI code when the widget
  // is closed.
  delegate_ = SigninViewControllerDelegate::CreateModalSigninDelegate(
      this, mode, browser, access_point);

  // When the user has a proxy that requires HTTP auth, loading the sign-in
  // dialog can trigger the HTTP auth dialog.  This means the signin view
  // controller needs a dialog manager to handle any such dialog.
  delegate_->AttachDialogManager();
  chrome::RecordDialogCreation(chrome::DialogIdentifier::SIGN_IN);
}

void SigninViewController::ShowModalSyncConfirmationDialog(Browser* browser) {
  CloseModalSignin();
  // The delegate will delete itself on request of the UI code when the widget
  // is closed.
  delegate_ = SigninViewControllerDelegate::CreateSyncConfirmationDelegate(
      this, browser, false /* is consent bump */);
  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::SIGN_IN_SYNC_CONFIRMATION);
}

void SigninViewController::ShowModalSyncConsentBump(Browser* browser) {
  CloseModalSignin();
  // The delegate will delete itself on request of the UI code when the widget
  // is closed.
  delegate_ = SigninViewControllerDelegate::CreateSyncConfirmationDelegate(
      this, browser, true /* is consent bump */);
  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::UNITY_SYNC_CONSENT_BUMP);
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

void SigninViewController::PerformNavigation() {
  if (delegate_)
    delegate_->PerformNavigation();
}

void SigninViewController::ResetModalSigninDelegate() {
  delegate_ = nullptr;
}

#if !defined(OS_CHROMEOS)
void SigninViewController::ShowDiceSigninTab(
    profiles::BubbleViewMode mode,
    Browser* browser,
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action,
    const std::string& email,
    const GURL& redirect_url) {
  signin_metrics::Reason signin_reason = GetSigninReasonFromMode(mode);
  GURL signin_url = signin::GetSigninURLForDice(browser->profile(), email);
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
    int dice_tab_index = FindDiceSigninTab(tab_strip);
    if (dice_tab_index != -1) {
      if (access_point !=
          signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS) {
        // Extensions do not activate the tab to prevent misbehaving
        // extensions to keep focusing the signin tab.
        tab_strip->ActivateTabAt(dice_tab_index, true /* user_gesture */);
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
  tab_helper->InitializeSigninFlow(signin_url, access_point, signin_reason,
                                   promo_action, redirect_url);
}
#endif  // !defined(OS_CHROMEOS)

content::WebContents*
SigninViewController::GetModalDialogWebContentsForTesting() {
  DCHECK(delegate_);
  return delegate_->web_contents();
}
