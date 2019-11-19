// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scan_scheduler_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/timer/mock_timer.h"
#include "chromeos/components/tether/fake_host_scanner.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/services/device_sync/cryptauth_device_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

namespace {

const char kEthernetServiceGuid[] = "ethernetServiceGuid";
const char kWifiServiceGuid[] = "wifiServiceGuid";
const char kTetherGuid[] = "tetherGuid";

std::string CreateConfigurationJsonString(const std::string& guid,
                                          const std::string& type,
                                          const std::string& state) {
  std::stringstream ss;
  ss << "{"
     << "  \"GUID\": \"" << guid << "\","
     << "  \"Type\": \"" << type << "\","
     << "  \"State\": \"" << state << "\""
     << "}";
  return ss.str();
}

}  // namespace

class HostScanSchedulerImplTest : public testing::Test {
 protected:
  void SetUp() override {
    helper_ = std::make_unique<NetworkStateTestHelper>(
        true /* use_default_devices_and_services */);

    histogram_tester_ = std::make_unique<base::HistogramTester>();

    helper_->network_state_handler()->SetTetherTechnologyState(
        NetworkStateHandler::TECHNOLOGY_ENABLED);

    fake_host_scanner_ = std::make_unique<FakeHostScanner>();
    session_manager_ = std::make_unique<session_manager::SessionManager>();

    host_scan_scheduler_ = std::make_unique<HostScanSchedulerImpl>(
        helper_->network_state_handler(), fake_host_scanner_.get(),
        session_manager_.get());

    mock_host_scan_batch_timer_ = new base::MockOneShotTimer();

    // Advance the clock by an arbitrary value to ensure that when Now() is
    // called, the Unix epoch will not be returned.
    test_clock_.Advance(base::TimeDelta::FromSeconds(10));
    test_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    host_scan_scheduler_->SetTestDoubles(
        base::WrapUnique(mock_host_scan_batch_timer_), &test_clock_,
        test_task_runner_);
  }

  void TearDown() override {
    host_scan_scheduler_.reset();
    helper_.reset();
  }

  void RequestScan(const NetworkTypePattern& type) {
    host_scan_scheduler_->ScanRequested(type);
  }

  void InitializeEthernet(bool is_initially_connected) {
    std::string state =
        is_initially_connected ? shill::kStateReady : shill::kStateIdle;
    ethernet_service_path_ =
        helper_->ConfigureService(CreateConfigurationJsonString(
            kEthernetServiceGuid, shill::kTypeEthernet, state));
    helper_->manager_test()->SetManagerProperty(
        shill::kDefaultServiceProperty, base::Value(ethernet_service_path_));
  }

  // Disconnects the Ethernet network and manually sets the default network to
  // |new_default_service_path|. If |new_default_service_path| is empty then no
  // default network is set.
  void SetEthernetNetworkDisconnected(
      const std::string& new_default_service_path) {
    helper_->SetServiceProperty(ethernet_service_path_,
                                std::string(shill::kStateProperty),
                                base::Value(shill::kStateIdle));
    test_task_runner_->RunUntilIdle();
    if (new_default_service_path.empty())
      return;

    helper_->manager_test()->SetManagerProperty(
        shill::kDefaultServiceProperty, base::Value(new_default_service_path));
  }

  void SetEthernetNetworkConnecting() {
    helper_->SetServiceProperty(ethernet_service_path_,
                                std::string(shill::kStateProperty),
                                base::Value(shill::kStateAssociation));
    test_task_runner_->RunUntilIdle();
    helper_->manager_test()->SetManagerProperty(
        shill::kDefaultServiceProperty, base::Value(ethernet_service_path_));
  }

  void SetEthernetNetworkConnected() {
    helper_->SetServiceProperty(ethernet_service_path_,
                                std::string(shill::kStateProperty),
                                base::Value(shill::kStateReady));
    test_task_runner_->RunUntilIdle();
    helper_->manager_test()->SetManagerProperty(
        shill::kDefaultServiceProperty, base::Value(ethernet_service_path_));
  }

  // Adds a Tether network state, adds a Wifi network to be used as the Wifi
  // hotspot, and associates the two networks. Returns the service path of the
  // Wifi network.
  std::string AddTetherNetworkState() {
    helper_->network_state_handler()->AddTetherNetworkState(
        kTetherGuid, "name", "carrier", 100 /* battery_percentage */,
        100 /* signal strength */, false /* has_connected_to_host */);
    std::string wifi_service_path =
        helper_->ConfigureService(CreateConfigurationJsonString(
            kWifiServiceGuid, shill::kTypeWifi, shill::kStateReady));
    helper_->network_state_handler()
        ->AssociateTetherNetworkStateWithWifiNetwork(kTetherGuid,
                                                     kWifiServiceGuid);
    return wifi_service_path;
  }

  void SetScreenLockedState(bool is_locked) {
    session_manager_->SetSessionState(
        is_locked ? session_manager::SessionState::LOCKED
                  : session_manager::SessionState::LOGIN_PRIMARY);
  }

