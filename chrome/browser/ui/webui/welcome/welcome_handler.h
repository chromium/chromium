// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_WELCOME_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_WELCOME_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "content/public/browser/web_ui_message_handler.h"

class Browser;
class Profile;
class GURL;

// Handles actions on Welcome page.
class WelcomeHandler : public content::WebUIMessageHandler {
 public:
  explicit WelcomeHandler(content::WebUI* web_ui);

  WelcomeHandler(const WelcomeHandler&) = delete;
  WelcomeHandler& operator=(const WelcomeHandler&) = delete;

  ~WelcomeHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  enum WelcomeResult {
    // User navigated away from page.
    DEFAULT = 0,
    // User clicked the "Get Started" button.
    DECLINED_SIGN_IN = 1,
    // DEPRECATED: User completed sign-in flow.
    // SIGNED_IN = 2,
    // DEPRECATED: User attempted sign-in flow, then navigated away.
    // ATTEMPTED = 3,
    // DEPRECATED: User attempted sign-in flow, then clicked "No Thanks."
    // ATTEMPTED_DECLINED = 4,
    // User started the sign-in flow.
    STARTED_SIGN_IN = 5,
    // New results must be added before this line, and should correspond to
    // values in tools/metrics/histograms/enums.xml.
    WELCOME_RESULT_MAX
  };

  void HandleActivateSignIn(const base::Value::List& args);
  void HandleUserDecline(const base::Value::List& args);
  void GoToNewTabPage();
  void GoToURL(GURL url);
  bool isValidRedirectUrl();

  Browser* GetBrowser();

  raw_ptr<Profile> profile_;
  WelcomeResult result_;

  // Indicates whether this WelcomeHandler instance is spawned due to users
  // being redirected back to welcome page as part of the onboarding flow.
  bool is_redirected_welcome_impression_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_WELCOME_HANDLER_H_
