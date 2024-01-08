// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_INTERNET_INTERNET_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_INTERNET_INTERNET_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/ash/components/tether/gms_core_notifications_state_tracker.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace ash::settings {

// Chrome OS Internet settings page UI handler.
class InternetHandler
    : public tether::GmsCoreNotificationsStateTracker::Observer,
      public ::settings::SettingsPageUIHandler {
 public:
  explicit InternetHandler(Profile* profile);

  InternetHandler(const InternetHandler&) = delete;
  InternetHandler& operator=(const InternetHandler&) = delete;

  ~InternetHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // tether::GmsCoreNotificationsStateTracker::Observer:
  void OnGmsCoreNotificationStateChanged() override;

 private:
  friend class InternetHandlerTest;

  // Settings JS handlers.
  void AddThirdPartyVpn(const base::Value::List& args);
  void ConfigureThirdPartyVpn(const base::Value::List& args);
  void RequestGmsCoreNotificationsDisabledDeviceNames(
      const base::Value::List& args);
  void ShowCarrierAccountDetail(const base::Value::List& args);
  void ShowCellularSetupUI(const base::Value::List& args);
  void ShowPortalSignin(const base::Value::List& args);

  // Sets list of names of devices whose "Google Play Services" notifications
  // are disabled.
  void SetGmsCoreNotificationsDisabledDeviceNames();

  // Sends the list of names.
  void SendGmsCoreNotificationsDisabledDeviceNames();

  gfx::NativeWindow GetNativeWindow();

  void SetGmsCoreNotificationsStateTrackerForTesting(
      tether::GmsCoreNotificationsStateTracker*
          gms_core_notifications_state_tracker);

  base::Value::List device_names_without_notifications_;

  const raw_ptr<Profile, DanglingUntriaged> profile_;

  raw_ptr<tether::GmsCoreNotificationsStateTracker>
      gms_core_notifications_state_tracker_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_INTERNET_INTERNET_HANDLER_H_
