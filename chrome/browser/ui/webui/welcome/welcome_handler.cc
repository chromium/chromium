// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/welcome_handler.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ui/base/page_transition_types.h"

const char kWelcomeReturningUserUrl[] = "chrome://welcome/returning-user";

WelcomeHandler::WelcomeHandler(content::WebUI* web_ui)
    : profile_(Profile::FromWebUI(web_ui)),
      login_ui_service_(LoginUIServiceFactory::GetForProfile(profile_)),
      result_(WelcomeResult::DEFAULT),
      is_redirected_welcome_impression_(false) {
  login_ui_service_->AddObserver(this);
}

WelcomeHandler::~WelcomeHandler() {
  login_ui_service_->RemoveObserver(this);

  // If this instance is spawned due to being redirected back to welcome page
  // by the onboarding logic, there's no need to log sign-in metrics again.
  if (is_redirected_welcome_impression_) {
    return;
  }

  // We log that an impression occurred at destruct-time. This can't be done at
  // construct-time on some platforms because this page is shown immediately
  // after a new installation of Chrome and loads while the user is deciding
  // whether or not to opt in to logging.
  signin_metrics::RecordSigninImpressionUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE);

  UMA_HISTOGRAM_ENUMERATION("Welcome.SignInPromptResult", result_,
                            WelcomeResult::WELCOME_RESULT_MAX);
}

bool WelcomeHandler::isValidRedirectUrl() {
  GURL current_url = web_ui()->GetWebContents()->GetVisibleURL();

  return current_url == kWelcomeReturningUserUrl;
}

// Override from LoginUIService::Observer.
void WelcomeHandler::OnSyncConfirmationUIClosed(
    LoginUIService::SyncConfirmationUIClosedResult result) {
  if (result != LoginUIService::ABORT_SIGNIN) {
    result_ = WelcomeResult::SIGNED_IN;

    // When signed in from welcome flow, it's possible to come back to
    // chrome://welcome/... after closing sync-confirmation UI. If current URL
    // matches such a case, do not navigate away.
    if (!is_redirected_welcome_impression_) {
      GoToNewTabPage();
    }
  }
}

// Handles backend events necessary when user clicks "Sign in."
void WelcomeHandler::HandleActivateSignIn(const base::ListValue* args) {
  result_ = WelcomeResult::ATTEMPTED;
  base::RecordAction(base::UserMetricsAction("WelcomePage_SignInClicked"));

  if (IdentityManagerFactory::GetForProfile(profile_)->HasPrimaryAccount()) {
    // In general, this page isn't shown to signed-in users; however, if one
    // should arrive here, then opening the sign-in dialog will likely lead
    // to a crash. Thus, we just act like sign-in was "successful" and whisk
    // them away to the NTP instead.
    GoToNewTabPage();
  } else {
    GURL redirect_url = GURL::EmptyGURL();
    if (args->GetSize() == 1U) {
      std::string url_string;
      CHECK(args->GetString(0, &url_string));
      redirect_url = GURL(url_string);
      DCHECK(redirect_url.is_valid());
    }

    Browser* browser = GetBrowser();
    browser->signin_view_controller()->ShowSignin(
        profiles::BubbleViewMode::BUBBLE_VIEW_MODE_GAIA_SIGNIN, browser,
        signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE, redirect_url);
  }
}

// Handles backend events necessary when user clicks "No thanks."
void WelcomeHandler::HandleUserDecline(const base::ListValue* args) {
  // Set the appropriate decline result, based on whether or not the user
  // attempted to sign in.
  result_ = (result_ == WelcomeResult::ATTEMPTED)
                ? WelcomeResult::ATTEMPTED_DECLINED
                : WelcomeResult::DECLINED;

  GoToNewTabPage();
}

// Override from WebUIMessageHandler.
void WelcomeHandler::RegisterMessages() {
  // Check if this instance of WelcomeHandler is spawned by welcome flow
  // redirecting users back to welcome page. This is done here instead of
  // constructor, because web_ui hasn't loaded yet at that time.
  is_redirected_welcome_impression_ = isValidRedirectUrl();

  web_ui()->RegisterMessageCallback(
      "handleActivateSignIn",
      base::BindRepeating(&WelcomeHandler::HandleActivateSignIn,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "handleUserDecline",
      base::BindRepeating(&WelcomeHandler::HandleUserDecline,
                          base::Unretained(this)));
}

void WelcomeHandler::GoToNewTabPage() {
  WelcomeHandler::GoToURL(GURL(chrome::kChromeUINewTabURL));
}

void WelcomeHandler::GoToURL(GURL url) {
  NavigateParams params(GetBrowser(), url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  params.source_contents = web_ui()->GetWebContents();
  Navigate(&params);
}

Browser* WelcomeHandler::GetBrowser() {
  DCHECK(web_ui());
  content::WebContents* contents = web_ui()->GetWebContents();
  DCHECK(contents);
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  DCHECK(browser);
  return browser;
}
