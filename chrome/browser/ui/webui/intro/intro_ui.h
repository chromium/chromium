// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_UI_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/webui/intro/intro_handler.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/browser/web_ui_controller.h"

enum class IntroChoice {
  kContinueWithAccount,
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  kContinueWithoutAccount,
#endif
  kQuit,
};

// Callback specification for `SetSigninChoiceCallback()`.
using IntroSigninChoiceCallback =
    base::StrongAlias<class IntroSigninChoiceCallbackTag,
                      base::OnceCallback<void(IntroChoice)>>;

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

 private:
  friend class ProfilePickerLacrosFirstRunBrowserTestBase;

  void HandleSigninChoice(IntroChoice choice);

  IntroSigninChoiceCallback signin_choice_callback_;
  raw_ptr<IntroHandler> intro_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_UI_H_
