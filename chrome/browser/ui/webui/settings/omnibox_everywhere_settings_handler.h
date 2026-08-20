// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_OMNIBOX_EVERYWHERE_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_OMNIBOX_EVERYWHERE_SETTINGS_HANDLER_H_

#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "content/public/browser/web_ui.h"

namespace settings {

// WebUI message handler for Omnibox Everywhere settings on
// chrome://settings/search.
class OmniboxEverywhereSettingsHandler : public SettingsPageUIHandler {
 public:
  OmniboxEverywhereSettingsHandler();
  OmniboxEverywhereSettingsHandler(const OmniboxEverywhereSettingsHandler&) =
      delete;
  OmniboxEverywhereSettingsHandler& operator=(
      const OmniboxEverywhereSettingsHandler&) = delete;
  ~OmniboxEverywhereSettingsHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override;

  void AllowJavascriptForTesting() { AllowJavascript(); }
  bool is_shortcut_suspended_for_testing() const {
    return is_shortcut_suspended_;
  }

  // Retrieves the currently configured Omnibox Everywhere global hotkey
  // accelerator (e.g. "Alt+Space" or custom) from local state and resolves
  // the callback with its localized display string.
  void HandleGetOmniboxEverywhereShortcut(const base::ListValue& args);

  // Updates the Omnibox Everywhere global hotkey string in local state with the
  // user-entered accelerator string.
  void HandleSetOmniboxEverywhereShortcut(const base::ListValue& args);

  // Suspends or resumes global shortcut handling via GlobalAcceleratorListener.
  // Called by WebUI when the user enters or leaves the shortcut recording input
  // field so that pressed keys are captured as text instead of triggering the
  // global action.
  void HandleSetOmniboxEverywhereShortcutSuspensionState(
      const base::ListValue& args);

 private:
  void SetShortcutSuspensionState(bool suspend);

  bool is_shortcut_suspended_ = false;
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_OMNIBOX_EVERYWHERE_SETTINGS_HANDLER_H_
