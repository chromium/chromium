// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/power/device_power_handler.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/power_utils.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash::settings {

namespace {

using ::chromeos::PowerManagerClient;
using ::chromeos::PowerPolicyController;

std::u16string GetBatteryTimeText(base::TimeDelta time_left) {
  int hour = 0;
  int min = 0;
  power_utils::SplitTimeIntoHoursAndMinutes(time_left, &hour, &min);

  std::u16string time_text;
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
  NOTREACHED_IN_MIGRATION();
  return 0;
}

}  // namespace

PowerHandler::IdleBehaviorInfo::IdleBehaviorInfo() = default;
PowerHandler::IdleBehaviorInfo::~IdleBehaviorInfo() = default;

PowerHandler::IdleBehaviorInfo::IdleBehaviorInfo(
    const std::set<PowerHandler::IdleBehavior>& possible_behaviors,
    const PowerHandler::IdleBehavior& current_behavior,
    const bool is_managed)
    : possible_behaviors(possible_behaviors),
      current_behavior(current_behavior),
      is_managed(is_managed) {}

PowerHandler::IdleBehaviorInfo::IdleBehaviorInfo(const IdleBehaviorInfo& o) =
    default;

const char PowerHandler::kPowerManagementSettingsChangedName[] =
    "power-management-settings-changed";

const char PowerHandler::kPossibleAcIdleBehaviorsKey[] =
    "possibleAcIdleBehaviors";
const char PowerHandler::kPossibleBatteryIdleBehaviorsKey[] =
    "possibleBatteryIdleBehaviors";
const char PowerHandler::kCurrentAcIdleBehaviorKey[] = "currentAcIdleBehavior";
const char PowerHandler::kCurrentBatteryIdleBehaviorKey[] =
    "currentBatteryIdleBehavior";
const char PowerHandler::kLidClosedBehaviorKey[] = "lidClosedBehavior";
const char PowerHandler::kAcIdleManagedKey[] = "acIdleManaged";
const char PowerHandler::kBatteryIdleManagedKey[] = "batteryIdleManaged";

const char PowerHandler::kLidClosedControlledKey[] = "lidClosedControlled";
const char PowerHandler::kHasLidKey[] = "hasLid";
const char PowerHandler::kAdaptiveChargingKey[] = "adaptiveCharging";
const char PowerHandler::kAdaptiveChargingManagedKey[] =
    "adaptiveChargingManaged";
const char PowerHandler::kBatterySaverFeatureEnabledKey[] =
    "batterySaverFeatureEnabled";

PowerHandler::TestAPI::TestAPI(PowerHandler* handler) : handler_(handler) {}

PowerHandler::TestAPI::~TestAPI() = default;

void PowerHandler::TestAPI::RequestPowerManagementSettings() {
  handler_->HandleRequestPowerManagementSettings(base::Value::List());
}

void PowerHandler::TestAPI::SetIdleBehavior(IdleBehavior behavior,
                                            bool when_on_ac) {
  base::Value::List args;
  args.Append(static_cast<int>(behavior));
  args.Append(when_on_ac);
  handler_->HandleSetIdleBehavior(args);
}

void PowerHandler::TestAPI::SetLidClosedBehavior(
    PowerPolicyController::Action behavior) {
  base::Value::List args;
  args.Append(behavior);
  handler_->HandleSetLidClosedBehavior(args);
}

void PowerHandler::TestAPI::SetAdaptiveCharging(bool enabled) {
  base::Value::List args;
  args.Append(enabled);
  handler_->HandleSetAdaptiveCharging(args);
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
  web_ui()->RegisterMessageCallback(
      "setAdaptiveCharging",
      base::BindRepeating(&PowerHandler::HandleSetAdaptiveCharging,
                          base::Unretained(this)));
}

