// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DATE_TIME_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DATE_TIME_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/dbus/system_clock/system_clock_client.h"
#include "components/prefs/pref_change_registrar.h"

namespace base {
class ListValue;
}

namespace content {
class WebUIDataSource;
}

namespace chromeos {
namespace settings {

// Chrome OS date and time settings page UI handler.
class DateTimeHandler : public ::settings::SettingsPageUIHandler,
                        public SystemClockClient::Observer {
 public:
  ~DateTimeHandler() override;

  // Adds load-time values to html_source before creating the handler.
  static DateTimeHandler* Create(content::WebUIDataSource* html_source);

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  DateTimeHandler();

  // SystemClockClient::Observer implementation.
  void SystemClockCanSetTimeChanged(bool can_set_time) override;

  // Called when the page is ready.
  void HandleDateTimePageReady(const base::ListValue* args);

  // Handler to fetch the list of time zones.
  void HandleGetTimeZones(const base::ListValue* args);

  // Called to show the Set Time UI.
  void HandleShowSetDateTimeUI(const base::ListValue* args);

  // Handles clicks on the timezone row on the settings page. This should only
  // be called when the current user is a child.
  void HandleShowParentAccessForTimeZone(const base::ListValue* args);

  // Called when the parent access code was validated with result equals
  // |success|.
  void OnParentAccessValidation(bool success);

  // Updates the UI, enabling or disabling the time zone automatic detection
  // setting according to policy.
  void NotifyTimezoneAutomaticDetectionPolicy();

  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      system_timezone_policy_subscription_;

  // Used to listen to changes to the system time zone detection policy.
  PrefChangeRegistrar local_state_pref_change_registrar_;

  ScopedObserver<SystemClockClient, SystemClockClient::Observer>
      scoped_observer_;
  base::WeakPtrFactory<DateTimeHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DateTimeHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DATE_TIME_HANDLER_H_
