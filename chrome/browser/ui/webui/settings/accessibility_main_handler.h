// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ACCESSIBILITY_MAIN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ACCESSIBILITY_MAIN_HANDLER_H_

#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace settings {

// Settings handler for the main accessibility settings page,
// chrome://settings/accessibility.
class AccessibilityMainHandler : public ::settings::SettingsPageUIHandler {
 public:
  AccessibilityMainHandler();
  ~AccessibilityMainHandler() override;
  AccessibilityMainHandler(const AccessibilityMainHandler&) = delete;
  AccessibilityMainHandler& operator=(const AccessibilityMainHandler&) = delete;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  void HandleA11yPageReady(const base::Value::List& args);
  void HandleCheckAccessibilityImageLabels(const base::Value::List& args);

 private:
  void SendScreenReaderStateChanged();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void OnAccessibilityStatusChanged(
      const ash::AccessibilityStatusEventDetails& details);

  base::CallbackListSubscription accessibility_subscription_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ACCESSIBILITY_MAIN_HANDLER_H_
