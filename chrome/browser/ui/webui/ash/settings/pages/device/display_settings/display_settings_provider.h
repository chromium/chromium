// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_DISPLAY_SETTINGS_DISPLAY_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_DISPLAY_SETTINGS_DISPLAY_SETTINGS_PROVIDER_H_

#include <map>

#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/types/id_type.h"
#include "chrome/browser/ui/webui/ash/settings/pages/device/display_settings/display_settings_provider.mojom.h"
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
                                public display::DisplayObserver {
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

  // The UMA histogram that records display settings usage.
  static constexpr char kDisplaySettingsHistogramName[] =
      "ChromeOS.Settings.Display";
  // The UMA histogram that records new display connected metrics.
  static constexpr char kNewDisplayConnectedHistogram[] =
      "ChromeOS.Settings.Display.NewDisplayConnected";

  void BindInterface(
      mojo::PendingReceiver<mojom::DisplaySettingsProvider> receiver);

  // mojom::DisplaySettingsProvider:
  void ObserveTabletMode(
      mojo::PendingRemote<mojom::TabletModeObserver> observer,
      ObserveTabletModeCallback callback) override;

  void ObserveDisplayConfiguration(
      mojo::PendingRemote<mojom::DisplayConfigurationObserver> observer)
      override;

  void RecordChangingDisplaySettings(
      mojom::DisplaySettingsType type,
      mojom::DisplaySettingsValuePtr value) override;

  // TabletModeObserver:
  void OnTabletModeEventsBlockingChanged() override;

  // display::DisplayManagerObserver:
  void OnDidProcessDisplayChanges(
      const DisplayConfigurationChange& configuration_change) override;

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;

 private:
  mojo::RemoteSet<mojom::TabletModeObserver> tablet_mode_observers_;

  mojo::RemoteSet<mojom::DisplayConfigurationObserver>
      display_configuration_observers_;

  // A map between display id and the timestamp this display is connected. Only
  // add those displays that are connected for the first time. Used to record
  // the time elapsed between users changing the display default settings and
  // the display is connected.
  std::map<DisplayId, base::TimeTicks> displays_connection_timestamp_map_;

  mojo::Receiver<mojom::DisplaySettingsProvider> receiver_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_DISPLAY_SETTINGS_DISPLAY_SETTINGS_PROVIDER_H_
