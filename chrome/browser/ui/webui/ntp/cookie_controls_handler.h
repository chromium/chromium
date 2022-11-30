// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_COOKIE_CONTROLS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_COOKIE_CONTROLS_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "content/public/browser/web_ui_message_handler.h"

// Communicates with the incognito ntp to show a third-party cookie control.
class CookieControlsHandler : public content::WebUIMessageHandler,
                              public CookieControlsService::Observer {
 public:
  explicit CookieControlsHandler(Profile* profile);

  CookieControlsHandler(const CookieControlsHandler&) = delete;
  CookieControlsHandler& operator=(const CookieControlsHandler&) = delete;

  ~CookieControlsHandler() override;

  // WebUIMessageHandler
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  void HandleCookieControlsToggleChanged(const base::Value::List& args);
  void HandleObserveCookieControlsSettingsChanges(
      const base::Value::List& args);
  static const char* GetEnforcementIcon(CookieControlsEnforcement enforcement);

  // CookieControlsService::Observer
  void OnThirdPartyCookieBlockingPrefChanged() override;
  void OnThirdPartyCookieBlockingPolicyChanged() override;

 private:
  // Updates cookie controls UI when third-party cookie blocking setting has
  // changed.
  void SendCookieControlsUIChanges();

  raw_ptr<CookieControlsService> service_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_COOKIE_CONTROLS_HANDLER_H_
