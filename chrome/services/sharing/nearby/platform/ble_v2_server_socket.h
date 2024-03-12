// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_SERVER_SOCKET_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_SERVER_SOCKET_H_

#include "chrome/services/sharing/nearby/platform/condition_variable.h"
#include "chrome/services/sharing/nearby/platform/mutex.h"
#include "third_party/nearby/src/internal/platform/byte_array.h"
#include "third_party/nearby/src/internal/platform/exception.h"
#include "third_party/nearby/src/internal/platform/implementation/ble_v2.h"
#include "third_party/nearby/src/internal/platform/input_stream.h"
#include "third_party/nearby/src/internal/platform/output_stream.h"

namespace nearby::chrome {

// This is a non-functional stub class used in an early version of
// BLE V2, which is still called by Nearby Connections.
// TODO(b/320554697): Remove calls to OpenServerSocket from NC.
class BleV2InputStream : public InputStream {
 public:
  ~BleV2InputStream() override = default;
  ExceptionOr<ByteArray> Read(std::int64_t size) override;
  Exception Close() override;
};

// This is a non-functional stub class used in an early version of
// BLE V2, which is still called by Nearby Connections.
// TODO(b/320554697): Remove calls to OpenServerSocket from NC.
class BleV2OutputStream : public OutputStream {
 public:
  ~BleV2OutputStream() override = default;

  Exception Write(const ByteArray& data) override;
  Exception Flush() override;
  Exception Close() override;
};

// This is a non-functional stub class used in an early version of
// BLE V2, which is still called by Nearby Connections.
// TODO(b/320554697): Remove calls to OpenServerSocket from NC.
class BleV2Socket : public ::nearby::api::ble_v2::BleSocket {
 public:
  BleV2Socket();
  ~BleV2Socket() override;

  BleV2Socket(const BleV2Socket&) = delete;
  BleV2Socket& operator=(const BleV2Socket&) = delete;

  // nearby::api::ble_v2::BleSocket:
  InputStream& GetInputStream() override;
  OutputStream& GetOutputStream() override;
  Exception Close() override;
  ::nearby::api::ble_v2::BlePeripheral* GetRemotePeripheral() override;

 private:
  std::unique_ptr<InputStream> input_stream_;
  std::unique_ptr<OutputStream> output_stream_;
  std::unique_ptr<::nearby::api::ble_v2::BlePeripheral> peripheral_;
};

// This is a non-functional stub class used in an early version of
// BLE V2, which is still called by Nearby Connections.
// TODO(b/320554697): Remove calls to OpenServerSocket from NC.
class BleV2ServerSocket : public ::nearby::api::ble_v2::BleServerSocket {
 public:
  BleV2ServerSocket();
  ~BleV2ServerSocket() override;

  BleV2ServerSocket(const BleV2ServerSocket&) = delete;
  BleV2ServerSocket& operator=(const BleV2ServerSocket&) = delete;

  // nearby::api::ble_v2::BleServerSocket:
  std::unique_ptr<::nearby::api::ble_v2::BleSocket> Accept() override;
  Exception Close() override;

 private:
  Mutex mutex_;
  ConditionVariable condition_variable_{&mutex_};
  bool closed_ = false;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_SERVER_SOCKET_H_
