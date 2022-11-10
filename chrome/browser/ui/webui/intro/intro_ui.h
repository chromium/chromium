// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_UI_H_

#include "base/functional/callback_forward.h"
#include "content/public/browser/web_ui_controller.h"

// Callback specification for `SetSigninChoiceCallback()`.
using IntroSigninChoiceCallback =
    base::StrongAlias<class IntroSigninChoiceCallbackTag,
                      base::OnceCallback<void(bool sign_in)>>;

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
  void HandleSigninChoice(bool sign_in);

  IntroSigninChoiceCallback signin_choice_callback_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_UI_H_
