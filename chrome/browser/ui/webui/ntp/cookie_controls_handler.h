// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_COOKIE_CONTROLS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_COOKIE_CONTROLS_HANDLER_H_

#include <memory>

#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_ui_message_handler.h"

class CookieControlsHandlerTest;
class Profile;

namespace base {
class ListValue;
class Value;
}  // namespace base

namespace policy {
class PolicyChangeRegistrar;
}

// Handles requests for prefs::kCookieControlsMode retrival/update.
class CookieControlsHandler : public content::WebUIMessageHandler {
 public:
  CookieControlsHandler();
  ~CookieControlsHandler() override;

  // WebUIMessageHandler
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  void HandleCookieControlsToggleChanged(const base::ListValue* args);

  void HandleObserveCookieControlsSettingsChanges(const base::ListValue* args);

  // Whether cookie controls UI should be hidden in incognito ntp.
  static bool ShouldHideCookieControlsUI(const Profile* profile);

  // Whether cookie controls should appear enforced.
  static bool ShouldEnforceCookieControls(const Profile* profile);

  static bool GetToggleCheckedValue(const Profile* profile);

 private:
  friend class CookieControlsHandlerTest;

  // Updates cookie controls UI when third-party cookie blocking setting has
  // changed.
  void SendCookieControlsUIChanges();

  void OnThirdPartyCookieBlockingPolicyChanged(const base::Value* previous,
                                               const base::Value* current);

  PrefChangeRegistrar pref_change_registrar_;

  std::unique_ptr<policy::PolicyChangeRegistrar> policy_registrar_;

  DISALLOW_COPY_AND_ASSIGN(CookieControlsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_COOKIE_CONTROLS_HANDLER_H_
