// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_PERFORMANCE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_PERFORMANCE_HANDLER_H_

#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace settings {

class PerformanceHandler : public SettingsPageUIHandler,
                           public performance_manager::user_tuning::
                               BatterySaverModeManager::Observer {
 public:
  PerformanceHandler();

  PerformanceHandler(const PerformanceHandler&) = delete;
  PerformanceHandler& operator=(const PerformanceHandler&) = delete;

  ~PerformanceHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  friend class PerformanceHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(PerformanceHandlerTest, GetCurrentOpenSites);

  base::ScopedObservation<
      performance_manager::user_tuning::BatterySaverModeManager,
      performance_manager::user_tuning::BatterySaverModeManager::Observer>
      performance_handler_observer_{this};

  // BatterySaverModeManager::Observer:
  void OnDeviceHasBatteryChanged(bool device_has_battery) override;

  /**
   * Returns a list of currently opened tabs' urls in order of most recently used.
   */
  base::Value GetCurrentOpenSites();
  void HandleGetCurrentOpenSites(const base::Value::List& args);

  /**
   * This function is called from the frontend in order to get the initial
   * state of the battery, and also has the side effect of notifying the handler
   * that it is ready to receive updates for future battery status changes.
   */
  void HandleGetDeviceHasBattery(const base::Value::List& args);
  void HandleOpenFeedbackDialog(const base::Value::List& args);
  void HandleValidateTabDiscardExceptionRule(const base::Value::List& args);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_PERFORMANCE_HANDLER_H_
