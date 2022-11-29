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

namespace ash::settings {

namespace {

const char kRequestFastPairDeviceSupport[] =
    "requestFastPairDeviceSupportStatus";

}  // namespace

BluetoothHandler::BluetoothHandler() {
  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&BluetoothHandler::BluetoothDeviceAdapterReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

BluetoothHandler::~BluetoothHandler() {}

void BluetoothHandler::RegisterMessages() {
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

void BluetoothHandler::HandleRequestFastPairDeviceSupport(
    const base::Value::List& args) {
  AllowJavascript();

  base::Value is_supported(quick_pair::IsFastPairSupported(bluetooth_adapter_));
  FireWebUIListener("fast-pair-device-supported-status", is_supported);
}

}  // namespace ash::settings