  void VerifyScanDuration(size_t expected_num_seconds) {
    histogram_tester_->ExpectTimeBucketCount(
        "InstantTethering.HostScanBatchDuration",
        base::TimeDelta::FromSeconds(expected_num_seconds), 1u);
  }

  NetworkStateHandler* network_state_handler() {
    return helper_->network_state_handler();
  }

  base::test::TaskEnvironment task_environment_;
  std::string ethernet_service_path_;

  std::unique_ptr<NetworkStateTestHelper> helper_;
  std::unique_ptr<FakeHostScanner> fake_host_scanner_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;

  base::MockOneShotTimer* mock_host_scan_batch_timer_;
  base::SimpleTestClock test_clock_;
  scoped_refptr<base::TestSimpleTaskRunner> test_task_runner_;

  std::unique_ptr<base::HistogramTester> histogram_tester_;

  std::unique_ptr<HostScanSchedulerImpl> host_scan_scheduler_;
};

TEST_F(HostScanSchedulerImplTest, AttemptScanIfOffline) {
  host_scan_scheduler_->AttemptScanIfOffline();
  EXPECT_EQ(1u, fake_host_scanner_->num_scans_started());
  EXPECT_TRUE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  test_clock_.Advance(base::TimeDelta::FromSeconds(5));
  fake_host_scanner_->StopScan();
  EXPECT_EQ(1u, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  // Fire the timer; the duration should be recorded.
  mock_host_scan_batch_timer_->Fire();
  VerifyScanDuration(5u /* expected_num_sections */);
}

TEST_F(HostScanSchedulerImplTest, TestDeviceLockAndUnlock_Offline) {
  // Lock the screen. This should never trigger a scan.
  SetScreenLockedState(true /* is_locked */);
  EXPECT_EQ(0u, fake_host_scanner_->num_scans_started());

  // Try to start a scan. Because the screen is locked, this should not
  // cause a scan to be started.
  host_scan_scheduler_->AttemptScanIfOffline();
  EXPECT_EQ(0u, fake_host_scanner_->num_scans_started());

  // Unlock the screen. Because the device is offline, a new scan should have
  // started.
  SetScreenLockedState(false /* is_locked */);
  EXPECT_EQ(1u, fake_host_scanner_->num_scans_started());
}

TEST_F(HostScanSchedulerImplTest, TestDeviceLockAndUnlock_Online) {
  // Simulate the device being online.
  InitializeEthernet(true /* is_initially_connected */);

  // Lock the screen. This should never trigger a scan.
  SetScreenLockedState(true /* is_locked */);
  EXPECT_EQ(0u, fake_host_scanner_->num_scans_started());

  // Try to start a scan. Because the screen is locked, this should not
  // cause a scan to be started.
  host_scan_scheduler_->AttemptScanIfOffline();
  EXPECT_EQ(0u, fake_host_scanner_->num_scans_started());

  // Unlock the screen. Because the device is online, a new scan should not have
  // started.
  SetScreenLockedState(false /* is_locked */);
  EXPECT_EQ(0u, fake_host_scanner_->num_scans_started());
}

TEST_F(HostScanSchedulerImplTest, ScanRequested) {
  // Begin scanning.
  RequestScan(NetworkTypePattern::Tether());
  EXPECT_EQ(1u, fake_host_scanner_->num_scans_started());
  EXPECT_TRUE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  // Should not begin a new scan while a scan is active.
  RequestScan(NetworkTypePattern::Tether());
  EXPECT_EQ(1u, fake_host_scanner_->num_scans_started());
  EXPECT_TRUE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  test_clock_.Advance(base::TimeDelta::FromSeconds(5));
  fake_host_scanner_->StopScan();
  EXPECT_EQ(1u, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));
  mock_host_scan_batch_timer_->Fire();
  VerifyScanDuration(5u /* expected_num_sections */);

  // A new scan should be allowed once a scan is not active.
  RequestScan(NetworkTypePattern::Tether());
  EXPECT_EQ(2u, fake_host_scanner_->num_scans_started());
  EXPECT_TRUE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));
}

TEST_F(HostScanSchedulerImplTest, ScanRequested_NonMatchingNetworkTypePattern) {
  RequestScan(NetworkTypePattern::WiFi());
  EXPECT_EQ(0u, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));
}

TEST_F(HostScanSchedulerImplTest, HostScanSchedulerDestroyed) {
  host_scan_scheduler_->AttemptScanIfOffline();
  EXPECT_TRUE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  test_clock_.Advance(base::TimeDelta::FromSeconds(5));

  // Delete |host_scan_scheduler_|, which should cause the metric to be logged.
  host_scan_scheduler_.reset();
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));
  VerifyScanDuration(5u /* expected_num_sections */);
}

