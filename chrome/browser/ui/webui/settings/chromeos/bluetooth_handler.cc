// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/bluetooth_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/network/network_event_log.h"
#include "content/public/browser/web_ui.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace chromeos {
namespace {

const char kIsDeviceBlockedByPolicy[] = "isDeviceBlockedByPolicy";

}  // namespace

namespace settings {

BluetoothHandler::BluetoothHandler() {
  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&BluetoothHandler::BluetoothDeviceAdapterReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

BluetoothHandler::~BluetoothHandler() {}

void BluetoothHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kIsDeviceBlockedByPolicy,
      base::BindRepeating(&BluetoothHandler::HandleIsDeviceBlockedByPolicy,
                          base::Unretained(this)));
}

void BluetoothHandler::OnJavascriptAllowed() {}

void BluetoothHandler::OnJavascriptDisallowed() {}

void BluetoothHandler::BluetoothDeviceAdapterReady(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK(adapter);
  bluetooth_adapter_ = std::move(adapter);
}

void BluetoothHandler::HandleIsDeviceBlockedByPolicy(
    const base::ListValue* args) {
  AllowJavascript();
  std::string callback_id;
  std::string address;
  CHECK_EQ(2U, args->GetSize());
  CHECK(args->GetString(0, &callback_id));
  CHECK(args->GetString(1, &address));

  if (!bluetooth_adapter_) {
    BLUETOOTH_LOG(EVENT) << "Bluetooth adapter not available.";
    ResolveJavascriptCallback(base::Value(callback_id), base::Value(false));
    return;
  }

  device::BluetoothDevice* device = bluetooth_adapter_->GetDevice(address);
  if (!device) {
    BLUETOOTH_LOG(EVENT) << "No device found for address.";
    ResolveJavascriptCallback(base::Value(callback_id), base::Value(false));
    return;
  }

  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(device->IsBlockedByPolicy()));
}

}  // namespace settings
}  // namespace chromeos
