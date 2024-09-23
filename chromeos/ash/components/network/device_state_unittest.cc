// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/device_state.h"

#include "base/test/task_environment.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace {

const char kTestCellularDevicePath[] = "cellular_path";
const char kTestCellularDeviceName[] = "cellular_name";
const char kTestCellularPSimIccid[] = "psim_iccid";
const char kTestCellularESimIccid[] = "esim_iccid";
const char kTestCellularEid[] = "eid";

const char kTestWifiDevicePath[] = "wifi_path";
const char kTestWifiDeviceName[] = "wifi_name";
const char kTestIpv6ConfigPath[] = "/ipconfig/wlan0_ipv6";
const char kTestIpv4ConfigPath[] = "/ipconfig/wlan0_ipv4";
const char kTestIpv4Address_1[] = "192.168.1.255";
const char kTestIpv4Address_2[] = "192.168.1.123";
const char kTestIpv6Address_1[] = "2600:1700:6900:f000:ab00:8a39:2099:72fd";
const char kTestIpv6Address_2[] = "2600:1700:6900:f000:ab00:8a39:2099:12df";

// Creates a list of cellular SIM slots with an eSIM and pSIM slot.
base::Value GenerateTestSimSlotInfos() {
  auto psim_slot_info =
      base::Value::Dict()
          .Set(shill::kSIMSlotInfoICCID, kTestCellularPSimIccid)
          .Set(shill::kSIMSlotInfoEID, std::string())
          .Set(shill::kSIMSlotInfoPrimary, true);

  auto esim_slot_info =
      base::Value::Dict()
          .Set(shill::kSIMSlotInfoICCID, kTestCellularESimIccid)
          .Set(shill::kSIMSlotInfoEID, kTestCellularEid)
          .Set(shill::kSIMSlotInfoPrimary, false);

  auto sim_slot_infos = base::Value::List()
                            .Append(std::move(psim_slot_info))
                            .Append(std::move(esim_slot_info));

  return base::Value(std::move(sim_slot_infos));
}

}  // namespace

class DeviceStateTest : public testing::Test {
 protected:
  DeviceStateTest() = default;
  DeviceStateTest(const DeviceStateTest&) = delete;
  DeviceStateTest& operator=(const DeviceStateTest&) = delete;
  ~DeviceStateTest() override = default;

  // testing::Test:
  void SetUp() override {
    helper_.device_test()->AddDevice(
        kTestCellularDevicePath, shill::kTypeCellular, kTestCellularDeviceName);
    helper_.device_test()->AddDevice(kTestWifiDevicePath, shill::kTypeWifi,
                                     kTestWifiDeviceName);
    base::RunLoop().RunUntilIdle();
  }

  const DeviceState* GetCellularDevice() {
    return helper_.network_state_handler()->GetDeviceState(
        kTestCellularDevicePath);
  }

  const DeviceState* GetWifiDevice() {
    return helper_.network_state_handler()->GetDeviceState(kTestWifiDevicePath);
  }

  void UpdateDeviceIpConfigProperties(const std::string& device_path,
                                      const std::string& ip_config_path,
                                      base::Value::Dict properties) {
    helper_.network_state_handler()->UpdateIPConfigProperties(
        ManagedState::MANAGED_TYPE_DEVICE, device_path, ip_config_path,
        std::move(properties));
  }