TEST_F(HostScanSchedulerImplTest, HostScanBatchMetric) {
  // The first scan takes 5 seconds. After stopping, the timer should be
  // running.
  host_scan_scheduler_->AttemptScanIfOffline();
  test_clock_.Advance(base::TimeDelta::FromSeconds(5));
  fake_host_scanner_->StopScan();
  EXPECT_TRUE(mock_host_scan_batch_timer_->IsRunning());

  // Advance the clock by 1 second and start another scan. The timer should have
  // been stopped.
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  EXPECT_LT(base::TimeDelta::FromSeconds(1),
            mock_host_scan_batch_timer_->GetCurrentDelay());
  host_scan_scheduler_->AttemptScanIfOffline();
  EXPECT_FALSE(mock_host_scan_batch_timer_->IsRunning());

  // Stop the scan; the duration should not have been recorded, and the timer
  // should be running again.
  test_clock_.Advance(base::TimeDelta::FromSeconds(5));
  fake_host_scanner_->StopScan();
  EXPECT_TRUE(mock_host_scan_batch_timer_->IsRunning());

  // Advance the clock by 59 seconds and start another scan. The timer should
  // have been stopped.
  test_clock_.Advance(base::TimeDelta::FromSeconds(59));
  EXPECT_LT(base::TimeDelta::FromSeconds(59),
            mock_host_scan_batch_timer_->GetCurrentDelay());
  host_scan_scheduler_->AttemptScanIfOffline();
  EXPECT_FALSE(mock_host_scan_batch_timer_->IsRunning());

  // Stop the scan; the duration should not have been recorded, and the timer
  // should be running again.
  test_clock_.Advance(base::TimeDelta::FromSeconds(5));
  fake_host_scanner_->StopScan();
  EXPECT_TRUE(mock_host_scan_batch_timer_->IsRunning());

  // Advance the clock by 60 seconds, which should be equal to the timer's
  // delay. Since this is a MockTimer, we need to manually fire the timer.
  test_clock_.Advance(base::TimeDelta::FromSeconds(60));
  EXPECT_EQ(base::TimeDelta::FromSeconds(60),
            mock_host_scan_batch_timer_->GetCurrentDelay());
  mock_host_scan_batch_timer_->Fire();

  // The scan duration should be equal to the three 5-second scans as well as
  // the 1-second and 59-second breaks between the three scans.
  VerifyScanDuration(5u + 1u + 5u + 59u + 5u /* expected_num_sections */);

  // Now, start a new 5-second scan, then wait for the timer to fire. A new
  // batch duration should have been logged to metrics.
  host_scan_scheduler_->AttemptScanIfOffline();
  test_clock_.Advance(base::TimeDelta::FromSeconds(5));
  fake_host_scanner_->StopScan();
  EXPECT_TRUE(mock_host_scan_batch_timer_->IsRunning());
  test_clock_.Advance(base::TimeDelta::FromSeconds(60));
  EXPECT_EQ(base::TimeDelta::FromSeconds(60),
            mock_host_scan_batch_timer_->GetCurrentDelay());
  mock_host_scan_batch_timer_->Fire();
  VerifyScanDuration(5u /* expected_num_sections */);
}

TEST_F(HostScanSchedulerImplTest, DefaultNetworkChanged) {
  InitializeEthernet(false /* is_initially_connected */);

  // When no Tether network is present, a scan should start when the default
  // network is disconnected.
  SetEthernetNetworkConnecting();
  EXPECT_EQ(0u, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetEthernetNetworkConnected();
  EXPECT_EQ(0u, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetEthernetNetworkDisconnected(std::string() /* default_service_path */);
  EXPECT_EQ(1u, fake_host_scanner_->num_scans_started());
  EXPECT_TRUE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  fake_host_scanner_->StopScan();

  // When Tether is present but disconnected, a scan should start when the
  // default network is disconnected.
  std::string tether_service_path = AddTetherNetworkState();
  EXPECT_EQ(1u, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetEthernetNetworkConnecting();
  EXPECT_EQ(1u, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetEthernetNetworkConnected();
  EXPECT_EQ(1u, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetEthernetNetworkDisconnected(std::string() /* default_service_path */);
  EXPECT_EQ(2u, fake_host_scanner_->num_scans_started());
  EXPECT_TRUE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  fake_host_scanner_->StopScan();

  // When Tether is present and connecting, no scan should start when an
  // Ethernet network becomes the default network and then disconnects.
  network_state_handler()->SetTetherNetworkStateConnecting(kTetherGuid);

  EXPECT_EQ(2u, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetEthernetNetworkConnecting();
  EXPECT_EQ(2u, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetEthernetNetworkConnected();
  EXPECT_EQ(2u, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetEthernetNetworkDisconnected(tether_service_path);
  EXPECT_EQ(2u, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  fake_host_scanner_->StopScan();

  // When Tether is present and connected, no scan should start when an Ethernet
  // network becomes the default network and then disconnects.
  base::RunLoop().RunUntilIdle();
  network_state_handler()->SetTetherNetworkStateConnected(kTetherGuid);

  EXPECT_EQ(2u, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetEthernetNetworkConnecting();
  EXPECT_EQ(2u, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetEthernetNetworkConnected();
  EXPECT_EQ(2u, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetEthernetNetworkDisconnected(tether_service_path);
  EXPECT_EQ(2u, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));
}

}  // namespace tether

}  // namespace chromeos
