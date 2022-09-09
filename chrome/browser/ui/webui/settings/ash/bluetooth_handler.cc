// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/bluetooth_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/quick_pair/fast_pair_support_utils.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "content/public/browser/web_ui.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace chromeos {
namespace {

const char kIsDeviceBlockedByPolicy[] = "isDeviceBlockedByPolicy";
const char kRequestFastPairDeviceSupport[] =
    "requestFastPairDeviceSupportStatus";

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
  web_ui()->RegisterMessageCallback(
      kRequestFastPairDeviceSupport,
      base::BindRepeating(&BluetoothHandler::HandleRequestFastPairDeviceSupport,
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
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(2U, args.size());
  const std::string& callback_id = args[0].GetString();
  const std::string& address = args[1].GetString();

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

void BluetoothHandler::HandleRequestFastPairDeviceSupport(
    const base::Value::List& args) {
  AllowJavascript();

  base::Value is_supported(
      ash::quick_pair::IsFastPairSupported(bluetooth_adapter_));
  FireWebUIListener("fast-pair-device-supported-status", is_supported);
}

}  // namespace settings
}  // namespace chromeos
