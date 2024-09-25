// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_UI_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/webui/intro/intro_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

enum class IntroChoice {
  kContinueWithAccount,
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  kContinueWithoutAccount,
#endif
  kQuit,
};

// This is also used for logging, so do not remove or reorder existing entries.
enum class DefaultBrowserChoice {
  // The user clicked to set Chrome as their default browser.
  kClickSetAsDefault = 0,
  // The user skipped the prompt to set Chrome as their default browser.
  kSkip = 1,
  // The user exited the first run flow while on the prompt to set Chrome as
  // their default browser.
  kQuit = 2,
  // The prompt was not shown due to a timeout when checking if the browser is
  // already default.
  kNotShownOnTimeout = 3,
  // Chrome was successfully set as default browser.
  kSuccessfullySetAsDefault = 4,
  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kSuccessfullySetAsDefault
};

// Callback specification for `SetSigninChoiceCallback()`.
using IntroSigninChoiceCallback =
    base::StrongAlias<class IntroSigninChoiceCallbackTag,
                      base::OnceCallback<void(IntroChoice)>>;

using DefaultBrowserCallback =
    base::StrongAlias<class DefaultBrowserCallbackTag,
                      base::OnceCallback<void(DefaultBrowserChoice)>>;

class IntroUI;

class IntroUIConfig : public content::DefaultWebUIConfig<IntroUI> {
 public:
  IntroUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIIntroHost) {}
};

// The WebUI controller for `chrome://intro`.
// Drops user inputs until a callback to receive the next one is provided by
// calling `SetSigninChoiceCallback()`.
class IntroUI : public content::WebUIController {
 public:
  explicit IntroUI(content::WebUI* web_ui);

  IntroUI(const IntroUI&) = delete;
  IntroUI& operator=(const IntroUI&) = delete;

  ~IntroUI() override;

  void SetSigninChoiceCallback(IntroSigninChoiceCallback callback);
  void SetDefaultBrowserCallback(DefaultBrowserCallback callback);

 private:
  friend class ProfilePickerLacrosFirstRunBrowserTestBase;

  void HandleSigninChoice(IntroChoice choice);
  void HandleDefaultBrowserChoice(DefaultBrowserChoice choice);

  IntroSigninChoiceCallback signin_choice_callback_;
  DefaultBrowserCallback default_browser_callback_;
  raw_ptr<IntroHandler> intro_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_UI_H_
