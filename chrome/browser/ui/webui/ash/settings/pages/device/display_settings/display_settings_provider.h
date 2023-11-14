// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_DISPLAY_SETTINGS_DISPLAY_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_DISPLAY_SETTINGS_DISPLAY_SETTINGS_PROVIDER_H_

#include "ash/public/cpp/tablet_mode_observer.h"
#include "chrome/browser/ui/webui/ash/settings/pages/device/display_settings/display_settings_provider.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/display/manager/display_manager_observer.h"

namespace ash::settings {

// Provide information about system display settings. Implemented in the browser
// process. Called by the OS settings app.
class DisplaySettingsProvider : public mojom::DisplaySettingsProvider,
                                public TabletModeObserver,
                                public display::DisplayManagerObserver {
 public:
  DisplaySettingsProvider();
  ~DisplaySettingsProvider() override;
  DisplaySettingsProvider(const DisplaySettingsProvider& other) = delete;
  DisplaySettingsProvider& operator=(const DisplaySettingsProvider& other) =
      delete;

  void BindInterface(
      mojo::PendingReceiver<mojom::DisplaySettingsProvider> receiver);

  // mojom::DisplaySettingsProvider:
  void ObserveTabletMode(
      mojo::PendingRemote<mojom::TabletModeObserver> observer,
      ObserveTabletModeCallback callback) override;

  void ObserveDisplayConfiguration(
      mojo::PendingRemote<mojom::DisplayConfigurationObserver> observer)
      override;

  // TabletModeObserver:
  void OnTabletModeEventsBlockingChanged() override;

  // display::DisplayManagerObserver:
  void OnDidProcessDisplayChanges(
      const DisplayConfigurationChange& configuration_change) override;

 private:
  mojo::RemoteSet<mojom::TabletModeObserver> tablet_mode_observers_;

  mojo::RemoteSet<mojom::DisplayConfigurationObserver>
      display_configuration_observers_;

  mojo::Receiver<mojom::DisplaySettingsProvider> receiver_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_DISPLAY_SETTINGS_DISPLAY_SETTINGS_PROVIDER_H_
