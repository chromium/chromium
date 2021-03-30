// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_ERROR_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_ERROR_UI_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/webui/signin/signin_web_dialog_ui.h"

namespace ui {
class WebUI;
}

class SigninErrorUI : public SigninWebDialogUI {
 public:
  explicit SigninErrorUI(content::WebUI* web_ui);
  ~SigninErrorUI() override {}

  // SigninWebDialogUI:
  void InitializeMessageHandlerWithBrowser(Browser* browser) override;

 private:
  void InitializeMessageHandlerForProfilePicker();
  void Initialize(Browser* browser, bool is_system_profile);

  DISALLOW_COPY_AND_ASSIGN(SigninErrorUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_ERROR_UI_H_
