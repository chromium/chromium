// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/bluetooth/cast_bluetooth_chooser.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromecast {

CastBluetoothChooser::CastBluetoothChooser(
    content::BluetoothChooser::EventHandler event_handler,
    mojo::PendingRemote<mojom::BluetoothDeviceAccessProvider> pending_provider)
    : event_handler_(std::move(event_handler)) {
  DCHECK(event_handler_);
  mojo::Remote<mojom::BluetoothDeviceAccessProvider> provider(
      std::move(pending_provider));
  provider->RequestDeviceAccess(receiver_.BindNewPipeAndPassRemote());
  receiver_.set_disconnect_handler(base::BindOnce(
      &CastBluetoothChooser::OnClientConnectionError, base::Unretained(this)));
}

CastBluetoothChooser::~CastBluetoothChooser() = default;

void CastBluetoothChooser::GrantAccess(const std::string& address) {
  DCHECK(event_handler_);

  if (all_devices_approved_) {
    LOG(WARNING) << __func__ << " called after access granted to all devices!";
    return;
  }

  if (base::Contains(available_devices_, address)) {
    RunEventHandlerAndResetReceiver(content::BluetoothChooserEvent::SELECTED,
                                    address);
    return;
  }
  approved_devices_.insert(address);
}

void CastBluetoothChooser::GrantAccessToAllDevices() {
  DCHECK(event_handler_);

  all_devices_approved_ = true;
  if (!available_devices_.empty()) {
    RunEventHandlerAndResetReceiver(content::BluetoothChooserEvent::SELECTED,
                                    *available_devices_.begin());
  }
}

void CastBluetoothChooser::AddOrUpdateDevice(const std::string& device_id,
                                             bool should_update_name,
                                             const std::u16string& device_name,
                                             bool is_gatt_connected,
                                             bool is_paired,
                                             int signal_strength_level) {
  DCHECK(event_handler_);

  // Note: |device_id| is just a canonical Bluetooth address.
  if (all_devices_approved_ || base::Contains(approved_devices_, device_id)) {
    RunEventHandlerAndResetReceiver(content::BluetoothChooserEvent::SELECTED,
                                    device_id);
    return;
  }
  available_devices_.insert(device_id);
}

void CastBluetoothChooser::RunEventHandlerAndResetReceiver(
    content::BluetoothChooserEvent event,
    std::string address) {
  DCHECK(event_handler_);
  std::move(event_handler_).Run(event, std::move(address));
  receiver_.reset();
}

void CastBluetoothChooser::OnClientConnectionError() {
  // If the DeviceAccessProvider has granted access to all devices, it may
  // tear down the client immediately. In this case, do not run the event
  // handler, as we may have not had the opportunity to select a device.
  if (!all_devices_approved_ && event_handler_) {
    RunEventHandlerAndResetReceiver(content::BluetoothChooserEvent::CANCELLED,
                                    "");
  }
}

}  // namespace chromecast
