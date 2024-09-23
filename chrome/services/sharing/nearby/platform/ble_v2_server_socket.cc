// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_v2_server_socket.h"

#include "chrome/services/sharing/nearby/platform/ble_v2_remote_peripheral.h"

namespace nearby::chrome {

// =================BleV2InputStream=================
ExceptionOr<ByteArray> BleV2InputStream::Read(std::int64_t size) {
  // Left deliberately unimplemented. Requires BLE Weave implementation.
  return ExceptionOr<ByteArray>(Exception::kIo);
}

Exception BleV2InputStream::Close() {
  // Left deliberately unimplemented. Requires BLE Weave implementation.
  return {Exception::kSuccess};
}

// =================BleV2OutputStream=================
Exception BleV2OutputStream::Write(const ByteArray& data) {
  // Left deliberately unimplemented. Requires BLE Weave implementation.
  return {Exception::kIo};
}

Exception BleV2OutputStream::Flush() {
  // Left deliberately unimplemented. Requires BLE Weave implementation.
  return {Exception::kSuccess};
}

Exception BleV2OutputStream::Close() {
  // Left deliberately unimplemented. Requires BLE Weave implementation.
  return {Exception::kSuccess};
}

// =================BleV2Socket=================
BleV2Socket::BleV2Socket() {
  input_stream_ = std::make_unique<BleV2InputStream>();
  output_stream_ = std::make_unique<BleV2OutputStream>();
}
BleV2Socket::~BleV2Socket() {}

InputStream& BleV2Socket::GetInputStream() {
  // Left deliberately unimplemented.
  return *input_stream_;
}

OutputStream& BleV2Socket::GetOutputStream() {
  // Left deliberately unimplemented.
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
  // This implementation of Accept intentionally blocks on the calling thread
  // until Close is called, thus effectively never returning a `BleSocket`
  // instance. This is done intentionally because the Chrome platform is not
  // expected to implement the deprecated `BleSocket` interface, but Nearby
  // Connections calls this Accept method on all platforms.
  //
  // The calling thread is a dedicated worker thread -- blocking here will not
  // block the main thread.
  //
  // In the future, the Chrome platform will implement GattServer and its
  // associated primitives, to allow Nearby Connections to compose its own
  // BleV2ServerSocket via Weave. At that time this method can be revisited to
  // be deleted.
  //
  // Calls to Accept post-Close will not block.
  mutex_.Lock();

  // Sleep forever until Close is called.
  while (!closed_) {
    condition_variable_.Wait();
  }

  mutex_.Unlock();
  return nullptr;
}

Exception BleV2ServerSocket::Close() {
  // This implementation of Close signals all blocked Accept threads to wake
  // and exit. It also sets closed_ to true so that future calls to Accept do
  // not block. Calling Close a second time is expected and is a no-op.
  mutex_.Lock();

  closed_ = true;

  condition_variable_.Notify();

  mutex_.Unlock();
  return {Exception::kSuccess};
}

}  // namespace nearby::chrome
