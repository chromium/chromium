// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/eche_app_ui/system_info_provider.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chromeos/components/eche_app_ui/mojom/types_mojom_traits.h"
#include "chromeos/components/eche_app_ui/system_info.h"

namespace chromeos {
namespace eche_app {

const char kJsonDeviceNameKey[] = "device_name";
const char kJsonBoardNameKey[] = "board_name";

SystemInfoProvider::SystemInfoProvider(std::unique_ptr<SystemInfo> system_info)
    : system_info_(std::move(system_info)) {
  if (ash::ScreenBacklight::Get())
    ash::ScreenBacklight::Get()->AddObserver(this);
}

SystemInfoProvider::~SystemInfoProvider() {
  if (ash::ScreenBacklight::Get())
    ash::ScreenBacklight::Get()->RemoveObserver(this);
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

void SystemInfoProvider::OnScreenStateChanged(ash::ScreenState screen_state) {
  if (!observer_remote_.is_bound())
    return;

  observer_remote_->OnScreenBacklightStateChanged(screen_state);
}

}  // namespace eche_app
}  // namespace chromeos
