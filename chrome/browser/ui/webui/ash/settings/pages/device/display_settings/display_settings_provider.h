// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_DISPLAY_SETTINGS_DISPLAY_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_DISPLAY_SETTINGS_DISPLAY_SETTINGS_PROVIDER_H_

#include <map>

#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "ash/system/brightness_control_delegate.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "base/types/id_type.h"
#include "chrome/browser/ui/webui/ash/settings/pages/device/display_settings/display_settings_provider.mojom.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/display_manager_observer.h"

namespace ash::settings {

using DisplayId = base::IdType64<display::Display>;

// Provide information about system display settings. Implemented in the browser
// process. Called by the OS settings app.
class DisplaySettingsProvider : public mojom::DisplaySettingsProvider,
                                public TabletModeObserver,
                                public display::DisplayManagerObserver,
                                public ash::ShellObserver,
                                public display::DisplayObserver,
                                public chromeos::PowerManagerClient::Observer {
 public:
  DisplaySettingsProvider();
  ~DisplaySettingsProvider() override;
  DisplaySettingsProvider(const DisplaySettingsProvider& other) = delete;
  DisplaySettingsProvider& operator=(const DisplaySettingsProvider& other) =
      delete;

  // Note that these values are persisted to histograms so existing values
  // should remain unchanged and new values should be added to the end.
  enum class DisplayType {
    kExternalDisplay = 0,
    kInternalDisplay = 1,
    kMaxValue = kInternalDisplay,
  };

  // Enum value for measuring display default settings performance.
  // Note that these values are persisted to histograms so existing values
  // should remain unchanged and new values should be added to the end.
  enum class DisplayDefaultSettingsMeasurement {
    kNewDisplayConnected = 0,
    kOverrideResolution = 1,
    kOverrideScaling = 2,
    kMaxValue = kOverrideScaling,
  };

  // Note that these values are persisted to histograms so existing values
  // should remain unchanged and new values should be added to the end.
  enum class UserInitiatedDisplayAmbientLightSensorDisabledCause {
    // The ambient light sensor was disabled directly through the settings
    // app by the user.
    kUserRequestSettingsApp = 0,
    // The ambient light sensor was disabled as a result of the user manually
    // adjusting the brightness.
    kBrightnessUserRequest = 1,
    // The ambient light sensor was disabled as a result of the user adjusting
    // the brightness through the settings app.
    kBrightnessUserRequestSettingsApp = 2,
    kMaxValue = kBrightnessUserRequestSettingsApp,
  };

  // The UMA histogram that records display settings usage.
  static constexpr char kDisplaySettingsHistogramName[] =
      "ChromeOS.Settings.Display";
  // The UMA histogram that records new display connected metrics.
  static constexpr char kNewDisplayConnectedHistogram[] =
      "ChromeOS.Settings.Display.NewDisplayConnected";

  // Records when user overrides the display resolution or scaling within an
  // hour of the display being connected for the first time.
  static constexpr char kUserOverrideInternalDisplayDefaultSettingsHistogram[] =
      "ChromeOS.Settings.Display.Internal.UserOverrideDisplayDefaultSettings";
  static constexpr char kUserOverrideExternalDisplayDefaultSettingsHistogram[] =
      "ChromeOS.Settings.Display.External.UserOverrideDisplayDefaultSettings";

  void BindInterface(
      mojo::PendingReceiver<mojom::DisplaySettingsProvider> receiver);

  // mojom::DisplaySettingsProvider:
  void ObserveTabletMode(
      mojo::PendingRemote<mojom::TabletModeObserver> observer,
      ObserveTabletModeCallback callback) override;

  void ObserveDisplayConfiguration(
      mojo::PendingRemote<mojom::DisplayConfigurationObserver> observer)
      override;

  void ObserveDisplayBrightnessSettings(
      mojo::PendingRemote<mojom::DisplayBrightnessSettingsObserver> observer,
      ObserveDisplayBrightnessSettingsCallback callback) override;

  void ObserveAmbientLightSensor(
      mojo::PendingRemote<mojom::AmbientLightSensorObserver> observer,
      ObserveAmbientLightSensorCallback callback) override;

  void RecordChangingDisplaySettings(
      mojom::DisplaySettingsType type,
      mojom::DisplaySettingsValuePtr value) override;

  void SetShinyPerformance(bool enabled) override;

  void SetInternalDisplayScreenBrightness(double percent) override;

  void SetInternalDisplayAmbientLightSensorEnabled(bool enabled) override;

  void HasAmbientLightSensor(HasAmbientLightSensorCallback callback) override;

  void StartNativeTouchscreenMappingExperience() override;

  // TabletModeObserver:
  void OnTabletModeEventsBlockingChanged() override;

  // display::DisplayManagerObserver:
  void OnDidProcessDisplayChanges(
      const DisplayConfigurationChange& configuration_change) override;

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;

  // PowerManagerClient::Observer:
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;
  void AmbientLightSensorEnabledChanged(
      const power_manager::AmbientLightSensorChange& change) override;

  // ash::ShellObserver:
  void OnShellDestroying() override;

  void SetBrightnessControlDelegateForTesting(
      raw_ptr<BrightnessControlDelegate> delegate) {
    brightness_control_delegate_ = delegate;
  }

 private:
  void OnGetInitialBrightness(ObserveDisplayBrightnessSettingsCallback callback,
                              std::optional<double> percent);

  void OnGetAmbientLightSensorEnabled(
      ObserveAmbientLightSensorCallback callback,
      std::optional<bool> is_ambient_light_sensor_enabled);

  void OnGetHasAmbientLightSensor(HasAmbientLightSensorCallback callback,
                                  std::optional<bool> has_ambient_light_sensor);

  void RecordBrightnessSliderAdjusted();

  base::ScopedObservation<ash::Shell, ash::ShellObserver> shell_observation_{
      this};

  // Maintain a reference to BrightnessControlDelegate so that we can test
  // behavior of methods in this class that interact with it.
  raw_ptr<BrightnessControlDelegate> brightness_control_delegate_;

  mojo::RemoteSet<mojom::TabletModeObserver> tablet_mode_observers_;

  mojo::RemoteSet<mojom::DisplayConfigurationObserver>
      display_configuration_observers_;

  mojo::RemoteSet<mojom::DisplayBrightnessSettingsObserver>
      display_brightness_settings_observers_;

  mojo::RemoteSet<mojom::AmbientLightSensorObserver>
      ambient_light_sensor_observers_;

  // A map between display id and the timestamp this display is connected. Only
  // add those displays that are connected for the first time. Used to record
  // the time elapsed between users changing the display default settings and
  // the display is connected.
  std::map<DisplayId, base::TimeTicks> displays_connection_timestamp_map_;

  // The last display brightness percentage set by the user. Used for metrics.
  double last_set_brightness_percent_;

  // Times used to prevent the brightness slider metrics from recording each
  // time the user moves the slider while setting the desired brightness.
  base::DelayTimer brightness_slider_metric_delay_timer_;

  mojo::Receiver<mojom::DisplaySettingsProvider> receiver_{this};

  base::WeakPtrFactory<DisplaySettingsProvider> weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_DISPLAY_SETTINGS_DISPLAY_SETTINGS_PROVIDER_H_
