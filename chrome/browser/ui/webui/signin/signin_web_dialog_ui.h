// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_WEB_DIALOG_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_WEB_DIALOG_UI_H_

#include "ui/web_dialogs/web_dialog_ui.h"

class Browser;

// Base class for web UI dialogs used for sign-in, which must be Browser-aware.
class SigninWebDialogUI : public ui::WebDialogUI {
 public:
  // Creates a WebUI message handler with the specified browser and adds it to
  // the web UI.
  virtual void InitializeMessageHandlerWithBrowser(Browser* browser) = 0;

 protected:
  explicit SigninWebDialogUI(content::WebUI* web_ui);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_WEB_DIALOG_UI_H_
