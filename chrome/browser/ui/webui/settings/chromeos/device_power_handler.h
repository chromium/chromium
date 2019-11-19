// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_POWER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_POWER_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power/power_policy_controller.h"

class PrefChangeRegistrar;
class PrefService;

namespace base {
class ListValue;
class TimeTicks;
}

namespace chromeos {
namespace settings {

// Chrome OS battery status and power settings handler.
class PowerHandler : public ::settings::SettingsPageUIHandler,
                     public PowerManagerClient::Observer {
 public:
  // Idle behaviors presented in the UI. These are mapped to preferences by
  // HandleSetIdleBehavior(). Values are shared with JS and exposed here for
  // tests.
  enum class IdleBehavior {
    DISPLAY_OFF_SLEEP = 0,
    DISPLAY_OFF = 1,
    DISPLAY_ON = 2,
    OTHER = 3,
  };

  // WebUI message name and dictionary keys. Shared with tests.
  static const char kPowerManagementSettingsChangedName[];
  static const char kIdleBehaviorKey[];
  static const char kIdleControlledKey[];
  static const char kLidClosedBehaviorKey[];
  static const char kLidClosedControlledKey[];
  static const char kHasLidKey[];

  // Class used by tests to interact with PowerHandler internals.
  class TestAPI {
   public:
    explicit TestAPI(PowerHandler* handler);
    ~TestAPI();

    void RequestPowerManagementSettings();
    void SetIdleBehavior(IdleBehavior behavior);
    void SetLidClosedBehavior(PowerPolicyController::Action behavior);

   private:
    PowerHandler* handler_;  // Not owned.

    DISALLOW_COPY_AND_ASSIGN(TestAPI);
  };

  explicit PowerHandler(PrefService* prefs);
  ~PowerHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // PowerManagerClient implementation.
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;
  void PowerManagerRestarted() override;
  void LidEventReceived(PowerManagerClient::LidState state,
                        const base::TimeTicks& timestamp) override;

 private:
  // Handler to request updating the power status.
  void HandleUpdatePowerStatus(const base::ListValue* args);

  // Handler to change the power source.
  void HandleSetPowerSource(const base::ListValue* args);

  // Handler to request the current power management settings. Just calls
  // SendPowerManagementSettings().
  void HandleRequestPowerManagementSettings(const base::ListValue* args);

  // Handlers to change the idle and lid-closed behaviors.
  void HandleSetIdleBehavior(const base::ListValue* args);
  void HandleSetLidClosedBehavior(const base::ListValue* args);

  // Updates the UI with the current battery status.
  void SendBatteryStatus();

  // Updates the UI with a list of available dual-role power sources.
  void SendPowerSources();

  // Updates the UI to display the current power management settings. If the
  // settings didn't change since the previous call, nothing is sent unless
  // |force| is true.
  void SendPowerManagementSettings(bool force);

  // Callback used to receive switch states from PowerManagerClient.
  void OnGotSwitchStates(
      base::Optional<PowerManagerClient::SwitchStates> result);

  PrefService* prefs_;              // Not owned.

  // Used to watch power management prefs for changes so the UI can be notified.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  ScopedObserver<PowerManagerClient, PowerManagerClient::Observer>
      power_manager_client_observer_{this};

  // Last lid state received from powerd.
  PowerManagerClient::LidState lid_state_ = PowerManagerClient::LidState::OPEN;

  // Last values sent by SendPowerManagementSettings(), cached here so
  // SendPowerManagementSettings() can avoid spamming the UI after this class
  // changes multiple prefs at once.
  IdleBehavior last_idle_behavior_ = IdleBehavior::DISPLAY_OFF_SLEEP;
  PowerPolicyController::Action last_lid_closed_behavior_ =
      PowerPolicyController::ACTION_SUSPEND;
  bool last_idle_controlled_ = false;
  bool last_lid_closed_controlled_ = false;
  bool last_has_lid_ = true;

  base::WeakPtrFactory<PowerHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PowerHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_POWER_HANDLER_H_
