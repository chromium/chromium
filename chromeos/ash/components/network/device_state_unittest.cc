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

// Creates a list of cellular SIM slots with an eSIM and pSIM slot.
base::Value GenerateTestSimSlotInfos() {
  base::Value::List sim_slot_infos;

  base::Value::Dict psim_slot_info;
  psim_slot_info.Set(shill::kSIMSlotInfoICCID, kTestCellularPSimIccid);
  psim_slot_info.Set(shill::kSIMSlotInfoEID, std::string());
  psim_slot_info.Set(shill::kSIMSlotInfoPrimary, true);
  sim_slot_infos.Append(std::move(psim_slot_info));

  base::Value::Dict esim_slot_info;
  esim_slot_info.Set(shill::kSIMSlotInfoICCID, kTestCellularESimIccid);
  esim_slot_info.Set(shill::kSIMSlotInfoEID, kTestCellularEid);
  esim_slot_info.Set(shill::kSIMSlotInfoPrimary, false);
  sim_slot_infos.Append(std::move(esim_slot_info));

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

  void SetIccid() {
    helper_.device_test()->SetDeviceProperty(
        kTestCellularDevicePath, shill::kIccidProperty,
        base::Value(kTestCellularPSimIccid), /*notify_changed=*/true);
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

TEST_F(DeviceStateTest, SimSlotInfo_Wifi) {
  // Default SIM slots should not be created for non-cellular.
  EXPECT_TRUE(GetWifiDevice()->GetSimSlotInfos().empty());
}

}  // namespace ash
