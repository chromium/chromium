// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_DICTATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_DICTATION_HANDLER_H_

#include "base/values.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace settings {

// WebUI message handler for Dictation settings.
// Handles requests to set and validate the Dictation shortcut hotkey.
class DictationHandler : public SettingsPageUIHandler {
 public:
  DictationHandler();
  DictationHandler(const DictationHandler&) = delete;
  DictationHandler& operator=(const DictationHandler&) = delete;
  ~DictationHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

 private:
  void HandleGetDictationShortcut(const base::ListValue& args);
  void HandleSetDictationShortcut(const base::ListValue& args);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_DICTATION_HANDLER_H_
