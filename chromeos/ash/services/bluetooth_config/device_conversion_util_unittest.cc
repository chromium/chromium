// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/device_conversion_util.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/services/bluetooth_config/fake_fast_pair_delegate.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/device_image_info.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::bluetooth_config {

namespace {

constexpr char kTestDefaultImage[] = "data:image/png;base64,TestDefaultImage";
constexpr char kTestLeftBudImage[] = "data:image/png;base64,TestLeftBudImage";
constexpr char kTestRightBudImage[] = "data:image/png;base64,TestRightBudImage";
constexpr char kTestCaseImage[] = "data:image/png;base64,TestCaseImage";

constexpr int kRenderingBitPosition = 18;
constexpr int kAudioBitPosition = 21;

const std::array<device::BluetoothUUID, 3> kAudioServiceUuids{
    device::BluetoothUUID("00001108-0000-1000-8000-00805f9b34fb"),  // Headset
    device::BluetoothUUID(
        "0000110b-0000-1000-8000-00805f9b34fb"),  // Audio Sink
    device::BluetoothUUID("0000111e-0000-1000-8000-00805f9b34fb"),  // Handsfree
};

}  // namespace

// Tests basic usage of the GenerateBluetoothDeviceMojoProperties() conversion
// function. Not meant to be an exhaustive test of each possible Bluetooth
// property.
class DeviceConversionUtilTest : public testing::Test {
 protected:
  DeviceConversionUtilTest()
      : fake_fast_pair_delegate_(FakeFastPairDelegate()) {}
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
                                      bool connected,
                                      bool is_blocked_by_policy) {
    mock_device_ =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            mock_adapter_.get(), bluetooth_class, name, address, paired,
            connected);
    mock_device_->SetIsBlockedByPolicy(is_blocked_by_policy);

    return mock_device_.get();
  }

  void ChangeDeviceConnected(bool connected) {
    ON_CALL(*mock_device_, IsConnected())
        .WillByDefault(testing::Return(connected));
  }

  void ChangeDeviceUUIDs(base::flat_set<device::BluetoothUUID> uuids) {
    ON_CALL(*mock_device_, GetUUIDs()).WillByDefault(testing::Return(uuids));
  }

  FakeFastPairDelegate* fake_fast_pair_delegate() {
    return &fake_fast_pair_delegate_;
  }

 private:
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>> mock_device_;
  FakeFastPairDelegate fake_fast_pair_delegate_;
};

TEST_F(DeviceConversionUtilTest, TestConversion) {
  device::BluetoothDevice* device = InitDevice(
      /*bluetooth_class=*/0u, /*name=*/"name", /*address=*/"address",
      /*paired=*/true, /*connected=*/true, /*is_blocked_by_policy=*/true);
  mojom::BluetoothDevicePropertiesPtr properties =
      GenerateBluetoothDeviceMojoProperties(device,
                                            /*fast_pair_delegate=*/nullptr);
  ASSERT_TRUE(properties);
  EXPECT_EQ("address-Identifier", properties->id);
  EXPECT_EQ("address", properties->address);
  EXPECT_EQ(u"name", properties->public_name);
  EXPECT_EQ(mojom::DeviceType::kUnknown, properties->device_type);
  EXPECT_EQ(mojom::AudioOutputCapability::kNotCapableOfAudioOutput,
            properties->audio_capability);
  EXPECT_FALSE(properties->battery_info);
  EXPECT_EQ(mojom::DeviceConnectionState::kConnected,
            properties->connection_state);
  EXPECT_TRUE(properties->is_blocked_by_policy);
}

TEST_F(DeviceConversionUtilTest, TestConversion_EmptyName) {
  device::BluetoothDevice* device = InitDevice(
      /*bluetooth_class=*/0u, /*name=*/nullptr, /*address=*/"address",
      /*paired=*/true, /*connected=*/true, /*is_blocked_by_policy=*/true);
  mojom::BluetoothDevicePropertiesPtr properties =
      GenerateBluetoothDeviceMojoProperties(device,
                                            /*fast_pair_delegate=*/nullptr);
  ASSERT_TRUE(properties);

  // A device with no name should have its name set as its address.
  EXPECT_EQ(u"address", properties->public_name);
}

