// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_UI_H_

#include "base/memory/weak_ptr.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/web_dialogs/web_dialog_ui.h"

class InlineLoginUI;

// Inline login UI is available on all platforms except Android and Lacros.
class InlineLoginUIConfig : public content::DefaultWebUIConfig<InlineLoginUI> {
 public:
  InlineLoginUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIChromeSigninHost) {}
};

// Inline login WebUI in various signin flows for ChromeOS and Chrome desktop.
// Upon success, the profile of the webui should be populated with proper
// cookies. Then this UI would fetch the oauth2 tokens using the cookies.
class InlineLoginUI : public ui::WebDialogUI {
 public:
  explicit InlineLoginUI(content::WebUI* web_ui);

  InlineLoginUI(const InlineLoginUI&) = delete;
  InlineLoginUI& operator=(const InlineLoginUI&) = delete;

  ~InlineLoginUI() override;

 private:
  base::WeakPtrFactory<InlineLoginUI> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_UI_H_
