// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_POWER_DEVICE_POWER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_POWER_DEVICE_POWER_HANDLER_H_

#include <memory>
#include <optional>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power/power_policy_controller.h"

class PrefChangeRegistrar;
class PrefService;

namespace base {
class TimeTicks;
}  // namespace base

namespace ash::settings {

// Chrome OS battery status and power settings handler.
class PowerHandler : public ::settings::SettingsPageUIHandler,
                     public chromeos::PowerManagerClient::Observer {
 public:
  // Idle behaviors presented in the UI. These are mapped to preferences by
  // HandleSetIdleBehavior(). Values are shared with JS and exposed here for
  // tests.
  enum class IdleBehavior {
    DISPLAY_OFF_SLEEP = 0,
    DISPLAY_OFF = 1,
    DISPLAY_ON = 2,
    SHUT_DOWN = 3,
    STOP_SESSION = 4,
  };

  // WebUI message name and dictionary keys. Shared with tests.
  static const char kPowerManagementSettingsChangedName[];
  static const char kPossibleAcIdleBehaviorsKey[];
  static const char kPossibleBatteryIdleBehaviorsKey[];
  static const char kAcIdleManagedKey[];
  static const char kBatteryIdleManagedKey[];
  static const char kCurrentAcIdleBehaviorKey[];
  static const char kCurrentBatteryIdleBehaviorKey[];
  static const char kLidClosedBehaviorKey[];
  static const char kLidClosedControlledKey[];
  static const char kHasLidKey[];
  static const char kAdaptiveChargingKey[];
  static const char kAdaptiveChargingManagedKey[];
  static const char kBatterySaverFeatureEnabledKey[];

  // Class used by tests to interact with PowerHandler internals.
  class TestAPI {
   public:
    explicit TestAPI(PowerHandler* handler);

    TestAPI(const TestAPI&) = delete;
    TestAPI& operator=(const TestAPI&) = delete;

    ~TestAPI();

    void RequestPowerManagementSettings();
    // Sets AC idle behavior to |behavior| if |when_on_ac| is true. Otherwise
    // sets battery idle behavior to |behavior|.
    void SetIdleBehavior(IdleBehavior behavior, bool when_on_ac);
    void SetLidClosedBehavior(chromeos::PowerPolicyController::Action behavior);
    void SetAdaptiveCharging(bool enabled);

   private:
    raw_ptr<PowerHandler> handler_;  // Not owned.
  };

  explicit PowerHandler(PrefService* prefs);

  PowerHandler(const PowerHandler&) = delete;
  PowerHandler& operator=(const PowerHandler&) = delete;

  ~PowerHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // chromeos::PowerManagerClient implementation.
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;
  void PowerManagerRestarted() override;
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        base::TimeTicks timestamp) override;

 private:
  enum class PowerSource { kAc, kBattery };

  // Struct holding possible idle behaviors and the current behavior while
  // charging/when on battery.
  struct IdleBehaviorInfo {
    IdleBehaviorInfo();
    IdleBehaviorInfo(const std::set<IdleBehavior>& possible_behaviors,
                     const IdleBehavior& current_behavior,
                     const bool is_managed);

    IdleBehaviorInfo(const IdleBehaviorInfo& o);
    ~IdleBehaviorInfo();

    bool operator==(const IdleBehaviorInfo& o) const {
      return (possible_behaviors == o.possible_behaviors &&
              current_behavior == o.current_behavior &&
              is_managed == o.is_managed);
    }

    // All possible idle behaviors.
    std::set<IdleBehavior> possible_behaviors;
    // Current idle behavior.
    IdleBehavior current_behavior = IdleBehavior::DISPLAY_OFF_SLEEP;
    // Whether enterpise policy manages idle behavior.
    bool is_managed = false;
  };

  // Handler to request updating the power status.
  void HandleUpdatePowerStatus(const base::Value::List& args);

  // Handler to change the power source.
  void HandleSetPowerSource(const base::Value::List& args);

  // Handler to request the current power management settings. Just calls
  // SendPowerManagementSettings().
  void HandleRequestPowerManagementSettings(const base::Value::List& args);

  // Handlers to change the idle and lid-closed behaviors.
  void HandleSetIdleBehavior(const base::Value::List& args);
  void HandleSetLidClosedBehavior(const base::Value::List& args);

  // Handler to toggle adaptive charging behavior.
  void HandleSetAdaptiveCharging(const base::Value::List& args);

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
      std::optional<chromeos::PowerManagerClient::SwitchStates> result);

  // Returns all possible idle behaviors (that a user can choose from) and
  // current idle behavior based on enterprise policy and other factors when on
  // |power_source|.
  IdleBehaviorInfo GetAllowedIdleBehaviors(PowerSource power_source);

  // Returns true if the enterprise policy enforces any settings that can impact
  // the idle behavior of the device when on |power_source|.
  bool IsIdleManaged(PowerSource power_source);

  const raw_ptr<PrefService> prefs_;

  // Used to watch power management prefs for changes so the UI can be notified.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_client_observation_{this};

  // Last lid state received from powerd.
  chromeos::PowerManagerClient::LidState lid_state_ =
      chromeos::PowerManagerClient::LidState::OPEN;

  // Last values sent by SendPowerManagementSettings(), cached here so
  // SendPowerManagementSettings() can avoid spamming the UI after this class
  // changes multiple prefs at once.
  IdleBehaviorInfo last_ac_idle_info_;
  IdleBehaviorInfo last_battery_idle_info_;
  chromeos::PowerPolicyController::Action last_lid_closed_behavior_ =
      chromeos::PowerPolicyController::ACTION_SUSPEND;
  bool last_lid_closed_controlled_ = false;
  bool last_has_lid_ = true;
  bool last_adaptive_charging_ = false;
  bool last_adaptive_charging_managed_ = false;
  bool last_battery_saver_feature_enabled_ = false;

  base::WeakPtrFactory<PowerHandler> weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_POWER_DEVICE_POWER_HANDLER_H_
