// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/eche_app_ui/system_info_provider.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chromeos/components/eche_app_ui/mojom/types_mojom_traits.h"
#include "chromeos/components/eche_app_ui/system_info.h"
#include "chromeos/components/multidevice/logging/logging.h"

namespace chromeos {
namespace eche_app {

const char kJsonDeviceNameKey[] = "device_name";
const char kJsonBoardNameKey[] = "board_name";

SystemInfoProvider::SystemInfoProvider(std::unique_ptr<SystemInfo> system_info)
    : system_info_(std::move(system_info)) {
  // TODO(samchiu): The intention of null check was for unit test. Add a fake
  // ScreenBacklight object to remove null check.
  if (ash::ScreenBacklight::Get())
    ash::ScreenBacklight::Get()->AddObserver(this);
  ash::TabletMode::Get()->AddObserver(this);
}

SystemInfoProvider::~SystemInfoProvider() {
  // Ash may be released before us.
  if (ash::ScreenBacklight::Get())
    ash::ScreenBacklight::Get()->RemoveObserver(this);
  if (ash::TabletMode::Get())
    ash::TabletMode::Get()->RemoveObserver(this);
}

void SystemInfoProvider::GetSystemInfo(
    base::OnceCallback<void(const std::string&)> callback) {
  base::DictionaryValue json_dictionary;
  json_dictionary.SetString(kJsonDeviceNameKey, system_info_->GetDeviceName());
  json_dictionary.SetString(kJsonBoardNameKey, system_info_->GetBoardName());
  std::string json_message;
  base::JSONWriter::Write(json_dictionary, &json_message);
  std::move(callback).Run(json_message);
}

void SystemInfoProvider::SetSystemInfoObserver(
    mojo::PendingRemote<mojom::SystemInfoObserver> observer) {
  observer_remote_.reset();
  observer_remote_.Bind(std::move(observer));
}

void SystemInfoProvider::Bind(
    mojo::PendingReceiver<mojom::SystemInfoProvider> receiver) {
  info_receiver_.reset();
  info_receiver_.Bind(std::move(receiver));
}

void SystemInfoProvider::OnScreenBacklightStateChanged(
    ash::ScreenBacklightState screen_state) {
  if (!observer_remote_.is_bound())
    return;

  observer_remote_->OnScreenBacklightStateChanged(screen_state);
}

void SystemInfoProvider::SetTabletModeChanged(bool enabled) {
  if (!observer_remote_.is_bound())
    return;

  PA_LOG(VERBOSE) << "OnReceivedTabletModeChanged:" << enabled;
  observer_remote_->OnReceivedTabletModeChanged(enabled);
}

// TabletModeObserver implementation:
void SystemInfoProvider::OnTabletModeStarted() {
  SetTabletModeChanged(true);
}

void SystemInfoProvider::OnTabletModeEnded() {
  SetTabletModeChanged(false);
}

}  // namespace eche_app
}  // namespace chromeos
