// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform_v2/ble_medium.h"

namespace location {
namespace nearby {
namespace chrome {

BleMedium::BleMedium() = default;

BleMedium::~BleMedium() = default;

bool BleMedium::StartAdvertising(const std::string& service_id,
                                 const ByteArray& advertisement) {
  // TODO(b/154845685): Implement this method.
  NOTIMPLEMENTED();
  return false;
}

bool BleMedium::StopAdvertising(const std::string& service_id) {
  // TODO(b/154845685): Implement this method.
  NOTIMPLEMENTED();
  return false;
}

bool BleMedium::StartScanning(const std::string& service_id,
                              api::BleMedium::DiscoveredPeripheralCallback
                                  discovered_peripheral_callback) {
  // TODO(b/154848193): Implement this method.
  NOTIMPLEMENTED();
  return true;
}

bool BleMedium::StopScanning(const std::string& service_id) {
  // TODO(b/154848193): Implement this method.
  NOTIMPLEMENTED();
  return false;
}

bool BleMedium::StartAcceptingConnections(
    const std::string& service_id,
    api::BleMedium::AcceptedConnectionCallback accepted_connection_callback) {
  // Do not actually start a GATT server, because BLE connections are not yet
  // supported in Chrome Nearby. However, return true in order to allow
  // BLE advertising to continue.

  // TODO(hansberry): Verify if this is still required in NCv2.
  return true;
}

bool BleMedium::StopAcceptingConnections(const std::string& service_id) {
  // Do nothing. BLE connections are not yet supported in Chrome Nearby.
  return false;
}

std::unique_ptr<api::BleSocket> BleMedium::Connect(
    api::BlePeripheral& ble_peripheral,
    const std::string& service_id) {
  // Do nothing. BLE connections are not yet supported in Chrome Nearby.
  return nullptr;
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location
