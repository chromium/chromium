// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scan_scheduler_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_simple_task_runner.h"
#include "base/timer/mock_timer.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/components/tether/fake_host_scanner.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test.h"
#include "chromeos/network/network_type_pattern.h"
#include "components/cryptauth/cryptauth_device_manager.h"
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
                                          const std::string& type) {
  std::stringstream ss;
  ss << "{"
     << "  \"GUID\": \"" << guid << "\","
     << "  \"Type\": \"" << type << "\","
     << "  \"State\": \"" << shill::kStateReady << "\""
     << "}";
  return ss.str();
}

}  // namespace

class HostScanSchedulerImplTest : public NetworkStateTest {
 protected:
  void SetUp() override {
    DBusThreadManager::Initialize();
    NetworkStateTest::SetUp();
  }

  void TearDown() override {
    host_scan_scheduler_.reset();

    ShutdownNetworkState();
    NetworkStateTest::TearDown();
    DBusThreadManager::Shutdown();
  }

  void InitializeTest(bool multidevice_flags_enabled) {
    SetMultiDeviceApi(multidevice_flags_enabled);

    histogram_tester_ = std::make_unique<base::HistogramTester>();

    network_state_handler()->SetTetherTechnologyState(
        NetworkStateHandler::TECHNOLOGY_ENABLED);

    fake_host_scanner_ = std::make_unique<FakeHostScanner>();
    session_manager_ = std::make_unique<session_manager::SessionManager>();

    host_scan_scheduler_ = std::make_unique<HostScanSchedulerImpl>(
        network_state_handler(), fake_host_scanner_.get(),
        session_manager_.get());

    mock_host_scan_batch_timer_ = new base::MockOneShotTimer();
    mock_delay_scan_after_unlock_timer_ = new base::MockOneShotTimer();

    // Advance the clock by an arbitrary value to ensure that when Now() is
    // called, the Unix epoch will not be returned.
    test_clock_.Advance(base::TimeDelta::FromSeconds(10));
    test_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    host_scan_scheduler_->SetTestDoubles(
        base::WrapUnique(mock_host_scan_batch_timer_),
        base::WrapUnique(mock_delay_scan_after_unlock_timer_), &test_clock_,
        test_task_runner_);
  }

  void SetMultiDeviceApi(bool enabled) {
    static const std::vector<base::Feature> kFeatures{
        chromeos::features::kMultiDeviceApi,
        chromeos::features::kEnableUnifiedMultiDeviceSetup};

    scoped_feature_list_.InitWithFeatures(
        (enabled ? kFeatures
                 : std::vector<base::Feature>() /* enable_features */),
        (enabled ? std::vector<base::Feature>()
                 : kFeatures /* disable_features */));
  }

  void RequestScan(const NetworkTypePattern& type) {
    host_scan_scheduler_->ScanRequested(type);
  }

  void InitializeEthernet() {
    ethernet_service_path_ = ConfigureService(CreateConfigurationJsonString(
        kEthernetServiceGuid, shill::kTypeEthernet));
    test_manager_client()->SetManagerProperty(
        shill::kDefaultServiceProperty, base::Value(ethernet_service_path_));
  }

  // Disconnects the Ethernet network and manually sets the default network to
  // |new_default_service_path|. If |new_default_service_path| is empty then no
  // default network is set.
  void SetEthernetNetworkDisconnected(
      const std::string& new_default_service_path) {
    SetServiceProperty(ethernet_service_path_,
                       std::string(shill::kStateProperty),
                       base::Value(shill::kStateIdle));
    test_task_runner_->RunUntilIdle();
    if (new_default_service_path.empty())
      return;

    test_manager_client()->SetManagerProperty(
        shill::kDefaultServiceProperty, base::Value(new_default_service_path));
  }

  void SetEthernetNetworkConnecting() {
    SetServiceProperty(ethernet_service_path_,
                       std::string(shill::kStateProperty),
                       base::Value(shill::kStateAssociation));
    test_task_runner_->RunUntilIdle();
    test_manager_client()->SetManagerProperty(
        shill::kDefaultServiceProperty, base::Value(ethernet_service_path_));
  }

  void SetEthernetNetworkConnected() {
    SetServiceProperty(ethernet_service_path_,
                       std::string(shill::kStateProperty),
                       base::Value(shill::kStateReady));
    test_task_runner_->RunUntilIdle();
    test_manager_client()->SetManagerProperty(
        shill::kDefaultServiceProperty, base::Value(ethernet_service_path_));
  }

  // Adds a Tether network state, adds a Wifi network to be used as the Wifi
  // hotspot, and associates the two networks. Returns the service path of the
  // Wifi network.
  std::string AddTetherNetworkState() {
    network_state_handler()->AddTetherNetworkState(
        kTetherGuid, "name", "carrier", 100 /* battery_percentage */,
        100 /* signal strength */, false /* has_connected_to_host */);
    std::string wifi_service_path = ConfigureService(
        CreateConfigurationJsonString(kWifiServiceGuid, shill::kTypeWifi));
    network_state_handler()->AssociateTetherNetworkStateWithWifiNetwork(
        kTetherGuid, kWifiServiceGuid);
    return wifi_service_path;
  }

