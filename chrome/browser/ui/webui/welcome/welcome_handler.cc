// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/welcome_handler.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ui/base/page_transition_types.h"

const char kWelcomeReturningUserUrl[] = "chrome://welcome/returning-user";

WelcomeHandler::WelcomeHandler(content::WebUI* web_ui)
    : profile_(Profile::FromWebUI(web_ui)),
      result_(WelcomeResult::DEFAULT),
      is_redirected_welcome_impression_(false) {
}

WelcomeHandler::~WelcomeHandler() {
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

}

bool WelcomeHandler::isValidRedirectUrl() {
  GURL current_url = web_ui()->GetWebContents()->GetVisibleURL();

  return current_url == kWelcomeReturningUserUrl;
}

// Handles backend events necessary when user clicks "Sign in."
void WelcomeHandler::HandleActivateSignIn(const base::Value::List& args) {
  result_ = WelcomeResult::STARTED_SIGN_IN;
  base::RecordAction(base::UserMetricsAction("WelcomePage_SignInClicked"));

  if (IdentityManagerFactory::GetForProfile(profile_)->HasPrimaryAccount(
          signin::ConsentLevel::kSync)) {
    // In general, this page isn't shown to signed-in users; however, if one
    // should arrive here, then opening the sign-in dialog will likely lead
    // to a crash. Thus, we just act like sign-in was "successful" and whisk
    // them away to the NTP instead.
    GoToNewTabPage();
  } else {
    GURL redirect_url(chrome::kChromeUINewTabURL);
    if (args.size() == 1U) {
      const std::string& url_string = args[0].GetString();
      redirect_url = GURL(url_string);
      DCHECK(redirect_url.is_valid());
    }

    Browser* browser = GetBrowser();
    browser->signin_view_controller()->ShowSignin(
        signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE, redirect_url);
  }
}

// Handles backend events necessary when user clicks "Get started."
void WelcomeHandler::HandleUserDecline(const base::Value::List& args) {
  result_ = WelcomeResult::DECLINED_SIGN_IN;
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
  Browser* browser = chrome::FindBrowserWithTab(contents);
  DCHECK(browser);
  return browser;
}
