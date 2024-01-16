// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/power/power_section.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kDeviceSectionPath;
using ::chromeos::settings::mojom::kPowerSubpagePath;
using ::chromeos::settings::mojom::kSystemPreferencesSectionPath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

const std::vector<SearchConcept>& GetDefaultSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_POWER,
       mojom::kPowerSubpagePath,
       mojom::SearchResultIcon::kPower,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kPower}},
      {IDS_OS_SETTINGS_TAG_POWER_IDLE_WHILE_CHARGING,
       mojom::kPowerSubpagePath,
       mojom::SearchResultIcon::kPower,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPowerIdleBehaviorWhileCharging},
       {IDS_OS_SETTINGS_TAG_POWER_IDLE_WHILE_CHARGING_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_POWER_IDLE_WHILE_ON_BATTERY,
       mojom::kPowerSubpagePath,
       mojom::SearchResultIcon::kPower,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPowerIdleBehaviorWhileOnBattery},
       {IDS_OS_SETTINGS_TAG_POWER_IDLE_WHILE_ON_BATTERY_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetPowerWithBatterySearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_POWER_SOURCE,
       mojom::kPowerSubpagePath,
       mojom::SearchResultIcon::kPower,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPowerSource},
       {IDS_OS_SETTINGS_TAG_POWER_SOURCE_ALT1,
        IDS_OS_SETTINGS_TAG_POWER_SOURCE_ALT2, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetPowerWithLaptopLidSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_POWER_SLEEP_COVER_CLOSED,
       mojom::kPowerSubpagePath,
       mojom::SearchResultIcon::kPower,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSleepWhenLaptopLidClosed},
       {IDS_OS_SETTINGS_TAG_POWER_SLEEP_COVER_CLOSED_ALT1,
        IDS_OS_SETTINGS_TAG_POWER_SLEEP_COVER_CLOSED_ALT2,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetPowerWithAdaptiveChargingSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_POWER_ADAPTIVE_CHARGING,
       mojom::kPowerSubpagePath,
       mojom::SearchResultIcon::kPower,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAdaptiveCharging}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetPowerWithBatterySaverModeSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_POWER_BATTERY_SAVER,
       mojom::kPowerSubpagePath,
       mojom::SearchResultIcon::kPower,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kBatterySaver}},
  });
  return *tags;
}

}  // namespace

PowerSection::PowerSection(Profile* profile,
                           SearchTagRegistry* search_tag_registry,
                           PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry),
      pref_service_(pref_service) {
  CHECK(profile);
  CHECK(search_tag_registry);
  CHECK(pref_service);

  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(GetDefaultSearchConcepts());

  chromeos::PowerManagerClient* power_manager_client =
      chromeos::PowerManagerClient::Get();
  if (power_manager_client) {
    power_manager_client->AddObserver(this);

    const std::optional<power_manager::PowerSupplyProperties>& last_status =
        power_manager_client->GetLastStatus();
    if (last_status) {
      PowerChanged(*last_status);
    }

    // Determine whether to show laptop lid power settings.
    power_manager_client->GetSwitchStates(base::BindOnce(
        &PowerSection::OnGotSwitchStates, weak_ptr_factory_.GetWeakPtr()));
  }
}

PowerSection::~PowerSection() {
  chromeos::PowerManagerClient* power_manager_client =
      chromeos::PowerManagerClient::Get();
  if (power_manager_client) {
    power_manager_client->RemoveObserver(this);
  }
}

void PowerSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  const bool kIsRevampEnabled =
      ash::features::IsOsSettingsRevampWayfindingEnabled();

  webui::LocalizedString kPowerStrings[] = {
      {"calculatingPower", IDS_SETTINGS_POWER_SOURCE_CALCULATING},
      {"powerAdaptiveChargingLabel",
       IDS_SETTINGS_POWER_ADAPTIVE_CHARGING_LABEL},
      {"powerAdaptiveChargingSubtext",
       IDS_SETTINGS_POWER_ADAPTIVE_CHARGING_SUBTEXT},
      {"powerIdleDisplayOff", IDS_SETTINGS_POWER_IDLE_DISPLAY_OFF},
      {"powerIdleDisplayOffSleep", IDS_SETTINGS_POWER_IDLE_DISPLAY_OFF_SLEEP},
      {"powerIdleDisplayOn", IDS_SETTINGS_POWER_IDLE_DISPLAY_ON},
      {"powerIdleDisplayShutDown", IDS_SETTINGS_POWER_IDLE_SHUT_DOWN},
      {"powerIdleDisplayStopSession", IDS_SETTINGS_POWER_IDLE_STOP_SESSION},
      {"powerIdleLabel", IDS_SETTINGS_POWER_IDLE_LABEL},
      {"powerIdleWhileChargingAriaLabel",
       IDS_SETTINGS_POWER_IDLE_WHILE_CHARGING_ARIA_LABEL},
      {"powerInactiveWhilePluggedInLabel",
       kIsRevampEnabled
           ? IDS_OS_SETTINGS_REVAMP_POWER_INACTIVE_WHILE_PLUGGED_IN_LABEL
           : IDS_SETTINGS_POWER_IDLE_WHILE_CHARGING_LABEL},
      {"powerIdleWhileOnBatteryAriaLabel",
       IDS_SETTINGS_POWER_IDLE_WHILE_ON_BATTERY_ARIA_LABEL},
      {"powerInactiveWhileOnBatteryLabel",
       kIsRevampEnabled
           ? IDS_OS_SETTINGS_REVAMP_POWER_INACTIVE_WHILE_ON_BATTERY_LABEL
           : IDS_SETTINGS_POWER_IDLE_WHILE_ON_BATTERY_LABEL},
      {"powerLidShutDownLabel", IDS_SETTINGS_POWER_LID_CLOSED_SHUT_DOWN_LABEL},
      {"powerLidSignOutLabel", IDS_SETTINGS_POWER_LID_CLOSED_SIGN_OUT_LABEL},
      {"powerLidSleepLabel", IDS_SETTINGS_POWER_LID_CLOSED_SLEEP_LABEL},
      {"powerSourceAcAdapter", IDS_SETTINGS_POWER_SOURCE_AC_ADAPTER},
      {"powerSourceBattery", IDS_SETTINGS_POWER_SOURCE_BATTERY},
      {"powerSourceLabel", IDS_SETTINGS_POWER_SOURCE_LABEL},
      {"powerSourceLowPowerCharger",
       IDS_SETTINGS_POWER_SOURCE_LOW_POWER_CHARGER},
      {"powerTitle", IDS_SETTINGS_POWER_TITLE},
      {"powerBatterySaverLabel", IDS_SETTINGS_POWER_BATTERY_SAVER_LABEL},
      {"powerBatterySaverSubtext", IDS_SETTINGS_POWER_BATTERY_SAVER_SUBTEXT},
  };
  html_source->AddLocalizedStrings(kPowerStrings);

  html_source->AddString(
      "powerAdaptiveChargingLearnMoreUrl",
      u"https://support.google.com/chromebook/?p=settings_adaptive_charging");

  html_source->AddString("powerBatterySaverLearnMoreUrl",
                         chrome::kCrosBatterySaverLearnMoreURL);

  html_source->AddBoolean("isAdaptiveChargingEnabled",
                          ash::features::IsAdaptiveChargingEnabled() &&
                              Shell::Get()
                                  ->adaptive_charging_controller()
                                  ->IsAdaptiveChargingSupported());
}

void PowerSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(std::make_unique<PowerHandler>(pref_service_));
}

int PowerSection::GetSectionNameMessageId() const {
  return IDS_SETTINGS_STORAGE_TITLE;
}

mojom::Section PowerSection::GetSection() const {
  // Note: This is a subsection that exists under Device or System Preferences.
  // This section will no longer exist under the Device section once the
  // OsSettingsRevampWayfinding feature is fully launched.
  // This is not a top-level section and does not have a respective declaration
  // in chromeos::settings::mojom::Section.
  return ash::features::IsOsSettingsRevampWayfindingEnabled()
             ? mojom::Section::kSystemPreferences
             : mojom::Section::kDevice;
}

mojom::SearchResultIcon PowerSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kPower;
}

const char* PowerSection::GetSectionPath() const {
  return ash::features::IsOsSettingsRevampWayfindingEnabled()
             ? mojom::kSystemPreferencesSectionPath
             : mojom::kDeviceSectionPath;
}

bool PowerSection::LogMetric(mojom::Setting setting, base::Value& value) const {
  // No metrics are logged.
  return false;
}

void PowerSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_POWER_TITLE, mojom::Subpage::kPower,
      mojom::SearchResultIcon::kPower, mojom::SearchResultDefaultRank::kMedium,
      mojom::kPowerSubpagePath);
  static constexpr mojom::Setting kPowerSettings[] = {
      mojom::Setting::kPowerIdleBehaviorWhileCharging,
      mojom::Setting::kPowerIdleBehaviorWhileOnBattery,
      mojom::Setting::kPowerSource,
      mojom::Setting::kSleepWhenLaptopLidClosed,
      mojom::Setting::kAdaptiveCharging,
      mojom::Setting::kBatterySaver,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kPower, kPowerSettings, generator);
}

void PowerSection::PowerChanged(
    const power_manager::PowerSupplyProperties& properties) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  if (properties.battery_state() !=
      power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT) {
    updater.AddSearchTags(GetPowerWithBatterySearchConcepts());
  }

  if (!has_observed_power_status_) {
    // IsAdaptiveChargingSupported and IsBatterySaverSupported both rely on
    // GetLastStatus, so make sure its not nullopt.
    DCHECK(chromeos::PowerManagerClient::Get()->GetLastStatus());
    if (!has_observed_power_status_) {
      if (ash::features::IsAdaptiveChargingEnabled() &&
          Shell::Get()
              ->adaptive_charging_controller()
              ->IsAdaptiveChargingSupported()) {
        updater.AddSearchTags(GetPowerWithAdaptiveChargingSearchConcepts());
      }

      const auto* battery_saver_controller =
          Shell::Get()->battery_saver_controller();
      if (battery_saver_controller != nullptr &&
          battery_saver_controller->IsBatterySaverSupported() &&
          ash::features::IsBatterySaverAvailable()) {
        updater.AddSearchTags(GetPowerWithBatterySaverModeSearchConcepts());
      }
    }
    has_observed_power_status_ = true;
  }
}

void PowerSection::OnGotSwitchStates(
    std::optional<chromeos::PowerManagerClient::SwitchStates> result) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  if (result && result->lid_state !=
                    chromeos::PowerManagerClient::LidState::NOT_PRESENT) {
    updater.AddSearchTags(GetPowerWithLaptopLidSearchConcepts());
  }
}

}  // namespace ash::settings
