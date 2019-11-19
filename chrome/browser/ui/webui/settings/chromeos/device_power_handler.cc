// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/device_power_handler.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/power_utils.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace settings {
namespace {

base::string16 GetBatteryTimeText(base::TimeDelta time_left) {
  int hour = 0;
  int min = 0;
  ash::power_utils::SplitTimeIntoHoursAndMinutes(time_left, &hour, &min);

  base::string16 time_text;
  if (hour == 0 || min == 0) {
    // Display only one unit ("2 hours" or "10 minutes").
    return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                                  ui::TimeFormat::LENGTH_LONG, time_left);
  }

  return ui::TimeFormat::Detailed(ui::TimeFormat::FORMAT_DURATION,
                                  ui::TimeFormat::LENGTH_LONG,
                                  -1,  // force hour and minute output
                                  time_left);
}

int PowerSourceToDisplayId(
    const power_manager::PowerSupplyProperties_PowerSource& source) {
  switch (source.port()) {
    case power_manager::PowerSupplyProperties_PowerSource_Port_UNKNOWN:
      return IDS_POWER_SOURCE_PORT_UNKNOWN;
    case power_manager::PowerSupplyProperties_PowerSource_Port_LEFT:
      return IDS_POWER_SOURCE_PORT_LEFT;
    case power_manager::PowerSupplyProperties_PowerSource_Port_RIGHT:
      return IDS_POWER_SOURCE_PORT_RIGHT;
    case power_manager::PowerSupplyProperties_PowerSource_Port_BACK:
      return IDS_POWER_SOURCE_PORT_BACK;
    case power_manager::PowerSupplyProperties_PowerSource_Port_FRONT:
      return IDS_POWER_SOURCE_PORT_FRONT;
    case power_manager::PowerSupplyProperties_PowerSource_Port_LEFT_FRONT:
      return IDS_POWER_SOURCE_PORT_LEFT_FRONT;
    case power_manager::PowerSupplyProperties_PowerSource_Port_LEFT_BACK:
      return IDS_POWER_SOURCE_PORT_LEFT_BACK;
    case power_manager::PowerSupplyProperties_PowerSource_Port_RIGHT_FRONT:
      return IDS_POWER_SOURCE_PORT_RIGHT_FRONT;
    case power_manager::PowerSupplyProperties_PowerSource_Port_RIGHT_BACK:
      return IDS_POWER_SOURCE_PORT_RIGHT_BACK;
    case power_manager::PowerSupplyProperties_PowerSource_Port_BACK_LEFT:
      return IDS_POWER_SOURCE_PORT_BACK_LEFT;
    case power_manager::PowerSupplyProperties_PowerSource_Port_BACK_RIGHT:
      return IDS_POWER_SOURCE_PORT_BACK_RIGHT;
  }
  NOTREACHED();
  return 0;
}

}  // namespace

const char PowerHandler::kPowerManagementSettingsChangedName[] =
    "power-management-settings-changed";
const char PowerHandler::kIdleBehaviorKey[] = "idleBehavior";
const char PowerHandler::kIdleControlledKey[] = "idleControlled";
const char PowerHandler::kLidClosedBehaviorKey[] = "lidClosedBehavior";
const char PowerHandler::kLidClosedControlledKey[] = "lidClosedControlled";
const char PowerHandler::kHasLidKey[] = "hasLid";

PowerHandler::TestAPI::TestAPI(PowerHandler* handler) : handler_(handler) {}

PowerHandler::TestAPI::~TestAPI() = default;

void PowerHandler::TestAPI::RequestPowerManagementSettings() {
  base::ListValue args;
  handler_->HandleRequestPowerManagementSettings(&args);
}

void PowerHandler::TestAPI::SetIdleBehavior(IdleBehavior behavior) {
  base::ListValue args;
  args.AppendInteger(static_cast<int>(behavior));
  handler_->HandleSetIdleBehavior(&args);
}

void PowerHandler::TestAPI::SetLidClosedBehavior(
    PowerPolicyController::Action behavior) {
  base::ListValue args;
  args.AppendInteger(behavior);
  handler_->HandleSetLidClosedBehavior(&args);
}

PowerHandler::PowerHandler(PrefService* prefs) : prefs_(prefs) {}

PowerHandler::~PowerHandler() {}

void PowerHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "updatePowerStatus",
      base::BindRepeating(&PowerHandler::HandleUpdatePowerStatus,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setPowerSource", base::BindRepeating(&PowerHandler::HandleSetPowerSource,
                                            base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestPowerManagementSettings",
      base::BindRepeating(&PowerHandler::HandleRequestPowerManagementSettings,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setLidClosedBehavior",
      base::BindRepeating(&PowerHandler::HandleSetLidClosedBehavior,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setIdleBehavior",
      base::BindRepeating(&PowerHandler::HandleSetIdleBehavior,
                          base::Unretained(this)));
}

void PowerHandler::OnJavascriptAllowed() {
  PowerManagerClient* power_manager_client = PowerManagerClient::Get();
  power_manager_client_observer_.Add(power_manager_client);
  power_manager_client->GetSwitchStates(base::Bind(
      &PowerHandler::OnGotSwitchStates, weak_ptr_factory_.GetWeakPtr()));

  // Observe power management prefs used in the UI.
  base::Closure callback(base::Bind(&PowerHandler::SendPowerManagementSettings,
                                    base::Unretained(this), false /* force */));
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs_);
  pref_change_registrar_->Add(ash::prefs::kPowerAcIdleAction, callback);
  pref_change_registrar_->Add(ash::prefs::kPowerAcScreenDimDelayMs, callback);
  pref_change_registrar_->Add(ash::prefs::kPowerAcScreenOffDelayMs, callback);
  pref_change_registrar_->Add(ash::prefs::kPowerAcScreenLockDelayMs, callback);
  pref_change_registrar_->Add(ash::prefs::kPowerBatteryIdleAction, callback);
  pref_change_registrar_->Add(ash::prefs::kPowerBatteryScreenDimDelayMs,
                              callback);
  pref_change_registrar_->Add(ash::prefs::kPowerBatteryScreenOffDelayMs,
                              callback);
  pref_change_registrar_->Add(ash::prefs::kPowerBatteryScreenLockDelayMs,
                              callback);
  pref_change_registrar_->Add(ash::prefs::kPowerLidClosedAction, callback);
}

void PowerHandler::OnJavascriptDisallowed() {
  power_manager_client_observer_.RemoveAll();
  pref_change_registrar_.reset();
}

void PowerHandler::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  SendBatteryStatus();
  SendPowerSources();
}

void PowerHandler::PowerManagerRestarted() {
  PowerManagerClient::Get()->GetSwitchStates(base::BindOnce(
      &PowerHandler::OnGotSwitchStates, weak_ptr_factory_.GetWeakPtr()));
}

void PowerHandler::LidEventReceived(PowerManagerClient::LidState state,
                                    const base::TimeTicks& timestamp) {
  lid_state_ = state;
  SendPowerManagementSettings(false /* force */);
}

void PowerHandler::HandleUpdatePowerStatus(const base::ListValue* args) {
  AllowJavascript();
  chromeos::PowerManagerClient::Get()->RequestStatusUpdate();
}

void PowerHandler::HandleSetPowerSource(const base::ListValue* args) {
  AllowJavascript();

  std::string id;
  CHECK(args->GetString(0, &id));
  chromeos::PowerManagerClient::Get()->SetPowerSource(id);
}

void PowerHandler::HandleRequestPowerManagementSettings(
    const base::ListValue* args) {
  AllowJavascript();
  SendPowerManagementSettings(true /* force */);
}

