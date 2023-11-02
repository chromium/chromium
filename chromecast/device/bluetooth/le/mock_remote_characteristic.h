// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_REMOTE_CHARACTERISTIC_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_REMOTE_CHARACTERISTIC_H_

#include <utility>
#include <vector>

#include "chromecast/device/bluetooth/le/remote_characteristic.h"
#include "chromecast/device/bluetooth/le/remote_descriptor.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace bluetooth {

class MockRemoteCharacteristic : public RemoteCharacteristic {
 public:
  explicit MockRemoteCharacteristic(const bluetooth_v2_shlib::Uuid& uuid);

  MOCK_METHOD0(GetDescriptors, std::vector<scoped_refptr<RemoteDescriptor>>());
  MOCK_METHOD1(
      GetDescriptorByUuid,
      scoped_refptr<RemoteDescriptor>(const bluetooth_v2_shlib::Uuid& uuid));

  MOCK_METHOD1(SetRegisterNotification, bool(bool));
  void SetRegisterNotification(bool enable, StatusCallback cb) override {
    std::move(cb).Run(SetRegisterNotification(enable));
  }

  MOCK_METHOD1(SetRegisterNotificationOrIndication, bool(bool));
  void SetRegisterNotificationOrIndication(bool enable,
                                           StatusCallback cb) override {
    std::move(cb).Run(SetRegisterNotificationOrIndication(enable));
  }

  void SetNotification(bool enable, StatusCallback cb) override {}
  void ReadAuth(bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                ReadCallback callback) override {}

  MOCK_METHOD0(Read, std::pair<bool, std::vector<uint8_t>>());
  void Read(ReadCallback callback) override {
    auto res = Read();
    std::move(callback).Run(res.first, std::move(res.second));
  }

  MOCK_METHOD3(WriteAuth,
               bool(bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                    bluetooth_v2_shlib::Gatt::WriteType write_type,
                    const std::vector<uint8_t>& value));
  void WriteAuth(bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                 bluetooth_v2_shlib::Gatt::WriteType write_type,
                 const std::vector<uint8_t>& value,
                 StatusCallback callback) override {
    std::move(callback).Run(WriteAuth(auth_req, write_type, value));
  }

  MOCK_METHOD1(Write, bool(const std::vector<uint8_t>& value));
  void Write(const std::vector<uint8_t>& value,
             StatusCallback callback) override {
    std::move(callback).Run(Write(value));
  }

  MOCK_METHOD0(NotificationEnabled, bool());
  const bluetooth_v2_shlib::Uuid& uuid() const override { return uuid_; }
  MOCK_CONST_METHOD0(handle, HandleId());
  MOCK_CONST_METHOD0(permissions, bluetooth_v2_shlib::Gatt::Permissions());
  MOCK_CONST_METHOD0(properties, bluetooth_v2_shlib::Gatt::Properties());

  const bluetooth_v2_shlib::Uuid uuid_;

 private:
  friend testing::StrictMock<MockRemoteCharacteristic>;

  ~MockRemoteCharacteristic() override;
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_REMOTE_CHARACTERISTIC_H_
