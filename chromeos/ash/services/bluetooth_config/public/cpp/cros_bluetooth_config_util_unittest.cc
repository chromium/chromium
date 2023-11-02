// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash::bluetooth_config {

namespace {

const std::u16string kTestPublicName = u"Public Name";

}  // namespace

TEST(CrosBluetoothConfigUtilTest, GetPairedDeviceNameNoNickname) {
  auto device_properties = mojom::BluetoothDeviceProperties::New();
  device_properties->public_name = kTestPublicName;
  auto properties = mojom::PairedBluetoothDeviceProperties::New();
  properties->device_properties = std::move(device_properties);

  EXPECT_EQ(GetPairedDeviceName(properties), kTestPublicName);
}

TEST(CrosBluetoothConfigUtilTest, GetPairedDeviceNameNickname) {
  auto device_properties = mojom::BluetoothDeviceProperties::New();
  device_properties->public_name = kTestPublicName;
  auto properties = mojom::PairedBluetoothDeviceProperties::New();
  properties->device_properties = std::move(device_properties);
  properties->nickname = "Nickname 🟣";

  EXPECT_EQ(GetPairedDeviceName(properties), u"Nickname 🟣");
}

}  // namespace ash::bluetooth_config
