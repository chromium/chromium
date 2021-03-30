// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_HATS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_HATS_HANDLER_H_

#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace settings {

// Settings page UI handler that shows HaTS surveys.
class HatsHandler : public SettingsPageUIHandler {
 public:
  HatsHandler();

  // Not copyable or movable
  HatsHandler(const HatsHandler&) = delete;
  HatsHandler& operator=(const HatsHandler&) = delete;

  ~HatsHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  void HandleTryShowHatsSurvey(const base::ListValue* args);

  void HandleTryShowPrivacySandboxHatsSurvey(const base::ListValue* args);

 private:
  friend class HatsHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(HatsHandlerTest, HandleTryShowHatsSurvey);

  // SettingsPageUIHandler implementation.
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_HATS_HANDLER_H_
