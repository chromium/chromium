// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_v2_server_socket.h"

#include "base/logging.h"
#include "chrome/services/sharing/nearby/platform/ble_v2_remote_peripheral.h"

#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"

namespace nearby::chrome {

// =================BleV2Socket=================
BleV2Socket::BleV2Socket() {}
BleV2Socket::~BleV2Socket() {}

InputStream& BleV2Socket::GetInputStream() {
  // Left deliberately unimplemented.
  mojo::ScopedDataPipeConsumerHandle input_handle;
  input_stream_ = std::make_unique<InputStreamImpl>(
      connections::mojom::Medium::kBluetooth, nullptr, std::move(input_handle));
  return *input_stream_;
}

OutputStream& BleV2Socket::GetOutputStream() {
  // Left deliberately unimplemented.
  mojo::ScopedDataPipeProducerHandle handle;
  output_stream_ = std::make_unique<OutputStreamImpl>(
      connections::mojom::Medium::kBluetooth, nullptr, std::move(handle));
  return *output_stream_;
}

Exception BleV2Socket::Close() {
  // Left deliberately unimplemented.
  return {Exception::kSuccess};
}

::nearby::api::ble_v2::BlePeripheral* BleV2Socket::GetRemotePeripheral() {
  // Left deliberately unimplemented.
  auto device_info = bluetooth::mojom::DeviceInfo::New();
  peripheral_ = std::make_unique<BleV2RemotePeripheral>(std::move(device_info));
  return peripheral_.get();
}

// =================BleV2ServerSocket=================
BleV2ServerSocket::BleV2ServerSocket() {}

BleV2ServerSocket::~BleV2ServerSocket() {}

std::unique_ptr<::nearby::api::ble_v2::BleSocket> BleV2ServerSocket::Accept() {
  // Left deliberately unimplemented.
  return std::make_unique<BleV2Socket>();
}

Exception BleV2ServerSocket::Close() {
  // Left deliberately unimplemented.
  return {Exception::kSuccess};
}

}  // namespace nearby::chrome
