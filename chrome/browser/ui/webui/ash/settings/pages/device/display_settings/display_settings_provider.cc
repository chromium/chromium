// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/device/display_settings/display_settings_provider.h"

#include <optional>

#include "ash/display/display_performance_mode_controller.h"
#include "ash/display/display_prefs.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/webui/ash/settings/pages/device/display_settings/display_settings_provider.mojom.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/display/util/display_util.h"

namespace ash::settings {

namespace {

// Minimum/maximum bucket value of user overriding display default settings.
constexpr int kMinTimeInMinuteOfUserOverrideDisplaySettings = 1;
constexpr int kMaxTimeInHourOfUserOverrideDisplaySettings = 8;

// The histogram bucket count of user overriding display default settings.
constexpr int kUserOverrideDisplaySettingsTimeDeltaBucketCount = 100;

// Get UMA histogram name that records the time elapsed between users changing
// the display settings and the display is connected.
const std::string GetUserOverrideDefaultSettingsHistogramName(
    mojom::DisplaySettingsType type,
    bool is_internal_display) {
  // Should only need to handle resolution and scaling, no other display
  // settings.
  CHECK(type == mojom::DisplaySettingsType::kResolution ||
        type == mojom::DisplaySettingsType::kScaling);

  std::string histogram_name(
      DisplaySettingsProvider::kDisplaySettingsHistogramName);
  histogram_name.append(is_internal_display ? ".Internal" : ".External");
  histogram_name.append(".UserOverrideDisplayDefaultSettingsTimeElapsed");
  histogram_name.append(type == mojom::DisplaySettingsType::kResolution
                            ? ".Resolution"
                            : ".Scaling");
  return histogram_name;
}

}  // namespace

DisplaySettingsProvider::DisplaySettingsProvider() {
  if (TabletMode::Get()) {
    TabletMode::Get()->AddObserver(this);
  }
  if (Shell::HasInstance() && Shell::Get()->display_manager()) {
    Shell::Get()->display_manager()->AddObserver(
        static_cast<display::DisplayManagerObserver*>(this));
    Shell::Get()->display_manager()->AddObserver(
        static_cast<display::DisplayObserver*>(this));
  }
}

DisplaySettingsProvider::~DisplaySettingsProvider() {
  if (TabletMode::Get()) {
    TabletMode::Get()->RemoveObserver(this);
  }
  if (Shell::HasInstance() && Shell::Get()->display_manager()) {
    Shell::Get()->display_manager()->RemoveObserver(
        static_cast<display::DisplayManagerObserver*>(this));
    Shell::Get()->display_manager()->RemoveObserver(
        static_cast<display::DisplayObserver*>(this));
  }
}

void DisplaySettingsProvider::BindInterface(
    mojo::PendingReceiver<mojom::DisplaySettingsProvider> pending_receiver) {
  if (receiver_.is_bound()) {
    receiver_.reset();
  }
  receiver_.Bind(std::move(pending_receiver));
}

void DisplaySettingsProvider::ObserveTabletMode(
    mojo::PendingRemote<mojom::TabletModeObserver> observer,
    ObserveTabletModeCallback callback) {
  tablet_mode_observers_.Add(std::move(observer));
  std::move(callback).Run(
      TabletMode::Get()->AreInternalInputDeviceEventsBlocked());
}

void DisplaySettingsProvider::OnTabletModeEventsBlockingChanged() {
  for (auto& observer : tablet_mode_observers_) {
    observer->OnTabletModeChanged(
        TabletMode::Get()->AreInternalInputDeviceEventsBlocked());
  }
}

void DisplaySettingsProvider::ObserveDisplayConfiguration(
    mojo::PendingRemote<mojom::DisplayConfigurationObserver> observer) {
  display_configuration_observers_.Add(std::move(observer));
}

void DisplaySettingsProvider::OnDidProcessDisplayChanges(
    const DisplayConfigurationChange& configuration_change) {
  for (auto& observer : display_configuration_observers_) {
    observer->OnDisplayConfigurationChanged();
  }
}

void DisplaySettingsProvider::OnDisplayAdded(
    const display::Display& new_display) {
  // Do not count new display connected when turning on unified desk mode.
  if (new_display.id() == display::kUnifiedDisplayId) {
    return;
  }

  // Check with prefs service to see if this display is firstly seen or was
  // saved to prefs before.
  if (Shell::HasInstance() && Shell::Get()->display_prefs() &&
      !Shell::Get()->display_prefs()->IsDisplayAvailableInPref(
          new_display.id())) {
    // Found display that is connected for the first time. Add the connection
    // timestamp.
    displays_connection_timestamp_map_[DisplayId(new_display.id())] =
        base::TimeTicks::Now();

    base::UmaHistogramEnumeration(kNewDisplayConnectedHistogram,
                                  display::IsInternalDisplayId(new_display.id())
                                      ? DisplayType::kInternalDisplay
                                      : DisplayType::kExternalDisplay);
  }
}

void DisplaySettingsProvider::RecordChangingDisplaySettings(
    mojom::DisplaySettingsType type,
    mojom::DisplaySettingsValuePtr value) {
  std::string histogram_name(kDisplaySettingsHistogramName);
  std::optional<bool> is_internal_display = value->is_internal_display;
  if (is_internal_display.has_value()) {
    histogram_name.append(is_internal_display.value() ? ".Internal"
                                                      : ".External");
  }
  base::UmaHistogramEnumeration(histogram_name, type);

  // Record settings change in details.
  if (type == mojom::DisplaySettingsType::kOrientation) {
    CHECK(value->orientation.has_value());
    histogram_name.append(".Orientation");
    base::UmaHistogramEnumeration(histogram_name, value->orientation.value());
  } else if (type == mojom::DisplaySettingsType::kNightLight) {
    CHECK(value->night_light_status.has_value());
    histogram_name.append(".NightLightStatus");
    base::UmaHistogramBoolean(histogram_name,
                              value->night_light_status.value());
  } else if (type == mojom::DisplaySettingsType::kNightLightSchedule) {
    CHECK(value->night_light_schedule.has_value());
    histogram_name.append(".NightLightSchedule");
    base::UmaHistogramEnumeration(histogram_name,
                                  value->night_light_schedule.value());
  } else if (type == mojom::DisplaySettingsType::kMirrorMode) {
    CHECK(value->mirror_mode_status.has_value());
    CHECK(!value->is_internal_display.has_value());
    histogram_name.append(".MirrorModeStatus");
    base::UmaHistogramBoolean(histogram_name,
                              value->mirror_mode_status.value());
  } else if (type == mojom::DisplaySettingsType::kUnifiedMode) {
    CHECK(value->unified_mode_status.has_value());
    CHECK(!value->is_internal_display.has_value());
    histogram_name.append(".UnifiedModeStatus");
    base::UmaHistogramBoolean(histogram_name,
                              value->unified_mode_status.value());
  }

  // Record default display settings performance metrics.
  if (value->display_id.has_value() &&
      (type == mojom::DisplaySettingsType::kResolution ||
       type == mojom::DisplaySettingsType::kScaling)) {
    const DisplayId id = DisplayId(value->display_id.value());
    auto it = displays_connection_timestamp_map_.find(id);
    if (it != displays_connection_timestamp_map_.end()) {
      int time_delta = (base::TimeTicks::Now() - it->second).InMinutes();
      const std::string override_histogram_name =
          GetUserOverrideDefaultSettingsHistogramName(
              type, is_internal_display.value());
      base::UmaHistogramCustomCounts(
          override_histogram_name, time_delta,
          kMinTimeInMinuteOfUserOverrideDisplaySettings,
          base::Hours(kMaxTimeInHourOfUserOverrideDisplaySettings).InMinutes(),
          kUserOverrideDisplaySettingsTimeDeltaBucketCount);

      // Once user has overridden the settings, remove it from the map to
      // prevent further recording, in which case, the user does not override
      // system default settings, but override previous user settings.
      displays_connection_timestamp_map_.erase(id);
    }
  }
}

void DisplaySettingsProvider::SetShinyPerformance(bool enabled) {
  // The provider could outlive the shell so check if it's still valid.
  if (!Shell::HasInstance() ||
      !Shell::Get()->display_performance_mode_controller()) {
    return;
  }

  Shell::Get()
      ->display_performance_mode_controller()
      ->SetHighPerformanceModeByUser(enabled);
}

}  // namespace ash::settings
