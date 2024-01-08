// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_LOCK_SCREEN_START_REAUTH_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_LOCK_SCREEN_START_REAUTH_UI_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_reauth_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace ash {

class LockScreenStartReauthUI;

// WebUIConfig for chrome://lock-reauth
class LockScreenStartReauthUIConfig
    : public content::DefaultWebUIConfig<LockScreenStartReauthUI> {
 public:
  LockScreenStartReauthUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUILockScreenStartReauthHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// For chrome:://lock-reauth
class LockScreenStartReauthUI : public ui::WebDialogUI {
 public:
  explicit LockScreenStartReauthUI(content::WebUI* web_ui);
  ~LockScreenStartReauthUI() override;

  LockScreenReauthHandler* GetMainHandler() { return main_handler_; }

 private:
  // The main message handler.
  raw_ptr<LockScreenReauthHandler> main_handler_;

  base::WeakPtrFactory<LockScreenStartReauthUI> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_LOCK_SCREEN_START_REAUTH_UI_H_
