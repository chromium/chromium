// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_ERROR_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_ERROR_UI_H_

#include "chrome/browser/ui/webui/signin/signin_web_dialog_ui.h"

namespace ui {
class WebUI;
}

class SigninErrorUI : public SigninWebDialogUI {
 public:
  explicit SigninErrorUI(content::WebUI* web_ui);

  SigninErrorUI(const SigninErrorUI&) = delete;
  SigninErrorUI& operator=(const SigninErrorUI&) = delete;

  ~SigninErrorUI() override {}

  // SigninWebDialogUI:
  void InitializeMessageHandlerWithBrowser(Browser* browser) override;

 private:
  void InitializeMessageHandlerForProfilePicker();
  void Initialize(Browser* browser, bool from_profile_picker);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_ERROR_UI_H_