  void SetScreenLockedState(bool is_locked) {
    session_manager_->SetSessionState(
        is_locked ? session_manager::SessionState::LOCKED
                  : session_manager::SessionState::LOGIN_PRIMARY);
  }

  void TestDeviceLockAndUnlock(bool is_online) {
    if (is_online)
      InitializeEthernet();

    // Lock the screen. This should never trigger a scan.
    SetScreenLockedState(true /* is_locked */);
    EXPECT_EQ(0u, fake_host_scanner_->num_scans_started());
    EXPECT_FALSE(mock_delay_scan_after_unlock_timer_->IsRunning());

    // Try to start a scan. Because the screen is locked, this should not
    // cause a scan to be started.
    host_scan_scheduler_->AttemptScanIfOffline();
    EXPECT_EQ(0u, fake_host_scanner_->num_scans_started());
    EXPECT_FALSE(mock_delay_scan_after_unlock_timer_->IsRunning());

    // Unlock the screen. A scan should not yet have been scheduled yet, but the
    // timer should have.
    SetScreenLockedState(false /* is_locked */);
    EXPECT_EQ(0u, fake_host_scanner_->num_scans_started());
    ASSERT_TRUE(mock_delay_scan_after_unlock_timer_->IsRunning());

    // Fire the timer; this should only start the scan if the device is offline.
    mock_delay_scan_after_unlock_timer_->Fire();
    EXPECT_EQ(is_online ? 0u : 1u, fake_host_scanner_->num_scans_started());
  }

  void TestDeviceLockAndUnlock_MultiDeviceApiEnabled(bool is_online) {
    if (is_online)
      InitializeEthernet();

    // Lock the screen. This should never trigger a scan.
    SetScreenLockedState(true /* is_locked */);
    EXPECT_EQ(0u, fake_host_scanner_->num_scans_started());
    EXPECT_FALSE(mock_delay_scan_after_unlock_timer_->IsRunning());

    // Try to start a scan. Because the screen is locked, this should not
    // cause a scan to be started.
    host_scan_scheduler_->AttemptScanIfOffline();
    EXPECT_EQ(0u, fake_host_scanner_->num_scans_started());
    EXPECT_FALSE(mock_delay_scan_after_unlock_timer_->IsRunning());

    // Unlock the screen. If the device is offline, a new scan should have
    // started. The timer should be untouched.
    SetScreenLockedState(false /* is_locked */);
    EXPECT_EQ(is_online ? 0u : 1u, fake_host_scanner_->num_scans_started());
    EXPECT_FALSE(mock_delay_scan_after_unlock_timer_->IsRunning());
  }

  void VerifyScanDuration(size_t expected_num_seconds) {
    histogram_tester_->ExpectTimeBucketCount(
        "InstantTethering.HostScanBatchDuration",
        base::TimeDelta::FromSeconds(expected_num_seconds), 1u);
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::string ethernet_service_path_;

  std::unique_ptr<FakeHostScanner> fake_host_scanner_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;

  base::MockOneShotTimer* mock_host_scan_batch_timer_;
  base::MockOneShotTimer* mock_delay_scan_after_unlock_timer_;
  base::SimpleTestClock test_clock_;
  scoped_refptr<base::TestSimpleTaskRunner> test_task_runner_;

  std::unique_ptr<base::HistogramTester> histogram_tester_;

  std::unique_ptr<HostScanSchedulerImpl> host_scan_scheduler_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(HostScanSchedulerImplTest, AttemptScanIfOffline) {
  InitializeTest(false /* multidevice_flags_enabled */);
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
  InitializeTest(false /* multidevice_flags_enabled */);
  TestDeviceLockAndUnlock(false /* is_online */);
}

TEST_F(HostScanSchedulerImplTest, TestDeviceLockAndUnlock_Online) {
  InitializeTest(false /* multidevice_flags_enabled */);
  TestDeviceLockAndUnlock(true /* is_online */);
}

TEST_F(HostScanSchedulerImplTest,
       TestDeviceLockAndUnlock_MultiDeviceApiEnabled_Offline) {
  InitializeTest(true /* multidevice_flags_enabled */);
  TestDeviceLockAndUnlock_MultiDeviceApiEnabled(false /* is_online */);
}

TEST_F(HostScanSchedulerImplTest,
       TestDeviceLockAndUnlock_MultiDeviceApiEnabled_Online) {
  InitializeTest(true /* multidevice_flags_enabled */);
  TestDeviceLockAndUnlock_MultiDeviceApiEnabled(true /* is_online */);
}

TEST_F(HostScanSchedulerImplTest, ScanRequested) {
  InitializeTest(false /* multidevice_flags_enabled */);

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
  InitializeTest(false /* multidevice_flags_enabled */);
  RequestScan(NetworkTypePattern::WiFi());
  EXPECT_EQ(0u, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));
}

TEST_F(HostScanSchedulerImplTest, HostScanSchedulerDestroyed) {
  InitializeTest(false /* multidevice_flags_enabled */);

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
  InitializeTest(false /* multidevice_flags_enabled */);

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
  InitializeTest(false /* multidevice_flags_enabled */);
  InitializeEthernet();

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
