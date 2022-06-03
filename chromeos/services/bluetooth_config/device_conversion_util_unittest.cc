// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/device_conversion_util.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace bluetooth_config {

// Tests basic usage of the GenerateBluetoothDeviceMojoProperties() conversion
// function. Not meant to be an exhaustive test of each possible Bluetooth
// property.
class DeviceConversionUtilTest : public testing::Test {
 protected:
  DeviceConversionUtilTest() = default;
  DeviceConversionUtilTest(const DeviceConversionUtilTest&) = delete;
  DeviceConversionUtilTest& operator=(const DeviceConversionUtilTest&) = delete;
  ~DeviceConversionUtilTest() override = default;

  // testing::Test:
  void SetUp() override {
    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
  }

  device::BluetoothDevice* InitDevice(uint32_t bluetooth_class,
                                      const char* name,
                                      const std::string& address,
                                      bool paired,
                                      bool connected) {
    mock_device_ =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            mock_adapter_.get(), bluetooth_class, name, address, paired,
            connected);
    return mock_device_.get();
  }

 private:
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>> mock_device_;
};

TEST_F(DeviceConversionUtilTest, TestConversion) {
  device::BluetoothDevice* device =
      InitDevice(/*bluetooth_class=*/0u, /*name=*/"name", /*address=*/"address",
                 /*paired=*/true, /*connected=*/true);
  mojom::BluetoothDevicePropertiesPtr properties =
      GenerateBluetoothDeviceMojoProperties(device);
  ASSERT_TRUE(properties);
  EXPECT_EQ("address-Identifier", properties->id);
  EXPECT_EQ(u"name", properties->public_name);
  EXPECT_EQ(mojom::DeviceType::kUnknown, properties->device_type);
  EXPECT_EQ(mojom::AudioOutputCapability::kNotCapableOfAudioOutput,
            properties->audio_capability);
  EXPECT_FALSE(properties->battery_info);
  EXPECT_EQ(mojom::DeviceConnectionState::kConnected,
            properties->connection_state);
}

}  // namespace bluetooth_config
}  // namespace chromeos
