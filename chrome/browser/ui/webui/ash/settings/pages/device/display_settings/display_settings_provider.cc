// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/device/display_settings/display_settings_provider.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/display/cros_display_config.h"
#include "ash/display/display_performance_mode_controller.h"
#include "ash/display/display_prefs.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "ash/system/brightness_control_delegate.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/webui/ash/settings/pages/device/display_settings/display_settings_provider.mojom.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "content/public/browser/browser_thread.h"
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

// The time threshold whether user override display settings metrics would be
// fired or not.
constexpr int kUserOverrideDisplaySettingsTimeThresholdInMinute = 60;

// The interval of the timer that records the brightness slider adjusted event.
// Multiple changes to the brightness percentage will not be recorded until
// after this interval elapses.
constexpr base::TimeDelta kMetricsDelayTimerInterval = base::Seconds(2);

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

void RecordUserInitiatedDisplayAmbientLightSensorDisabledCause(
    const power_manager::AmbientLightSensorChange_Cause& cause) {
  using DisplayALSDisabledCause = DisplaySettingsProvider::
      UserInitiatedDisplayAmbientLightSensorDisabledCause;
  DisplayALSDisabledCause disabled_cause;

  switch (cause) {
    case power_manager::
        AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP:
      disabled_cause = DisplayALSDisabledCause::kUserRequestSettingsApp;
      break;
    case power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST:
      disabled_cause = DisplayALSDisabledCause::kBrightnessUserRequest;
      break;
    case power_manager::
        AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST_SETTINGS_APP:
      disabled_cause =
          DisplayALSDisabledCause::kBrightnessUserRequestSettingsApp;
      break;
    default:
      return;  // Exit function if none of the specified cases match
  }

  base::UmaHistogramEnumeration(
      base::StrCat({DisplaySettingsProvider::kDisplaySettingsHistogramName,
                    ".Internal.UserInitiated.AmbientLightSensorDisabledCause"}),
      disabled_cause);
}

}  // namespace

DisplaySettingsProvider::DisplaySettingsProvider()
    : brightness_slider_metric_delay_timer_(
          FROM_HERE,
          kMetricsDelayTimerInterval,
          this,
          &DisplaySettingsProvider::RecordBrightnessSliderAdjusted) {
  if (Shell::HasInstance()) {
    shell_observation_.Observe(ash::Shell::Get());
  }

  if (Shell::HasInstance() && Shell::Get()->brightness_control_delegate()) {
    brightness_control_delegate_ = Shell::Get()->brightness_control_delegate();
  } else {
    LOG(WARNING) << "DisplaySettingsProvider: Shell not available, did not "
                    "save BrightnessControlDelegate.";
  }

  if (TabletMode::Get()) {
    TabletMode::Get()->AddObserver(this);
  }
  if (Shell::HasInstance() && Shell::Get()->display_manager()) {
    Shell::Get()->display_manager()->AddDisplayManagerObserver(this);
    Shell::Get()->display_manager()->AddDisplayObserver(this);
  }
  if (features::IsBrightnessControlInSettingsEnabled()) {
    chromeos::PowerManagerClient* power_manager_client =
        chromeos::PowerManagerClient::Get();
    if (power_manager_client) {
      power_manager_client->AddObserver(this);
    }
  }
}

DisplaySettingsProvider::~DisplaySettingsProvider() {
  if (TabletMode::Get()) {
    TabletMode::Get()->RemoveObserver(this);
  }
  if (Shell::HasInstance() && Shell::Get()->display_manager()) {
    Shell::Get()->display_manager()->RemoveDisplayManagerObserver(this);
    Shell::Get()->display_manager()->RemoveDisplayObserver(this);
  }
  if (features::IsBrightnessControlInSettingsEnabled()) {
    chromeos::PowerManagerClient* power_manager_client =
        chromeos::PowerManagerClient::Get();
    if (power_manager_client) {
      power_manager_client->RemoveObserver(this);
    }
  }
}

