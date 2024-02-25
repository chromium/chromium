// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/geolocation_handler.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

class GeolocationHandlerTest : public testing::Test {
 public:
  GeolocationHandlerTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}

  GeolocationHandlerTest(const GeolocationHandlerTest&) = delete;
  GeolocationHandlerTest& operator=(const GeolocationHandlerTest&) = delete;

  ~GeolocationHandlerTest() override = default;

  void SetUp() override {
    shill_clients::InitializeFakes();
    // Get the test interface for manager / device.
    manager_test_ = ShillManagerClient::Get()->GetTestInterface();
    ASSERT_TRUE(manager_test_);
    geolocation_handler_.reset(new GeolocationHandler());
    geolocation_handler_->Init();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    geolocation_handler_.reset();
    shill_clients::Shutdown();
  }

  bool GetWifiAccessPoints() {
    return geolocation_handler_->GetWifiAccessPoints(&wifi_access_points_,
                                                     nullptr);
  }

  bool GetCellTowers() {
    return geolocation_handler_->GetNetworkInformation(nullptr, &cell_towers_);
  }

  // This should remain in sync with the format of shill (chromeos) dict entries
  // Shill provides us Cell ID and LAC in hex, but all other fields in decimal.
  void AddAccessPoint(int idx) {
    std::string mac_address =
        base::StringPrintf("%02X:%02X:%02X:%02X:%02X:%02X",
                           idx, 0, 0, 0, 0, 0);
    std::string channel = base::NumberToString(idx);
    std::string strength = base::NumberToString(idx * 10);

    manager_test_->AddGeoNetwork(
        shill::kGeoWifiAccessPointsProperty,
        base::Value::Dict()
            .Set(shill::kGeoMacAddressProperty, mac_address)
            .Set(shill::kGeoChannelProperty, channel)
            .Set(shill::kGeoSignalStrengthProperty, strength));
    base::RunLoop().RunUntilIdle();
  }

  // This should remain in sync with the format of shill (chromeos) dict entries
  void AddCellTower(int idx) {
    // Multiplications, additions, and string concatenations
    // are intended solely to differentiate the various fields
    // in a predictable way, while preserving 3 digits for MCC and MNC.
    std::string ci = base::NumberToString(idx) + "D3A15F2";
    std::string lac = "7FF" + base::NumberToString(idx);
    std::string mcc = base::NumberToString(idx * 100);
    std::string mnc = base::NumberToString(idx * 100 + 1);

    manager_test_->AddGeoNetwork(
        shill::kGeoCellTowersProperty,
        base::Value::Dict()
            .Set(shill::kGeoCellIdProperty, ci)
            .Set(shill::kGeoLocationAreaCodeProperty, lac)
            .Set(shill::kGeoMobileCountryCodeProperty, mcc)
            .Set(shill::kGeoMobileNetworkCodeProperty, mnc));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<GeolocationHandler> geolocation_handler_;
  raw_ptr<ShillManagerClient::TestInterface, DanglingUntriaged> manager_test_ =
      nullptr;
  WifiAccessPointVector wifi_access_points_;
  CellTowerVector cell_towers_;
};

TEST_F(GeolocationHandlerTest, NoAccessPoints) {
  // Inititial call should return false.
  EXPECT_FALSE(GetWifiAccessPoints());
  EXPECT_FALSE(GetCellTowers());
  base::RunLoop().RunUntilIdle();
  // Second call should return false since there are no devices.
  EXPECT_FALSE(GetWifiAccessPoints());
  EXPECT_FALSE(GetCellTowers());
}

TEST_F(GeolocationHandlerTest, OneAccessPoint) {
  // Add an access point.
  AddAccessPoint(1);
  base::RunLoop().RunUntilIdle();
  // Inititial call should return false and request access points.
  EXPECT_FALSE(GetWifiAccessPoints());
  base::RunLoop().RunUntilIdle();
  // Second call should return true since we have an access point.
  EXPECT_TRUE(GetWifiAccessPoints());
  ASSERT_EQ(1u, wifi_access_points_.size());
  EXPECT_EQ("01:00:00:00:00:00", wifi_access_points_[0].mac_address);
  EXPECT_EQ(1, wifi_access_points_[0].channel);
}

