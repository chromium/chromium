// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ACCESSIBILITY_MAIN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ACCESSIBILITY_MAIN_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/update_client/crx_update_item.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#else
#include "base/scoped_observer.h"
#include "components/component_updater/component_updater_service.h"
#endif  // defined(OS_CHROMEOS)

namespace base {
class ListValue;
}

class PrefService;

namespace settings {

// Settings handler for the main accessibility settings page,
// chrome://settings/accessibility.
// TODO(1055150) Implement the SODA download progress handling on ChromeOS and
// remove the ChromeOS-only class declaration.
#if defined(OS_CHROMEOS)
class AccessibilityMainHandler : public ::settings::SettingsPageUIHandler {
 public:
  AccessibilityMainHandler();
#else
class AccessibilityMainHandler : public ::settings::SettingsPageUIHandler,
                                 public component_updater::ServiceObserver {
 public:
  explicit AccessibilityMainHandler(PrefService* prefs);
#endif  // defined(OS_CHROMEOS)

  ~AccessibilityMainHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  void HandleA11yPageReady(const base::ListValue* args);
  void HandleCheckAccessibilityImageLabels(const base::ListValue* args);

 private:
  void SendScreenReaderStateChanged();

#if defined(OS_CHROMEOS)
  void OnAccessibilityStatusChanged(
      const chromeos::AccessibilityStatusEventDetails& details);

  std::unique_ptr<chromeos::AccessibilityStatusSubscription>
      accessibility_subscription_;
#else
  // component_updater::ServiceObserver:
  void OnEvent(Events event, const std::string& id) override;

  std::unordered_map<std::string, update_client::CrxUpdateItem>
      downloading_components_;
  PrefService* prefs_;
  ScopedObserver<component_updater::ComponentUpdateService,
                 component_updater::ComponentUpdateService::Observer>
      component_updater_observer_{this};
#endif  // defined(OS_CHROMEOS)

  DISALLOW_COPY_AND_ASSIGN(AccessibilityMainHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ACCESSIBILITY_MAIN_HANDLER_H_
