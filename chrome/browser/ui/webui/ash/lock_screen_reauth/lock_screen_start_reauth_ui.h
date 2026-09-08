// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_LOCK_SCREEN_START_REAUTH_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_LOCK_SCREEN_START_REAUTH_UI_H_

#include "ash/constants/webui_url_constants.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_reauth_handler.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/web_dialogs/web_dialog_ui.h"

class ApplicationLocaleStorage;
class PrefService;

namespace policy {
class BrowserPolicyConnectorAsh;
}

namespace ash {

class LockScreenStartReauthUI;

// WebUIConfig for chrome://lock-reauth
class LockScreenStartReauthUIConfig : public content::WebUIConfig {
 public:
  // `local_state`, `application_locale_storage`, and
  // `browser_policy_connector_ash` must be non-null and must outlive `this`.
  LockScreenStartReauthUIConfig(
      PrefService* local_state,
      const ApplicationLocaleStorage* application_locale_storage,
      const policy::BrowserPolicyConnectorAsh* browser_policy_connector_ash);
  LockScreenStartReauthUIConfig(const LockScreenStartReauthUIConfig&) = delete;
  LockScreenStartReauthUIConfig& operator=(
      const LockScreenStartReauthUIConfig&) = delete;
  ~LockScreenStartReauthUIConfig() override;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;

 private:
  const raw_ref<PrefService> local_state_;
  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;
  const raw_ref<const policy::BrowserPolicyConnectorAsh>
      browser_policy_connector_ash_;
};

// For chrome:://lock-reauth
class LockScreenStartReauthUI : public ui::WebDialogUI {
 public:
  // `local_state`, `application_locale_storage`, and
  // `browser_policy_connector_ash` must be non-null and must outlive `this`.
  LockScreenStartReauthUI(
      PrefService* local_state,
      const ApplicationLocaleStorage* application_locale_storage,
      const policy::BrowserPolicyConnectorAsh* browser_policy_connector_ash,
      content::WebUI* web_ui);
  ~LockScreenStartReauthUI() override;

  LockScreenReauthHandler* GetMainHandler() { return main_handler_; }

 private:
  // The main message handler.
  raw_ptr<LockScreenReauthHandler> main_handler_;

  base::WeakPtrFactory<LockScreenStartReauthUI> weak_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_LOCK_SCREEN_START_REAUTH_UI_H_
