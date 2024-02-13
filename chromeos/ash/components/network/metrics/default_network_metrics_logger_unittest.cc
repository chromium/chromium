// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/default_network_metrics_logger.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

const char kCellularGuid[] = "cellular_guid";
const char kCellularServicePath[] = "/service/network";
const char kCellularName[] = "cellular_name";

const char kWifiGuid[] = "wifi_guid";
const char kWifiServicePath[] = "/service/network2";
const char kWifiName[] = "wifi_name";

const char kEthernetGuid[] = "eth_guid";
const char kEthernetServicePath[] = "/service/network3";
const char kEthernetName[] = "eth_name";

const char kVpnGuid[] = "vpn_guid";
const char kVpnServicePath[] = "/service/network4";
const char kVpnName[] = "vpn_name";

}  // namespace

class DefaultNetworkMetricsLoggerTest : public testing::Test {
 public:
  DefaultNetworkMetricsLoggerTest() = default;
  DefaultNetworkMetricsLoggerTest(const DefaultNetworkMetricsLoggerTest&) =
      delete;
  DefaultNetworkMetricsLoggerTest& operator=(
      const DefaultNetworkMetricsLoggerTest&) = delete;
  ~DefaultNetworkMetricsLoggerTest() override = default;

  void SetUp() override {
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();

    GetShillServiceClient()->ClearServices();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    GetShillServiceClient()->ClearServices();
    network_handler_test_helper_.reset();
  }