void PowerHandler::OnJavascriptAllowed() {
  PowerManagerClient* power_manager_client = PowerManagerClient::Get();
  power_manager_client_observation_.Observe(power_manager_client);
  power_manager_client->GetSwitchStates(base::BindOnce(
      &PowerHandler::OnGotSwitchStates, weak_ptr_factory_.GetWeakPtr()));

  // Observe power management prefs used in the UI.
  base::RepeatingClosure callback(
      base::BindRepeating(&PowerHandler::SendPowerManagementSettings,
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
  pref_change_registrar_->Add(ash::prefs::kPowerAdaptiveChargingEnabled,
                              callback);
}

void PowerHandler::OnJavascriptDisallowed() {
  power_manager_client_observation_.Reset();
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
                                    base::TimeTicks timestamp) {
  lid_state_ = state;
  SendPowerManagementSettings(false /* force */);
}

void PowerHandler::HandleUpdatePowerStatus(const base::Value::List& args) {
  AllowJavascript();
  chromeos::PowerManagerClient::Get()->RequestStatusUpdate();
}

void PowerHandler::HandleSetPowerSource(const base::Value::List& args) {
  AllowJavascript();

  const std::string& id = args[0].GetString();
  chromeos::PowerManagerClient::Get()->SetPowerSource(id);
}

void PowerHandler::HandleRequestPowerManagementSettings(
    const base::Value::List& args) {
  AllowJavascript();
  SendPowerManagementSettings(true /* force */);
}

void PowerHandler::HandleSetIdleBehavior(const base::Value::List& args) {
  AllowJavascript();

  const auto& list = args;
  CHECK_GE(list.size(), 2u);
  int value = list[0].GetInt();
  bool when_on_ac = list[1].GetBool();

  const char* idle_pref = when_on_ac ? ash::prefs::kPowerAcIdleAction
                                     : ash::prefs::kPowerBatteryIdleAction;
  const char* screen_dim_delay_pref =
      when_on_ac ? ash::prefs::kPowerAcScreenDimDelayMs
                 : ash::prefs::kPowerBatteryScreenDimDelayMs;
  const char* screen_off_delay_pref =
      when_on_ac ? ash::prefs::kPowerAcScreenOffDelayMs
                 : ash::prefs::kPowerBatteryScreenOffDelayMs;
  const char* screen_lock_delay_pref =
      when_on_ac ? ash::prefs::kPowerAcScreenLockDelayMs
                 : ash::prefs::kPowerBatteryScreenLockDelayMs;

  switch (static_cast<IdleBehavior>(value)) {
    case IdleBehavior::DISPLAY_OFF_SLEEP:
      // The default behavior is to turn the display off and sleep.
      // Clear the prefs so we use the default delays.
      prefs_->ClearPref(idle_pref);
      prefs_->ClearPref(screen_dim_delay_pref);
      prefs_->ClearPref(screen_off_delay_pref);
      prefs_->ClearPref(screen_lock_delay_pref);
      break;
    case IdleBehavior::DISPLAY_OFF:
      // Override idle actions to keep the system awake, but use the
      // default screen delays.
      prefs_->SetInteger(idle_pref, PowerPolicyController::ACTION_DO_NOTHING);
      prefs_->ClearPref(screen_dim_delay_pref);
      prefs_->ClearPref(screen_off_delay_pref);
      prefs_->ClearPref(screen_lock_delay_pref);
      break;
    case IdleBehavior::DISPLAY_ON:
      // Override idle actions and set screen delays to 0 in order to
      // disable them (i.e. keep the screen on).
      prefs_->SetInteger(idle_pref, PowerPolicyController::ACTION_DO_NOTHING);
      prefs_->SetInteger(screen_dim_delay_pref, 0);
      prefs_->SetInteger(screen_off_delay_pref, 0);
      prefs_->SetInteger(screen_lock_delay_pref, 0);
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid idle behavior " << value;
  }
}

void PowerHandler::HandleSetLidClosedBehavior(const base::Value::List& args) {
  AllowJavascript();

  const auto& list = args;
  CHECK_GE(list.size(), 1u);
  int value = list[0].GetInt();
  switch (static_cast<PowerPolicyController::Action>(value)) {
    case PowerPolicyController::ACTION_SUSPEND:
      prefs_->ClearPref(ash::prefs::kPowerLidClosedAction);
      break;
    case PowerPolicyController::ACTION_DO_NOTHING:
      prefs_->SetInteger(ash::prefs::kPowerLidClosedAction,
                         PowerPolicyController::ACTION_DO_NOTHING);
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Unsupported lid-closed behavior " << value;
  }
}

void PowerHandler::HandleSetAdaptiveCharging(const base::Value::List& args) {
  AllowJavascript();

  CHECK_GE(args.size(), 1u);
  bool enabled = args[0].GetBool();

  prefs_->SetBoolean(ash::prefs::kPowerAdaptiveChargingEnabled, enabled);
}

void PowerHandler::SendBatteryStatus() {
  const std::optional<power_manager::PowerSupplyProperties>& proto =
      PowerManagerClient::Get()->GetLastStatus();
  DCHECK(proto);
  bool charging = proto->battery_state() ==
                  power_manager::PowerSupplyProperties_BatteryState_CHARGING;
  bool calculating = proto->is_calculating_battery_time();
  int percent = power_utils::GetRoundedBatteryPercent(proto->battery_percent());
  base::TimeDelta time_left;
  bool show_time = false;

  if (!calculating) {
    time_left = base::Seconds(charging ? proto->battery_time_to_full_sec()
                                       : proto->battery_time_to_empty_sec());
    show_time = power_utils::ShouldDisplayBatteryTime(time_left);
  }

  std::u16string status_text;
  if (show_time) {
    status_text = l10n_util::GetStringFUTF16(
        charging ? IDS_SETTINGS_BATTERY_STATUS_CHARGING
                 : IDS_SETTINGS_BATTERY_STATUS,
        base::NumberToString16(percent), GetBatteryTimeText(time_left));
  } else {
    status_text = l10n_util::GetStringFUTF16(IDS_SETTINGS_BATTERY_STATUS_SHORT,
                                             base::NumberToString16(percent));
  }

  auto battery_dict =
      base::Value::Dict()
          .Set(
              "present",
              proto->battery_state() !=
                  power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT)
          .Set("charging", charging)
          .Set("calculating", calculating)
          .Set("percent", percent)
          .Set("statusText", status_text);

  FireWebUIListener("battery-status-changed", battery_dict);
}

void PowerHandler::SendPowerSources() {
  const std::optional<power_manager::PowerSupplyProperties>& proto =
      PowerManagerClient::Get()->GetLastStatus();
  DCHECK(proto);
  base::Value::List sources_list;
  for (int i = 0; i < proto->available_external_power_source_size(); i++) {
    const auto& source = proto->available_external_power_source(i);
    sources_list.Append(
        base::Value::Dict()
            .Set("id", source.id())
            .Set("is_dedicated_charger", source.active_by_default())
            .Set("description",
                 l10n_util::GetStringUTF16(PowerSourceToDisplayId(source))));
  }

  FireWebUIListener(
      "power-sources-changed", sources_list,
      base::Value(proto->external_power_source_id()),
      base::Value(proto->external_power() ==
                  power_manager::PowerSupplyProperties_ExternalPower_USB),
      base::Value(proto->external_power() ==
                  power_manager::PowerSupplyProperties_ExternalPower_AC));
}

void PowerHandler::SendPowerManagementSettings(bool force) {
  const PowerHandler::IdleBehaviorInfo ac_idle_info =
      GetAllowedIdleBehaviors(PowerSource::kAc);
  const PowerHandler::IdleBehaviorInfo battery_idle_info =
      GetAllowedIdleBehaviors(PowerSource::kBattery);

  const PowerPolicyController::Action lid_closed_behavior =
      static_cast<PowerPolicyController::Action>(
          prefs_->GetInteger(ash::prefs::kPowerLidClosedAction));
  const bool lid_closed_controlled =
      prefs_->IsManagedPreference(ash::prefs::kPowerLidClosedAction);
  const bool has_lid = lid_state_ != PowerManagerClient::LidState::NOT_PRESENT;

  const bool adaptive_charging =
      prefs_->GetBoolean(ash::prefs::kPowerAdaptiveChargingEnabled);
  const bool adaptive_charging_managed =
      prefs_->IsManagedPreference(ash::prefs::kPowerAdaptiveChargingEnabled);
  const bool battery_saver_feature_enabled =
      ash::features::IsBatterySaverAvailable();
  // Don't notify the UI if nothing changed.
  if (!force && ac_idle_info == last_ac_idle_info_ &&
      battery_idle_info == last_battery_idle_info_ &&
      lid_closed_behavior == last_lid_closed_behavior_ &&
      lid_closed_controlled == last_lid_closed_controlled_ &&
      has_lid == last_has_lid_ &&
      adaptive_charging == last_adaptive_charging_ &&
      adaptive_charging_managed == last_adaptive_charging_managed_ &&
      battery_saver_feature_enabled == last_battery_saver_feature_enabled_) {
    return;
  }

  auto dict =
      base::Value::Dict()
          .Set(kCurrentAcIdleBehaviorKey,
               static_cast<int>(ac_idle_info.current_behavior))
          .Set(kCurrentBatteryIdleBehaviorKey,
               static_cast<int>(battery_idle_info.current_behavior))
          .Set(kLidClosedBehaviorKey, lid_closed_behavior)
          .Set(kAcIdleManagedKey, ac_idle_info.is_managed)
          .Set(kBatteryIdleManagedKey, battery_idle_info.is_managed)
          .Set(kLidClosedControlledKey, lid_closed_controlled)
          .Set(kHasLidKey, has_lid)
          .Set(kAdaptiveChargingKey, adaptive_charging)
          .Set(kAdaptiveChargingManagedKey, adaptive_charging_managed)
          .Set(kBatterySaverFeatureEnabledKey, battery_saver_feature_enabled);

  base::Value::List* list = dict.EnsureList(kPossibleAcIdleBehaviorsKey);
  for (auto idle_behavior : ac_idle_info.possible_behaviors) {
    list->Append(static_cast<int>(idle_behavior));
  }

  list = dict.EnsureList(kPossibleBatteryIdleBehaviorsKey);
  for (auto idle_behavior : battery_idle_info.possible_behaviors) {
    list->Append(static_cast<int>(idle_behavior));
  }

  FireWebUIListener(kPowerManagementSettingsChangedName, dict);

  last_ac_idle_info_ = ac_idle_info;
  last_battery_idle_info_ = battery_idle_info;
  last_lid_closed_behavior_ = lid_closed_behavior;
  last_lid_closed_controlled_ = lid_closed_controlled;
  last_has_lid_ = has_lid;
  last_adaptive_charging_ = adaptive_charging;
  last_adaptive_charging_managed_ = adaptive_charging_managed;
  last_battery_saver_feature_enabled_ = battery_saver_feature_enabled;
}

void PowerHandler::OnGotSwitchStates(
    std::optional<PowerManagerClient::SwitchStates> result) {
  if (!result.has_value()) {
    return;
  }
  lid_state_ = result->lid_state;
  SendPowerManagementSettings(false /* force */);
}

PowerHandler::IdleBehaviorInfo PowerHandler::GetAllowedIdleBehaviors(
    PowerSource power_source) {
  const char* idle_pref = power_source == PowerSource::kAc
                              ? ash::prefs::kPowerAcIdleAction
                              : ash::prefs::kPowerBatteryIdleAction;
  const char* screen_off_delay_pref =
      power_source == PowerSource::kAc
          ? ash::prefs::kPowerAcScreenOffDelayMs
          : ash::prefs::kPowerBatteryScreenOffDelayMs;

  std::set<IdleBehavior> possible_behaviors;
  IdleBehavior current_idle_behavior;

  // If idle action is managed and set to suspend, only possible idle
  // behaviour is sleep with display off.
  if (prefs_->IsManagedPreference(idle_pref) &&
      prefs_->GetInteger(idle_pref) == PowerPolicyController::ACTION_SUSPEND) {
    current_idle_behavior = IdleBehavior::DISPLAY_OFF_SLEEP;
    possible_behaviors.insert(IdleBehavior::DISPLAY_OFF_SLEEP);
    return IdleBehaviorInfo(possible_behaviors, current_idle_behavior,
                            IsIdleManaged(power_source));
  }

  // If idle action is managed and set to STOP_SESSION, STOP_SESSION is the only
  // possibility.
  if (prefs_->IsManagedPreference(idle_pref) &&
      (prefs_->GetInteger(idle_pref) ==
       PowerPolicyController::ACTION_STOP_SESSION)) {
    current_idle_behavior = IdleBehavior::STOP_SESSION;
    possible_behaviors.insert(IdleBehavior::STOP_SESSION);
    return IdleBehaviorInfo(possible_behaviors, current_idle_behavior,
                            IsIdleManaged(power_source));
  }

  // If idle action is managed and set to SHUT_DOWN, SHUT_DOWN is the only
  // possibility.
  if (prefs_->IsManagedPreference(idle_pref) &&
      (prefs_->GetInteger(idle_pref) ==
       PowerPolicyController::ACTION_SHUT_DOWN)) {
    current_idle_behavior = IdleBehavior::SHUT_DOWN;
    possible_behaviors.insert(IdleBehavior::SHUT_DOWN);
    return IdleBehaviorInfo(possible_behaviors, current_idle_behavior,
                            IsIdleManaged(power_source));
  }

  // Note that after this point |idle_pref| should either be:
  //    1. Not managed.
  //    2. Or managed and set to
  //    PowerPolicyController::ACTION_DO_NOTHING.
  DCHECK(!prefs_->IsManagedPreference(idle_pref) ||
         (prefs_->GetInteger(idle_pref) ==
          PowerPolicyController::ACTION_DO_NOTHING));

  // If screen off delay is managed and set to a value greater than 0
  // and
  //   1. If idle action is managed and set to DO_NOTHING, only
  //      possible idle behavior is DISPLAY_OFF.
  //   2. If idle action is not managed then possible idle options
  //      are DiSPLAY_OFF and DISPLAY_OFF_SLEEP
  if (prefs_->IsManagedPreference(screen_off_delay_pref) &&
      prefs_->GetInteger(screen_off_delay_pref) > 0) {
    if (prefs_->IsManagedPreference(idle_pref) &&
        prefs_->GetInteger(idle_pref) ==
            PowerPolicyController::ACTION_DO_NOTHING) {
      current_idle_behavior = IdleBehavior::DISPLAY_OFF;
      possible_behaviors.insert(IdleBehavior::DISPLAY_OFF);
      return IdleBehaviorInfo(possible_behaviors, current_idle_behavior,
                              IsIdleManaged(power_source));
    }

    possible_behaviors.insert(IdleBehavior::DISPLAY_OFF);
    possible_behaviors.insert(IdleBehavior::DISPLAY_OFF_SLEEP);
    // Set the current default option based on the current idle action.
    const PowerPolicyController::Action idle_action =
        static_cast<PowerPolicyController::Action>(
            prefs_->GetInteger(idle_pref));

    if (idle_action == PowerPolicyController::ACTION_SUSPEND) {
      current_idle_behavior = IdleBehavior::DISPLAY_OFF_SLEEP;
    } else {
      current_idle_behavior = IdleBehavior::DISPLAY_OFF;
    }
    return IdleBehaviorInfo(possible_behaviors, current_idle_behavior,
                            IsIdleManaged(power_source));
  }

  // If idle action is managed and set to DO_NOTHING, and
  //   1. If screen off delay is also managed (and set to 0), only
  //   possible idle
  //      action is DISPLAY_ON.
  //   2. If AC screen off delay is not managed, possible idle actions
  //      are DISPLAY_ON && DISPLAY_OFF.
  if (prefs_->IsManagedPreference(idle_pref) &&
      prefs_->GetInteger(idle_pref) ==
          PowerPolicyController::ACTION_DO_NOTHING) {
    if (prefs_->IsManagedPreference(screen_off_delay_pref)) {
      // Note that we reach here only when screen off delays are
      // set by enterprise policy to 0 to prevent display from turning
      // off.
      DCHECK(prefs_->GetInteger(screen_off_delay_pref) == 0);
      current_idle_behavior = IdleBehavior::DISPLAY_ON;
      possible_behaviors.insert(IdleBehavior::DISPLAY_ON);
      return IdleBehaviorInfo(possible_behaviors, current_idle_behavior,
                              IsIdleManaged(power_source));
    }
    possible_behaviors.insert(IdleBehavior::DISPLAY_ON);
    possible_behaviors.insert(IdleBehavior::DISPLAY_OFF);
    // Set the current default option based on the current screen off
    // delay.
    current_idle_behavior = prefs_->GetInteger(screen_off_delay_pref) > 0
                                ? IdleBehavior::DISPLAY_OFF
                                : IdleBehavior::DISPLAY_ON;
    return IdleBehaviorInfo(possible_behaviors, current_idle_behavior,
                            IsIdleManaged(power_source));
  }

  // Looks like we did not find enterprise policy restricitng the idle
  // options. So add all three idle options to what user can select
  // from.
  possible_behaviors.insert(IdleBehavior::DISPLAY_ON);
  possible_behaviors.insert(IdleBehavior::DISPLAY_OFF);
  possible_behaviors.insert(IdleBehavior::DISPLAY_OFF_SLEEP);

  // Infer the idle behavior based on the current idle action
  // (determining whether we'll sleep eventually or not) and the AC
  // screen-off delay.
  const PowerPolicyController::Action idle_action =
      static_cast<PowerPolicyController::Action>(prefs_->GetInteger(idle_pref));

  if (idle_action == PowerPolicyController::ACTION_SUSPEND) {
    current_idle_behavior = IdleBehavior::DISPLAY_OFF_SLEEP;
  } else if (idle_action == PowerPolicyController::ACTION_DO_NOTHING) {
    current_idle_behavior = (prefs_->GetInteger(screen_off_delay_pref) > 0
                                 ? IdleBehavior::DISPLAY_OFF
                                 : IdleBehavior::DISPLAY_ON);
  } else {
    NOTREACHED_IN_MIGRATION()
        << "Idle behavior is set to a enterprise-only value, but "
        << "the setting is not enterprise managed. Defaulting to "
        << "DISPLAY_OFF_SLEEP behavior.";
    current_idle_behavior = IdleBehavior::DISPLAY_OFF_SLEEP;
  }

  return IdleBehaviorInfo(possible_behaviors, current_idle_behavior,
                          IsIdleManaged(power_source));
}

bool PowerHandler::IsIdleManaged(PowerSource power_source) {
  switch (power_source) {
    case PowerSource::kAc:
      return prefs_->IsManagedPreference(ash::prefs::kPowerAcIdleAction) ||
             prefs_->IsManagedPreference(
                 ash::prefs::kPowerAcScreenDimDelayMs) ||
             prefs_->IsManagedPreference(
                 ash::prefs::kPowerAcScreenOffDelayMs) ||
             prefs_->IsManagedPreference(ash::prefs::kPowerAcScreenLockDelayMs);
    case PowerSource::kBattery:
      return prefs_->IsManagedPreference(ash::prefs::kPowerBatteryIdleAction) ||
             prefs_->IsManagedPreference(
                 ash::prefs::kPowerBatteryScreenDimDelayMs) ||
             prefs_->IsManagedPreference(
                 ash::prefs::kPowerBatteryScreenOffDelayMs) ||
             prefs_->IsManagedPreference(
                 ash::prefs::kPowerBatteryScreenLockDelayMs);
  }
}

}  // namespace ash::settings