void PowerHandler::HandleSetIdleBehavior(const base::ListValue* args) {
  AllowJavascript();

  int value = 0;
  CHECK(args->GetInteger(0, &value));
  switch (static_cast<IdleBehavior>(value)) {
    case IdleBehavior::DISPLAY_OFF_SLEEP:
      // The default behavior is to turn the display off and sleep. Clear the
      // prefs so we use the default delays.
      prefs_->ClearPref(ash::prefs::kPowerAcIdleAction);
      prefs_->ClearPref(ash::prefs::kPowerAcScreenDimDelayMs);
      prefs_->ClearPref(ash::prefs::kPowerAcScreenOffDelayMs);
      prefs_->ClearPref(ash::prefs::kPowerAcScreenLockDelayMs);
      prefs_->ClearPref(ash::prefs::kPowerBatteryIdleAction);
      prefs_->ClearPref(ash::prefs::kPowerBatteryScreenDimDelayMs);
      prefs_->ClearPref(ash::prefs::kPowerBatteryScreenOffDelayMs);
      prefs_->ClearPref(ash::prefs::kPowerBatteryScreenLockDelayMs);
      break;
    case IdleBehavior::DISPLAY_OFF:
      // Override idle actions to keep the system awake, but use the default
      // screen delays.
      prefs_->SetInteger(ash::prefs::kPowerAcIdleAction,
                         PowerPolicyController::ACTION_DO_NOTHING);
      prefs_->ClearPref(ash::prefs::kPowerAcScreenDimDelayMs);
      prefs_->ClearPref(ash::prefs::kPowerAcScreenOffDelayMs);
      prefs_->ClearPref(ash::prefs::kPowerAcScreenLockDelayMs);
      prefs_->SetInteger(ash::prefs::kPowerBatteryIdleAction,
                         PowerPolicyController::ACTION_DO_NOTHING);
      prefs_->ClearPref(ash::prefs::kPowerBatteryScreenDimDelayMs);
      prefs_->ClearPref(ash::prefs::kPowerBatteryScreenOffDelayMs);
      prefs_->ClearPref(ash::prefs::kPowerBatteryScreenLockDelayMs);
      break;
    case IdleBehavior::DISPLAY_ON:
      // Override idle actions and set screen delays to 0 in order to disable
      // them (i.e. keep the screen on).
      prefs_->SetInteger(ash::prefs::kPowerAcIdleAction,
                         PowerPolicyController::ACTION_DO_NOTHING);
      prefs_->SetInteger(ash::prefs::kPowerAcScreenDimDelayMs, 0);
      prefs_->SetInteger(ash::prefs::kPowerAcScreenOffDelayMs, 0);
      prefs_->SetInteger(ash::prefs::kPowerAcScreenLockDelayMs, 0);
      prefs_->SetInteger(ash::prefs::kPowerBatteryIdleAction,
                         PowerPolicyController::ACTION_DO_NOTHING);
      prefs_->SetInteger(ash::prefs::kPowerBatteryScreenDimDelayMs, 0);
      prefs_->SetInteger(ash::prefs::kPowerBatteryScreenOffDelayMs, 0);
      prefs_->SetInteger(ash::prefs::kPowerBatteryScreenLockDelayMs, 0);
      break;
    default:
      NOTREACHED() << "Invalid idle behavior " << value;
  }
}

void PowerHandler::HandleSetLidClosedBehavior(const base::ListValue* args) {
  AllowJavascript();

  int value = 0;
  CHECK(args->GetInteger(0, &value));
  switch (static_cast<PowerPolicyController::Action>(value)) {
    case PowerPolicyController::ACTION_SUSPEND:
      prefs_->ClearPref(ash::prefs::kPowerLidClosedAction);
      break;
    case PowerPolicyController::ACTION_DO_NOTHING:
      prefs_->SetInteger(ash::prefs::kPowerLidClosedAction,
                         PowerPolicyController::ACTION_DO_NOTHING);
      break;
    default:
      NOTREACHED() << "Unsupported lid-closed behavior " << value;
  }
}

void PowerHandler::SendBatteryStatus() {
  const base::Optional<power_manager::PowerSupplyProperties>& proto =
      PowerManagerClient::Get()->GetLastStatus();
  DCHECK(proto);
  bool charging = proto->battery_state() ==
                  power_manager::PowerSupplyProperties_BatteryState_CHARGING;
  bool calculating = proto->is_calculating_battery_time();
  int percent =
      ash::power_utils::GetRoundedBatteryPercent(proto->battery_percent());
  base::TimeDelta time_left;
  bool show_time = false;

  if (!calculating) {
    time_left = base::TimeDelta::FromSeconds(
        charging ? proto->battery_time_to_full_sec()
                 : proto->battery_time_to_empty_sec());
    show_time = ash::power_utils::ShouldDisplayBatteryTime(time_left);
  }

  base::string16 status_text;
  if (show_time) {
    status_text = l10n_util::GetStringFUTF16(
        charging ? IDS_SETTINGS_BATTERY_STATUS_CHARGING
                 : IDS_SETTINGS_BATTERY_STATUS,
        base::NumberToString16(percent), GetBatteryTimeText(time_left));
  } else {
    status_text = l10n_util::GetStringFUTF16(IDS_SETTINGS_BATTERY_STATUS_SHORT,
                                             base::NumberToString16(percent));
  }

  base::DictionaryValue battery_dict;
  battery_dict.SetBoolean(
      "present",
      proto->battery_state() !=
          power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT);
  battery_dict.SetBoolean("charging", charging);
  battery_dict.SetBoolean("calculating", calculating);
  battery_dict.SetInteger("percent", percent);
  battery_dict.SetString("statusText", status_text);

  FireWebUIListener("battery-status-changed", battery_dict);
}

