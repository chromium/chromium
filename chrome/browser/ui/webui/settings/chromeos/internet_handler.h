// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_INTERNET_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_INTERNET_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/components/tether/gms_core_notifications_state_tracker.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace chromeos {

namespace tether {
class GmsCoreNotificationsStateTracker;
}  // namespace tether

namespace settings {

// Chrome OS Internet settings page UI handler.
class InternetHandler
    : public chromeos::tether::GmsCoreNotificationsStateTracker::Observer,
      public ::settings::SettingsPageUIHandler {
 public:
  explicit InternetHandler(Profile* profile);
  ~InternetHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // chromeos::tether::GmsCoreNotificationsStateTracker::Observer:
  void OnGmsCoreNotificationStateChanged() override;

 private:
  friend class InternetHandlerTest;

  // Settings JS handlers.
  void AddThirdPartyVpn(const base::ListValue* args);
  void ConfigureThirdPartyVpn(const base::ListValue* args);
  void RequestGmsCoreNotificationsDisabledDeviceNames(
      const base::ListValue* args);
  void ShowCellularSetupUI(const base::ListValue* args);

  // Sets list of names of devices whose "Google Play Services" notifications
  // are disabled.
  void SetGmsCoreNotificationsDisabledDeviceNames();

  // Sends the list of names.
  void SendGmsCoreNotificationsDisabledDeviceNames();

  gfx::NativeWindow GetNativeWindow() const;

  void SetGmsCoreNotificationsStateTrackerForTesting(
      chromeos::tether::GmsCoreNotificationsStateTracker*
          gms_core_notifications_state_tracker);

  std::vector<std::unique_ptr<base::Value>> device_names_without_notifications_;

  Profile* const profile_;

  chromeos::tether::GmsCoreNotificationsStateTracker*
      gms_core_notifications_state_tracker_;

  DISALLOW_COPY_AND_ASSIGN(InternetHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_INTERNET_HANDLER_H_
