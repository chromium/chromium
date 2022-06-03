// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_REMOTE_DESCRIPTOR_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_REMOTE_DESCRIPTOR_H_

#include <vector>

#include "chromecast/device/bluetooth/le/remote_descriptor.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace bluetooth {

class MockRemoteDescriptor : public RemoteDescriptor {
 public:
  MockRemoteDescriptor();

  void ReadAuth(bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                ReadCallback callback) override {}
  void Read(ReadCallback callback) override {}
  void WriteAuth(bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                 const std::vector<uint8_t>& value,
                 StatusCallback callback) override {}
  void Write(const std::vector<uint8_t>& value,
             StatusCallback callback) override {}
  MOCK_CONST_METHOD0(uuid, const bluetooth_v2_shlib::Uuid());
  MOCK_CONST_METHOD0(handle, HandleId());
  MOCK_CONST_METHOD0(permissions, bluetooth_v2_shlib::Gatt::Permissions());

 private:
  ~MockRemoteDescriptor();
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_REMOTE_DESCRIPTOR_H_
