// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bluetooth/bluetooth_chooser_desktop.h"

#include "base/check.h"
#include "chrome/browser/ui/bluetooth/bluetooth_chooser_controller.h"
#include "chrome/browser/ui/browser_dialogs.h"

BluetoothChooserDesktop::BluetoothChooserDesktop(
    content::RenderFrameHost* frame,
    const content::BluetoothChooser::EventHandler& event_handler) {
  auto controller =
      std::make_unique<BluetoothChooserController>(frame, event_handler);
  bluetooth_chooser_controller_ = controller->GetWeakPtr();
  close_closure_ =
      chrome::ShowDeviceChooserDialog(frame, std::move(controller));
}

BluetoothChooserDesktop::~BluetoothChooserDesktop() {
  // This satisfies the WebContentsDelegate::RunBluetoothChooser() requirement
  // that the EventHandler can be destroyed any time after the BluetoothChooser
  // instance.
  if (bluetooth_chooser_controller_)
    bluetooth_chooser_controller_->ResetEventHandler();
  if (close_closure_)
    std::move(close_closure_).Run();
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