TEST_F(DeviceConversionUtilTest, TestConversion_DefaultBattery) {
  device::BluetoothDevice* device = InitDevice(
      /*bluetooth_class=*/0u, /*name=*/"name", /*address=*/"address",
      /*paired=*/true, /*connected=*/false, /*is_blocked_by_policy=*/false);

  device::BluetoothDevice::BatteryInfo battery_info(
      /*battery_type=*/device::BluetoothDevice::BatteryType::kDefault,
      /*percentage=*/65,
      /*charge_state=*/
      device::BluetoothDevice::BatteryInfo::ChargeState::kCharging);
  device->SetBatteryInfo(battery_info);

  mojom::BluetoothDevicePropertiesPtr properties =
      GenerateBluetoothDeviceMojoProperties(device,
                                            /*fast_pair_delegate=*/nullptr);
  ASSERT_TRUE(properties);

  // If device is not connected, |battery_info| should be null.
  EXPECT_FALSE(properties->battery_info);

  // Set the device to connected.
  ChangeDeviceConnected(/*connected=*/true);

  properties = GenerateBluetoothDeviceMojoProperties(
      device, /*fast_pair_delegate=*/nullptr);
  ASSERT_TRUE(properties);

  EXPECT_TRUE(properties->battery_info->default_properties);
  EXPECT_EQ(properties->battery_info->default_properties->battery_percentage,
            65);
  EXPECT_FALSE(properties->battery_info->left_bud_info);
  EXPECT_FALSE(properties->battery_info->right_bud_info);
  EXPECT_FALSE(properties->battery_info->case_info);
}

TEST_F(DeviceConversionUtilTest, TestConversion_MultipleBatteries) {
  device::BluetoothDevice* device = InitDevice(
      /*bluetooth_class=*/0u, /*name=*/"name", /*address=*/"address",
      /*paired=*/true, /*connected=*/false, /*is_blocked_by_policy=*/false);

  device::BluetoothDevice::BatteryInfo left_battery_info(
      /*battery_type=*/device::BluetoothDevice::BatteryType::
          kLeftBudTrueWireless,
      /*percentage=*/65,
      /*charge_state=*/
      device::BluetoothDevice::BatteryInfo::ChargeState::kCharging);
  device->SetBatteryInfo(left_battery_info);

  device::BluetoothDevice::BatteryInfo right_battery_info(
      /*battery_type=*/device::BluetoothDevice::BatteryType::
          kRightBudTrueWireless,
      /*percentage=*/45,
      /*charge_state=*/
      device::BluetoothDevice::BatteryInfo::ChargeState::kCharging);
  device->SetBatteryInfo(right_battery_info);

  device::BluetoothDevice::BatteryInfo case_battery_info(
      /*battery_type=*/device::BluetoothDevice::BatteryType::kCaseTrueWireless,
      /*percentage=*/50,
      /*charge_state=*/
      device::BluetoothDevice::BatteryInfo::ChargeState::kCharging);
  device->SetBatteryInfo(case_battery_info);

  mojom::BluetoothDevicePropertiesPtr properties =
      GenerateBluetoothDeviceMojoProperties(device,
                                            /*fast_pair_delegate=*/nullptr);
  ASSERT_TRUE(properties);

  // If device is not connected, |battery_info| should be null.
  EXPECT_FALSE(properties->battery_info);

  // Set the device to connected.
  ChangeDeviceConnected(/*connected=*/true);

  properties = GenerateBluetoothDeviceMojoProperties(
      device, /*fast_pair_delegate=*/nullptr);
  ASSERT_TRUE(properties);

  EXPECT_FALSE(properties->battery_info->default_properties);
  EXPECT_TRUE(properties->battery_info->left_bud_info);
  EXPECT_EQ(properties->battery_info->left_bud_info->battery_percentage, 65);
  EXPECT_TRUE(properties->battery_info->right_bud_info);
  EXPECT_EQ(properties->battery_info->right_bud_info->battery_percentage, 45);
  EXPECT_TRUE(properties->battery_info->case_info);
  EXPECT_EQ(properties->battery_info->case_info->battery_percentage, 50);
}

TEST_F(DeviceConversionUtilTest, TestConversion_NoImages) {
  device::BluetoothDevice* device = InitDevice(
      /*bluetooth_class=*/0u, /*name=*/"name", /*address=*/"address",
      /*paired=*/true, /*connected=*/false, /*is_blocked_by_policy=*/false);

  mojom::BluetoothDevicePropertiesPtr properties =
      GenerateBluetoothDeviceMojoProperties(device, fake_fast_pair_delegate());
  EXPECT_TRUE(properties);

  EXPECT_FALSE(properties->image_info);
}

TEST_F(DeviceConversionUtilTest, TestConversion_DefaultDeviceImage) {
  device::BluetoothDevice* device = InitDevice(
      /*bluetooth_class=*/0u, /*name=*/"name", /*address=*/"address",
      /*paired=*/true, /*connected=*/false, /*is_blocked_by_policy=*/false);
  // Save a default image to be returned by the delegate.
  DeviceImageInfo images = DeviceImageInfo(
      /*default_image=*/kTestDefaultImage, /*left_bud_image=*/"",
      /*right_bud_image=*/"", /*case_image=*/"");
  fake_fast_pair_delegate()->SetDeviceImageInfo("address", images);

  mojom::BluetoothDevicePropertiesPtr properties =
      GenerateBluetoothDeviceMojoProperties(device, fake_fast_pair_delegate());
  EXPECT_TRUE(properties);

  EXPECT_TRUE(properties->image_info);
  EXPECT_TRUE(properties->image_info->default_image_url);
  EXPECT_EQ(properties->image_info->default_image_url, GURL(kTestDefaultImage));
  EXPECT_FALSE(properties->image_info->true_wireless_images);
}

