// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/device/display_settings/display_settings_provider.h"

#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "chrome/browser/ui/webui/ash/settings/pages/device/display_settings/display_settings_provider.mojom.h"
#include "ui/display/manager/display_manager.h"

namespace ash::settings {

DisplaySettingsProvider::DisplaySettingsProvider() {
  if (TabletMode::Get()) {
    TabletMode::Get()->AddObserver(this);
  }
  if (Shell::HasInstance() && Shell::Get()->display_manager()) {
    Shell::Get()->display_manager()->AddObserver(this);
  }
}

DisplaySettingsProvider::~DisplaySettingsProvider() {
  if (TabletMode::Get()) {
    TabletMode::Get()->RemoveObserver(this);
  }
  if (Shell::HasInstance() && Shell::Get()->display_manager()) {
    Shell::Get()->display_manager()->RemoveObserver(this);
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

}  // namespace ash::settings