TEST_F(GeolocationHandlerTest, MultipleAccessPoints) {
  // Add several access points.
  AddAccessPoint(1);
  AddAccessPoint(2);
  AddAccessPoint(3);
  base::RunLoop().RunUntilIdle();
  // Inititial call should return false and request access points.
  EXPECT_FALSE(GetWifiAccessPoints());
  EXPECT_FALSE(GetCellTowers());
  base::RunLoop().RunUntilIdle();
  // Second call should return true since we have an access point.
  EXPECT_TRUE(GetWifiAccessPoints());
  ASSERT_EQ(3u, wifi_access_points_.size());
  EXPECT_EQ("02:00:00:00:00:00", wifi_access_points_[1].mac_address);
  EXPECT_EQ(3, wifi_access_points_[2].channel);
}

TEST_F(GeolocationHandlerTest, OneCellTower) {
  // Add a cell tower.
  AddCellTower(1);
  base::RunLoop().RunUntilIdle();
  // Inititial call should return false and request towers.
  EXPECT_FALSE(GetCellTowers());
  EXPECT_FALSE(GetWifiAccessPoints());
  base::RunLoop().RunUntilIdle();
  // Second call should return true since we have a cell tower.
  EXPECT_TRUE(GetCellTowers());
  EXPECT_FALSE(GetWifiAccessPoints());
  ASSERT_EQ(1u, cell_towers_.size());
  EXPECT_EQ("490345970", cell_towers_[0].ci);
  EXPECT_EQ("32753", cell_towers_[0].lac);
  EXPECT_EQ("100", cell_towers_[0].mcc);
  EXPECT_EQ("101", cell_towers_[0].mnc);
}

TEST_F(GeolocationHandlerTest, MultipleCellTowers) {
  // Add several cell towers.
  AddCellTower(1);
  AddCellTower(2);
  AddCellTower(3);
  base::RunLoop().RunUntilIdle();
  // Inititial call should return false and request cell towers.
  EXPECT_FALSE(GetWifiAccessPoints());
  EXPECT_FALSE(GetCellTowers());
  base::RunLoop().RunUntilIdle();
  // Second call should return true since we have a cell tower.
  EXPECT_FALSE(GetWifiAccessPoints());
  EXPECT_TRUE(GetCellTowers());
  ASSERT_EQ(3u, cell_towers_.size());
  EXPECT_EQ("32754", cell_towers_[1].lac);
  EXPECT_EQ("301", cell_towers_[2].mnc);
}

TEST_F(GeolocationHandlerTest, MultipleGeolocations) {
  // Add both a cell tower and wifi AP.
  AddCellTower(1);
  AddCellTower(2);
  AddAccessPoint(1);
  AddAccessPoint(2);
  base::RunLoop().RunUntilIdle();
  // Inititial call should return false and request towers.
  EXPECT_FALSE(GetCellTowers());
  EXPECT_FALSE(GetWifiAccessPoints());
  base::RunLoop().RunUntilIdle();
  // Second call should return true since we have a cell tower.
  EXPECT_TRUE(GetCellTowers());
  EXPECT_TRUE(GetWifiAccessPoints());
  ASSERT_EQ(2u, wifi_access_points_.size());
  EXPECT_EQ("02:00:00:00:00:00", wifi_access_points_[1].mac_address);
  EXPECT_EQ(1, wifi_access_points_[0].channel);

  ASSERT_EQ(2u, cell_towers_.size());
  EXPECT_EQ("758781426", cell_towers_[1].ci);
  EXPECT_EQ("32753", cell_towers_[0].lac);
  EXPECT_EQ("200", cell_towers_[1].mcc);
  EXPECT_EQ("101", cell_towers_[0].mnc);
}

}  // namespace ash