TEST_F(DeviceConversionUtilTest, TestConversion_TrueWirelessImages) {
  device::BluetoothDevice* device = InitDevice(
      /*bluetooth_class=*/0u, /*name=*/"name", /*address=*/"address",
      /*paired=*/true, /*connected=*/false, /*is_blocked_by_policy=*/false);
  // Save a default image and True Wireless images to be returned by the
  // delegate.
  DeviceImageInfo images = DeviceImageInfo(
      /*default_image=*/kTestDefaultImage, /*left_bud_image=*/kTestLeftBudImage,
      /*right_bud_image=*/kTestRightBudImage, /*case_image=*/kTestCaseImage);
  fake_fast_pair_delegate()->SetDeviceImageInfo("address", images);

  mojom::BluetoothDevicePropertiesPtr properties =
      GenerateBluetoothDeviceMojoProperties(device, fake_fast_pair_delegate());
  EXPECT_TRUE(properties);

  EXPECT_TRUE(properties->image_info);
  EXPECT_TRUE(properties->image_info->default_image_url);
  EXPECT_EQ(properties->image_info->default_image_url, GURL(kTestDefaultImage));
  EXPECT_TRUE(properties->image_info->true_wireless_images);
  EXPECT_EQ(properties->image_info->true_wireless_images->left_bud_image_url,
            GURL(kTestLeftBudImage));
  EXPECT_EQ(properties->image_info->true_wireless_images->right_bud_image_url,
            GURL(kTestRightBudImage));
  EXPECT_EQ(properties->image_info->true_wireless_images->case_image_url,
            GURL(kTestCaseImage));
}

TEST_F(DeviceConversionUtilTest, TestConversion_PartialTrueWirelessImages) {
  device::BluetoothDevice* device = InitDevice(
      /*bluetooth_class=*/0u, /*name=*/"name", /*address=*/"address",
      /*paired=*/true, /*connected=*/false, /*is_blocked_by_policy=*/false);
  // Simulate the case image being missing.
  DeviceImageInfo images = DeviceImageInfo(
      /*default_image=*/kTestDefaultImage, /*left_bud_image=*/kTestLeftBudImage,
      /*right_bud_image=*/kTestRightBudImage, /*case_image=*/"");
  fake_fast_pair_delegate()->SetDeviceImageInfo("address", images);

  mojom::BluetoothDevicePropertiesPtr properties =
      GenerateBluetoothDeviceMojoProperties(device, fake_fast_pair_delegate());
  EXPECT_TRUE(properties);

  EXPECT_TRUE(properties->image_info);
  EXPECT_TRUE(properties->image_info->default_image_url);
  EXPECT_EQ(properties->image_info->default_image_url, GURL(kTestDefaultImage));
  // True Wireless images should only display if the full set exists.
  EXPECT_FALSE(properties->image_info->true_wireless_images);
}

TEST_F(DeviceConversionUtilTest,
       TestConversion_AudioOutputCapableBluetoothClass) {
  // Create a device with a Bluetooth class with the "rendering" and
  // "audio" bits set.
  uint32_t bluetooth_class =
      1u << kRenderingBitPosition | 1u << kAudioBitPosition;

  device::BluetoothDevice* device = InitDevice(
      bluetooth_class, /*name=*/"name", /*address=*/"address",
      /*paired=*/true, /*connected=*/true, /*is_blocked_by_policy=*/true);
  mojom::BluetoothDevicePropertiesPtr properties =
      GenerateBluetoothDeviceMojoProperties(device,
                                            /*fast_pair_delegate=*/nullptr);
  ASSERT_TRUE(properties);
  EXPECT_EQ(mojom::AudioOutputCapability::kCapableOfAudioOutput,
            properties->audio_capability);
}

TEST_F(DeviceConversionUtilTest, TestConversion_AudioOutputCapableUUIDs) {
  // Create a device with a non-audio output capable Bluetooth class.
  device::BluetoothDevice* device = InitDevice(
      /*bluetooth_class=*/0u, /*name=*/"name", /*address=*/"address",
      /*paired=*/true, /*connected=*/true, /*is_blocked_by_policy=*/true);

  // Set the device's UUIDs with a UUID corresponding to an audio output capable
  // device. This simulates the case where a device does not have the correct
  // Bluetooth class bits set but still contains a UUID corresponding to an
  // audio service.
  for (device::BluetoothUUID uuid : kAudioServiceUuids) {
    ChangeDeviceUUIDs({uuid});
    mojom::BluetoothDevicePropertiesPtr properties =
        GenerateBluetoothDeviceMojoProperties(device,
                                              /*fast_pair_delegate=*/nullptr);
    ASSERT_TRUE(properties);
    EXPECT_EQ(mojom::AudioOutputCapability::kCapableOfAudioOutput,
              properties->audio_capability);
  }
}

}  // namespace ash::bluetooth_config
