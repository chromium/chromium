// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_DEVICE_STYLUS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_DEVICE_STYLUS_HANDLER_H_

#include <set>
#include <string>

#include "base/scoped_observation.h"
#include "chrome/browser/ash/note_taking/note_taking_helper.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace ash::settings {

// Chrome OS stylus settings handler.
class StylusHandler : public ::settings::SettingsPageUIHandler,
                      public NoteTakingHelper::Observer,
                      public ui::InputDeviceEventObserver {
 public:
  StylusHandler();

  StylusHandler(const StylusHandler&) = delete;
  StylusHandler& operator=(const StylusHandler&) = delete;

  ~StylusHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // NoteTakingHelper::Observer implementation.
  void OnAvailableNoteTakingAppsUpdated() override;
  void OnPreferredNoteTakingAppUpdated(Profile* profile) override;

  // ui::InputDeviceObserver:
  void OnDeviceListsComplete() override;

 private:
  void UpdateNoteTakingApps();
  void HandleRequestApps(const base::Value::List& unused_args);
  void HandleSetPreferredNoteTakingApp(const base::Value::List& args);
  void HandleSetPreferredNoteTakingAppEnabledOnLockScreen(
      const base::Value::List& args);
  void HandleInitialize(const base::Value::List& args);

  // Enables or disables the stylus UI section.
  void SendHasStylus();

  // Called by JS to show the Play Store Android app.
  void HandleShowPlayStoreApps(const base::Value::List& args);

  // IDs of available note-taking apps.
  std::set<std::string> note_taking_app_ids_;

  // Observer registration.
  base::ScopedObservation<NoteTakingHelper, NoteTakingHelper::Observer>
      note_observation_{this};
  base::ScopedObservation<ui::DeviceDataManager, ui::InputDeviceEventObserver>
      input_observation_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_DEVICE_STYLUS_HANDLER_H_