void PowerHandler::SendPowerSources() {
  const base::Optional<power_manager::PowerSupplyProperties>& proto =
      PowerManagerClient::Get()->GetLastStatus();
  DCHECK(proto);
  base::ListValue sources_list;
  for (int i = 0; i < proto->available_external_power_source_size(); i++) {
    const auto& source = proto->available_external_power_source(i);
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    dict->SetString("id", source.id());
    dict->SetBoolean("is_dedicated_charger", source.active_by_default());
    dict->SetString("description",
                    l10n_util::GetStringUTF16(PowerSourceToDisplayId(source)));
    sources_list.Append(std::move(dict));
  }

  FireWebUIListener(
      "power-sources-changed", sources_list,
      base::Value(proto->external_power_source_id()),
      base::Value(proto->external_power() ==
                  power_manager::PowerSupplyProperties_ExternalPower_USB));
}

void PowerHandler::SendPowerManagementSettings(bool force) {
  // Infer the idle behavior based on the idle action (determining whether we'll
  // sleep eventually or not) and the AC screen-off delay. Policy can request
  // more-nuanced combinations of AC/battery actions and delays, but we wouldn't
  // be able to display something meaningful in the UI in those cases anyway.
  const PowerPolicyController::Action idle_action =
      static_cast<PowerPolicyController::Action>(
          prefs_->GetInteger(ash::prefs::kPowerAcIdleAction));
  IdleBehavior idle_behavior = IdleBehavior::OTHER;
  if (idle_action == PowerPolicyController::ACTION_SUSPEND) {
    idle_behavior = IdleBehavior::DISPLAY_OFF_SLEEP;
  } else if (idle_action == PowerPolicyController::ACTION_DO_NOTHING) {
    idle_behavior =
        (prefs_->GetInteger(ash::prefs::kPowerAcScreenOffDelayMs) > 0
             ? IdleBehavior::DISPLAY_OFF
             : IdleBehavior::DISPLAY_ON);
  }

  const bool idle_controlled =
      prefs_->IsManagedPreference(ash::prefs::kPowerAcIdleAction) ||
      prefs_->IsManagedPreference(ash::prefs::kPowerAcScreenDimDelayMs) ||
      prefs_->IsManagedPreference(ash::prefs::kPowerAcScreenOffDelayMs) ||
      prefs_->IsManagedPreference(ash::prefs::kPowerAcScreenLockDelayMs) ||
      prefs_->IsManagedPreference(ash::prefs::kPowerBatteryIdleAction) ||
      prefs_->IsManagedPreference(ash::prefs::kPowerBatteryScreenDimDelayMs) ||
      prefs_->IsManagedPreference(ash::prefs::kPowerBatteryScreenOffDelayMs) ||
      prefs_->IsManagedPreference(ash::prefs::kPowerBatteryScreenLockDelayMs);

  const PowerPolicyController::Action lid_closed_behavior =
      static_cast<PowerPolicyController::Action>(
          prefs_->GetInteger(ash::prefs::kPowerLidClosedAction));
  const bool lid_closed_controlled =
      prefs_->IsManagedPreference(ash::prefs::kPowerLidClosedAction);
  const bool has_lid = lid_state_ != PowerManagerClient::LidState::NOT_PRESENT;

  // Don't notify the UI if nothing changed.
  if (!force && idle_behavior == last_idle_behavior_ &&
      idle_controlled == last_idle_controlled_ &&
      lid_closed_behavior == last_lid_closed_behavior_ &&
      lid_closed_controlled == last_lid_closed_controlled_ &&
      has_lid == last_has_lid_)
    return;

  base::DictionaryValue dict;
  dict.SetInteger(kIdleBehaviorKey, static_cast<int>(idle_behavior));
  dict.SetBoolean(kIdleControlledKey, idle_controlled);
  dict.SetInteger(kLidClosedBehaviorKey, lid_closed_behavior);
  dict.SetBoolean(kLidClosedControlledKey, lid_closed_controlled);
  dict.SetBoolean(kHasLidKey, has_lid);
  FireWebUIListener(kPowerManagementSettingsChangedName, dict);

  last_idle_behavior_ = idle_behavior;
  last_idle_controlled_ = idle_controlled;
  last_lid_closed_behavior_ = lid_closed_behavior;
  last_lid_closed_controlled_ = lid_closed_controlled;
  last_has_lid_ = has_lid;
}

void PowerHandler::OnGotSwitchStates(
    base::Optional<PowerManagerClient::SwitchStates> result) {
  if (!result.has_value())
    return;
  lid_state_ = result->lid_state;
  SendPowerManagementSettings(false /* force */);
}

}  // namespace settings
}  // namespace chromeos