  void SetUpGenericCellularNetwork() {
    GetShillServiceClient()->AddService(kCellularServicePath, kCellularGuid,
                                        kCellularName, shill::kTypeCellular,
                                        shill::kStateIdle,
                                        /*visible=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void SetUpGenericWifiNetwork() {
    GetShillServiceClient()->AddService(kWifiServicePath, kWifiGuid, kWifiName,
                                        shill::kTypeWifi, shill::kStateIdle,
                                        /*visible=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void SetUpGenericEthernetNetwork() {
    GetShillServiceClient()->AddService(kEthernetServicePath, kEthernetGuid,
                                        kEthernetName, shill::kTypeEthernet,
                                        shill::kStateIdle,
                                        /*visible=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void SetUpGenericVpnNetwork() {
    GetShillServiceClient()->AddService(kVpnServicePath, kVpnGuid, kVpnName,
                                        shill::kTypeVPN, shill::kStateIdle,
                                        /*visible=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void SetShillState(const std::string& service_path,
                     const std::string& shill_state) {
    GetShillServiceClient()->SetServiceProperty(
        service_path, shill::kStateProperty, base::Value(shill_state));
    base::RunLoop().RunUntilIdle();
  }

  void SetShillMetered(const std::string& service_path, bool is_metered) {
    GetShillServiceClient()->SetServiceProperty(
        service_path, shill::kMeteredProperty, base::Value(is_metered));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  ShillServiceClient::TestInterface* GetShillServiceClient() {
    return ShillServiceClient::Get()->GetTestInterface();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
};

TEST_F(DefaultNetworkMetricsLoggerTest, NetworkTechnologyMeterSubtypeChanges) {
  using NetworkTechnologyMeterSubtype =
      DefaultNetworkMetricsLogger::NetworkTechnologyMeterSubtype;

  SetUpGenericVpnNetwork();
  SetUpGenericCellularNetwork();
  SetUpGenericWifiNetwork();
  SetUpGenericEthernetNetwork();

  histogram_tester_->ExpectTotalCount(
      DefaultNetworkMetricsLogger::kDefaultNetworkMeterSubtypeHistogram, 0);

  SetShillState(kEthernetServicePath, shill::kStateOnline);
  histogram_tester_->ExpectTotalCount(
      DefaultNetworkMetricsLogger::kDefaultNetworkMeterSubtypeHistogram, 1);
  histogram_tester_->ExpectBucketCount(
      DefaultNetworkMetricsLogger::kDefaultNetworkMeterSubtypeHistogram,
      NetworkTechnologyMeterSubtype::kEthernet, 1);

  // Cellular becomes connected. Note that cellular is metered by default.
  // No impact on metrics because connected ethernet is higher in priority.
  SetShillState(kCellularServicePath, shill::kStateOnline);
  histogram_tester_->ExpectTotalCount(
      DefaultNetworkMetricsLogger::kDefaultNetworkMeterSubtypeHistogram, 1);

  // Ethernet goes offline, so connected metered cellular becomes default.
  SetShillState(kEthernetServicePath, shill::kStateIdle);
  histogram_tester_->ExpectTotalCount(
      DefaultNetworkMetricsLogger::kDefaultNetworkMeterSubtypeHistogram, 2);
  histogram_tester_->ExpectBucketCount(
      DefaultNetworkMetricsLogger::kDefaultNetworkMeterSubtypeHistogram,
      NetworkTechnologyMeterSubtype::kCellularMetered, 1);

  // Cellular becomes non-metered.
  SetShillMetered(kCellularServicePath, false);
  histogram_tester_->ExpectTotalCount(
      DefaultNetworkMetricsLogger::kDefaultNetworkMeterSubtypeHistogram, 3);
  histogram_tester_->ExpectBucketCount(
      DefaultNetworkMetricsLogger::kDefaultNetworkMeterSubtypeHistogram,
      NetworkTechnologyMeterSubtype::kCellular, 1);

  // WiFi becomes online, which has higher priority than cellular.
  SetShillState(kWifiServicePath, shill::kStateOnline);
  histogram_tester_->ExpectTotalCount(
      DefaultNetworkMetricsLogger::kDefaultNetworkMeterSubtypeHistogram, 4);
  histogram_tester_->ExpectBucketCount(
      DefaultNetworkMetricsLogger::kDefaultNetworkMeterSubtypeHistogram,
      NetworkTechnologyMeterSubtype::kWifi, 1);

  // WiFi becomes metered, which has higher priority than cellular.
  SetShillMetered(kWifiServicePath, true);
  histogram_tester_->ExpectTotalCount(
      DefaultNetworkMetricsLogger::kDefaultNetworkMeterSubtypeHistogram, 5);
  histogram_tester_->ExpectBucketCount(
      DefaultNetworkMetricsLogger::kDefaultNetworkMeterSubtypeHistogram,
      NetworkTechnologyMeterSubtype::kWifiMetered, 1);

  // Ethernet becomes online, which has highest priority.
  SetShillState(kEthernetServicePath, shill::kStateOnline);
  // Connected WiFi becomes nonmetered. No effect on metrics since it's lower in
  // priority than connected Ethernet.
  SetShillMetered(kWifiServicePath, false);
  histogram_tester_->ExpectTotalCount(
      DefaultNetworkMetricsLogger::kDefaultNetworkMeterSubtypeHistogram, 6);
  histogram_tester_->ExpectBucketCount(
      DefaultNetworkMetricsLogger::kDefaultNetworkMeterSubtypeHistogram,
      NetworkTechnologyMeterSubtype::kEthernet, 2);

  // Ethernet goes offline, so connected nonmetered WiFi becomes default.
  SetShillState(kEthernetServicePath, shill::kStateIdle);
  // Cellular becomes metered. No effect on metrics since it's lower in
  // priority than connected WiFi.
  SetShillMetered(kCellularServicePath, true);
  histogram_tester_->ExpectTotalCount(
      DefaultNetworkMetricsLogger::kDefaultNetworkMeterSubtypeHistogram, 7);
  histogram_tester_->ExpectBucketCount(
      DefaultNetworkMetricsLogger::kDefaultNetworkMeterSubtypeHistogram,
      NetworkTechnologyMeterSubtype::kWifi, 2);

  // WiFi goes offline, so connected metered cellular becomes default.
  SetShillState(kWifiServicePath, shill::kStateIdle);
  histogram_tester_->ExpectTotalCount(
      DefaultNetworkMetricsLogger::kDefaultNetworkMeterSubtypeHistogram, 8);
  histogram_tester_->ExpectBucketCount(
      DefaultNetworkMetricsLogger::kDefaultNetworkMeterSubtypeHistogram,
      NetworkTechnologyMeterSubtype::kCellularMetered, 2);

  // VPN is ignored.
  SetShillState(kVpnServicePath, shill::kStateIdle);
  SetShillState(kVpnServicePath, shill::kStateOnline);
  histogram_tester_->ExpectTotalCount(
      DefaultNetworkMetricsLogger::kDefaultNetworkMeterSubtypeHistogram, 8);
}

}  // namespace ash