  void SetIccid() {
    helper_.device_test()->SetDeviceProperty(
        kTestCellularDevicePath, shill::kIccidProperty,
        base::Value(kTestCellularPSimIccid), /*notify_changed=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void SetFlashing(bool flashing) {
    helper_.device_test()->SetDeviceProperty(
        kTestCellularDevicePath, shill::kFlashingProperty,
        base::Value(flashing), /*notify_changed=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void SetSimSlotInfos() {
    helper_.device_test()->SetDeviceProperty(
        kTestCellularDevicePath, shill::kSIMSlotInfoProperty,
        GenerateTestSimSlotInfos(), /*notify_changed=*/true);
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  NetworkStateTestHelper helper_{/*use_default_devices_and_services=*/false};
};

TEST_F(DeviceStateTest, SimSlotInfo_Cellular) {
  // Set an ICCID, but do not set the SIMSlotInfo property.
  SetIccid();

  // A single SIM slot should have been created, copying the ICCID.
  DeviceState::CellularSIMSlotInfos infos =
      GetCellularDevice()->GetSimSlotInfos();
  EXPECT_EQ(1u, infos.size());
  EXPECT_EQ(kTestCellularPSimIccid, infos[0].iccid);
  EXPECT_TRUE(infos[0].eid.empty());
  EXPECT_TRUE(infos[0].primary);

  // Set the SIM Slot infos.
  SetSimSlotInfos();
  infos = GetCellularDevice()->GetSimSlotInfos();
  EXPECT_EQ(2u, infos.size());
  EXPECT_EQ(kTestCellularPSimIccid, infos[0].iccid);
  EXPECT_TRUE(infos[0].eid.empty());
  EXPECT_TRUE(infos[0].primary);
  EXPECT_EQ(kTestCellularESimIccid, infos[1].iccid);
  EXPECT_EQ(kTestCellularEid, infos[1].eid);
  EXPECT_FALSE(infos[1].primary);
}

TEST_F(DeviceStateTest, Flashing_Cellular) {
  SetFlashing(true);
  EXPECT_TRUE(GetCellularDevice()->flashing());
  SetFlashing(false);
  EXPECT_FALSE(GetCellularDevice()->flashing());
}

TEST_F(DeviceStateTest, SimSlotInfo_Wifi) {
  // Default SIM slots should not be created for non-cellular.
  EXPECT_TRUE(GetWifiDevice()->GetSimSlotInfos().empty());
}

TEST_F(DeviceStateTest, DeviceIPAddress) {
  auto dhcp_ip_config = base::Value::Dict()
                            .Set(shill::kAddressProperty, kTestIpv4Address_1)
                            .Set(shill::kMethodProperty, shill::kTypeDHCP);
  UpdateDeviceIpConfigProperties(kTestWifiDevicePath, kTestIpv4ConfigPath,
                                 std::move(dhcp_ip_config));
  EXPECT_EQ(kTestIpv4Address_1,
            GetWifiDevice()->GetIpAddressByType(shill::kTypeIPv4));
  EXPECT_EQ(std::string(),
            GetWifiDevice()->GetIpAddressByType(shill::kTypeIPv6));

  auto ipv4_ip_config = base::Value::Dict()
                            .Set(shill::kAddressProperty, kTestIpv4Address_2)
                            .Set(shill::kMethodProperty, shill::kTypeIPv4);
  UpdateDeviceIpConfigProperties(kTestWifiDevicePath, kTestIpv4ConfigPath,
                                 std::move(ipv4_ip_config));
  EXPECT_EQ(kTestIpv4Address_2,
            GetWifiDevice()->GetIpAddressByType(shill::kTypeIPv4));
  EXPECT_EQ(std::string(),
            GetWifiDevice()->GetIpAddressByType(shill::kTypeIPv6));

  auto slaac_ip_config = base::Value::Dict()
                             .Set(shill::kAddressProperty, kTestIpv6Address_1)
                             .Set(shill::kMethodProperty, shill::kTypeSLAAC);
  UpdateDeviceIpConfigProperties(kTestWifiDevicePath, kTestIpv6ConfigPath,
                                 std::move(slaac_ip_config));
  EXPECT_EQ(kTestIpv4Address_2,
            GetWifiDevice()->GetIpAddressByType(shill::kTypeIPv4));
  EXPECT_EQ(kTestIpv6Address_1,
            GetWifiDevice()->GetIpAddressByType(shill::kTypeIPv6));

  auto ipv6_ip_config = base::Value::Dict()
                            .Set(shill::kAddressProperty, kTestIpv6Address_2)
                            .Set(shill::kMethodProperty, shill::kTypeIPv6);
  UpdateDeviceIpConfigProperties(kTestWifiDevicePath, kTestIpv6ConfigPath,
                                 std::move(ipv6_ip_config));
  EXPECT_EQ(kTestIpv4Address_2,
            GetWifiDevice()->GetIpAddressByType(shill::kTypeIPv4));
  EXPECT_EQ(kTestIpv6Address_2,
            GetWifiDevice()->GetIpAddressByType(shill::kTypeIPv6));
}

}  // namespace ash
