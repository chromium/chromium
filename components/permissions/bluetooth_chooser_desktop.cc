// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/bluetooth_chooser_desktop.h"

#include "components/permissions/bluetooth_chooser_controller.h"

namespace permissions {

BluetoothChooserDesktop::BluetoothChooserDesktop(
    std::unique_ptr<permissions::BluetoothChooserController> controller,
    base::OnceCallback<
        base::OnceClosure(std::unique_ptr<permissions::ChooserController>)>
        show_dialog_callback) {
  bluetooth_chooser_controller_ = controller->GetWeakPtr();
  close_closure_runner_.ReplaceClosure(
      std::move(show_dialog_callback).Run(std::move(controller)));
}

BluetoothChooserDesktop::~BluetoothChooserDesktop() {
  // This satisfies the WebContentsDelegate::RunBluetoothChooser() requirement
  // that the EventHandler can be destroyed any time after the BluetoothChooser
  // instance.
  if (bluetooth_chooser_controller_)
    bluetooth_chooser_controller_->ResetEventHandler();
}

void BluetoothChooserDesktop::SetAdapterPresence(AdapterPresence presence) {
  if (bluetooth_chooser_controller_)
    bluetooth_chooser_controller_->OnAdapterPresenceChanged(presence);
}

void BluetoothChooserDesktop::ShowDiscoveryState(DiscoveryState state) {
  if (bluetooth_chooser_controller_)
    bluetooth_chooser_controller_->OnDiscoveryStateChanged(state);
}

void BluetoothChooserDesktop::AddOrUpdateDevice(
    const std::string& device_id,
    bool should_update_name,
    const std::u16string& device_name,
    bool is_gatt_connected,
    bool is_paired,
    int signal_strength_level) {
  if (bluetooth_chooser_controller_) {
    bluetooth_chooser_controller_->AddOrUpdateDevice(
        device_id, should_update_name, device_name, is_gatt_connected,
        is_paired, signal_strength_level);
  }
}

}  // namespace permissions
