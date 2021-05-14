// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_PRIVACY_SANDBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_PRIVACY_SANDBOX_HANDLER_H_

#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace settings {

class PrivacySandboxHandler : public SettingsPageUIHandler {
 public:
  PrivacySandboxHandler() = default;
  ~PrivacySandboxHandler() override = default;

  // SettingsPageUIHandler:
  void RegisterMessages() override;

 private:
  friend class PrivacySandboxHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxHandlerTest, GetFlocId);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxHandlerTest, ResetFlocId);

  void HandleGetFlocId(const base::ListValue* args);

  void HandleResetFlocId(const base::ListValue* args);

  // SettingsPageUIHandler:
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_PRIVACY_SANDBOX_HANDLER_H_