void DisplaySettingsProvider::OnShellDestroying() {
  // Explicitly nullify the pointer to BrightnessControlDelegate before it's
  // destroyed to avoid a dangling pointer.
  brightness_control_delegate_ = nullptr;

  shell_observation_.Reset();
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

void DisplaySettingsProvider::OnGetInitialBrightness(
    ObserveDisplayBrightnessSettingsCallback callback,
    std::optional<double> percent) {
  if (!percent.has_value()) {
    LOG(ERROR) << "GetBrightnessPercent returned nullopt.";
    // In the rare case that GetInitialBrightness returns a nullopt, set the
    // brightness slider to the middle.
    std::move(callback).Run(50.0);
    return;
  }
  std::move(callback).Run(percent.value());
}

void DisplaySettingsProvider::ObserveDisplayBrightnessSettings(
    mojo::PendingRemote<mojom::DisplayBrightnessSettingsObserver> observer,
    ObserveDisplayBrightnessSettingsCallback callback) {
  if (!brightness_control_delegate_) {
    LOG(ERROR) << "DisplaySettingsProvider: Expected BrightnessControlDelegate "
                  "to be non-null when adding an observer.";
    return;
  }

  display_brightness_settings_observers_.Add(std::move(observer));

  // Get the current screen brightness and run the callback with that value.
  brightness_control_delegate_->GetBrightnessPercent(
      base::BindOnce(&DisplaySettingsProvider::OnGetInitialBrightness,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DisplaySettingsProvider::OnGetAmbientLightSensorEnabled(
    ObserveAmbientLightSensorCallback callback,
    std::optional<bool> is_ambient_light_sensor_enabled) {
  if (!is_ambient_light_sensor_enabled.has_value()) {
    LOG(ERROR) << "GetAmbientLightSensorEnabled returned nullopt.";
    // In the rare case that GetAmbientLightSensorEnabled returns a nullopt,
    // assume that the ambient light sensor is enabled, because by default the
    // ambient light sensor is re-enabled at system start.
    std::move(callback).Run(true);
    return;
  }
  std::move(callback).Run(is_ambient_light_sensor_enabled.value());
}

void DisplaySettingsProvider::ObserveAmbientLightSensor(
    mojo::PendingRemote<mojom::AmbientLightSensorObserver> observer,
    ObserveAmbientLightSensorCallback callback) {
  if (!brightness_control_delegate_) {
    LOG(ERROR) << "DisplaySettingsProvider: Expected BrightnessControlDelegate "
                  "to be non-null when adding an observer.";
    return;
  }

  ambient_light_sensor_observers_.Add(std::move(observer));

  // Get whether the ambient light sensor is currently enabled and run the
  // callback with that value.
  brightness_control_delegate_->GetAmbientLightSensorEnabled(
      base::BindOnce(&DisplaySettingsProvider::OnGetAmbientLightSensorEnabled,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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

    base::UmaHistogramEnumeration(
        new_display.IsInternal()
            ? kUserOverrideInternalDisplayDefaultSettingsHistogram
            : kUserOverrideExternalDisplayDefaultSettingsHistogram,
        DisplayDefaultSettingsMeasurement::kNewDisplayConnected);
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

      if (time_delta <= kUserOverrideDisplaySettingsTimeThresholdInMinute) {
        base::UmaHistogramEnumeration(
            is_internal_display.value()
                ? kUserOverrideInternalDisplayDefaultSettingsHistogram
                : kUserOverrideExternalDisplayDefaultSettingsHistogram,
            type == mojom::DisplaySettingsType::kResolution
                ? DisplayDefaultSettingsMeasurement::kOverrideResolution
                : DisplayDefaultSettingsMeasurement::kOverrideScaling);
      }

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

void DisplaySettingsProvider::ScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  for (auto& observer : display_brightness_settings_observers_) {
    bool triggered_by_als =
        change.cause() ==
        power_manager::BacklightBrightnessChange_Cause_AMBIENT_LIGHT_CHANGED;
    observer->OnDisplayBrightnessChanged(change.percent(), triggered_by_als);
  }
}

void DisplaySettingsProvider::AmbientLightSensorEnabledChanged(
    const power_manager::AmbientLightSensorChange& change) {
  for (auto& observer : ambient_light_sensor_observers_) {
    observer->OnAmbientLightSensorEnabledChanged(change.sensor_enabled());
  }

  if (!change.sensor_enabled()) {
    RecordUserInitiatedDisplayAmbientLightSensorDisabledCause(change.cause());
  }
}

void DisplaySettingsProvider::SetInternalDisplayScreenBrightness(
    double percent) {
  if (!features::IsBrightnessControlInSettingsEnabled()) {
    return;
  }

  if (!brightness_control_delegate_) {
    LOG(ERROR) << "DisplaySettingsProvider: Expected BrightnessControlDelegate "
                  "to be non-null when setting the internal display screen "
                  "brightness.";
    return;
  }

  brightness_control_delegate_->SetBrightnessPercent(
      percent, /*gradual=*/true, /*source=*/
      BrightnessControlDelegate::BrightnessChangeSource::kSettingsApp);

  last_set_brightness_percent_ = percent;
  // Start or reset timer for recording to metrics.
  brightness_slider_metric_delay_timer_.Reset();
}

void DisplaySettingsProvider::RecordBrightnessSliderAdjusted() {
  // Record the brightness change event.
  std::string histogram_name(base::StrCat(
      {kDisplaySettingsHistogramName, ".Internal.BrightnessSliderAdjusted"}));
  base::UmaHistogramPercentage(histogram_name, last_set_brightness_percent_);
}

void DisplaySettingsProvider::SetInternalDisplayAmbientLightSensorEnabled(
    bool enabled) {
  if (!features::IsBrightnessControlInSettingsEnabled()) {
    return;
  }

  brightness_control_delegate_->SetAmbientLightSensorEnabled(
      enabled, BrightnessControlDelegate::
                   AmbientLightSensorEnabledChangeSource::kSettingsApp);

  // Record the auto-brightness toggle event.
  std::string histogram_name(base::StrCat(
      {kDisplaySettingsHistogramName, ".Internal.AutoBrightnessEnabled"}));
  base::UmaHistogramBoolean(histogram_name, /*sample=*/enabled);
}

void DisplaySettingsProvider::HasAmbientLightSensor(
    HasAmbientLightSensorCallback callback) {
  brightness_control_delegate_->HasAmbientLightSensor(
      base::BindOnce(&DisplaySettingsProvider::OnGetHasAmbientLightSensor,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DisplaySettingsProvider::OnGetHasAmbientLightSensor(
    HasAmbientLightSensorCallback callback,
    std::optional<bool> has_ambient_light_sensor) {
  if (!has_ambient_light_sensor.has_value()) {
    LOG(ERROR) << "HasAmbientLightSensor returned nullopt.";
    // In the rare case that HasAmbientLightSensor returns a nullopt, assume
    // that the device does not have an ambient light sensor.
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(has_ambient_light_sensor.value());
}

void DisplaySettingsProvider::StartNativeTouchscreenMappingExperience() {
  // CrosDisplayConfig must be called from the UI thread, so post this task
  // instead of directly calling it.
  // base::Unretained is safe as Shell is deleted in PostMainMessageLoopRun
  // which means the cros_display_config object will always outlive the posted
  // task.
  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CrosDisplayConfig::TouchCalibration,
          base::Unretained(Shell::Get()->cros_display_config()), "",
          crosapi::mojom::DisplayConfigOperation::kShowNativeMappingDisplays,
          nullptr, base::DoNothing()));
}

}  // namespace ash::settings
