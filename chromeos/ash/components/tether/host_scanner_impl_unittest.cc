// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/host_scanner_impl.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/ash/components/tether/fake_connection_preserver.h"
#include "chromeos/ash/components/tether/fake_host_scan_cache.h"
#include "chromeos/ash/components/tether/fake_notification_presenter.h"
#include "chromeos/ash/components/tether/fake_tether_host_fetcher.h"
#include "chromeos/ash/components/tether/gms_core_notifications_state_tracker_impl.h"
#include "chromeos/ash/components/tether/host_scanner.h"
#include "chromeos/ash/components/tether/mock_tether_host_response_recorder.h"
#include "chromeos/ash/components/tether/proto_test_util.h"
#include "chromeos/ash/components/tether/top_level_host_scan_cache.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "components/session_manager/core/session_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace tether {

namespace {

class TestObserver final : public HostScanner::Observer {
 public:
  void ScanFinished() override { scan_finished_count_++; }

  int scan_finished_count() { return scan_finished_count_; }

 private:
  int scan_finished_count_ = 0;
};

class FakeTetherAvailabilityOperationOrchestrator
    : public TetherAvailabilityOperationOrchestrator {
 public:
  FakeTetherAvailabilityOperationOrchestrator()
      : TetherAvailabilityOperationOrchestrator(
            /*tether_availability_operation_initializer=*/nullptr) {}

  ~FakeTetherAvailabilityOperationOrchestrator() override = default;

  void SendScannedDeviceListUpdate(
      const std::vector<ScannedDeviceInfo>& scanned_device_list_so_far,
      bool is_final_scan_result) {
    for (auto& observer : observers_) {
      observer.OnTetherAvailabilityResponse(
          scanned_device_list_so_far, gms_core_notifications_disabled_devices_,
          is_final_scan_result);
    }
    scanned_device_list_so_far_ = scanned_device_list_so_far;
  }

  void Start() override { is_started_ = true; }

  bool is_started() { return is_started_; }

 private:
  bool is_started_ = false;
};

class FakeTetherAvailabilityOperationOrchestratorFactory
    : public TetherAvailabilityOperationOrchestrator::Factory {
 public:
  FakeTetherAvailabilityOperationOrchestratorFactory() {}

  ~FakeTetherAvailabilityOperationOrchestratorFactory() override {}

  std::unique_ptr<TetherAvailabilityOperationOrchestrator> CreateInstance()
      override {
    FakeTetherAvailabilityOperationOrchestrator* orchestrator =
        new FakeTetherAvailabilityOperationOrchestrator();
    created_orchestrators_.push_back(orchestrator);
    return base::WrapUnique(orchestrator);
  }

  std::vector<
      raw_ptr<FakeTetherAvailabilityOperationOrchestrator, VectorExperimental>>&
  created_orchestrators() {
    return created_orchestrators_;
  }

 private:
  std::vector<
      raw_ptr<FakeTetherAvailabilityOperationOrchestrator, VectorExperimental>>
      created_orchestrators_;
};

std::string GenerateCellProviderForDevice(
    multidevice::RemoteDeviceRef remote_device) {
  // Return a string unique to |remote_device|.
  return "cellProvider" + remote_device.GetTruncatedDeviceIdForLogs();
}

std::vector<ScannedDeviceInfo> CreateFakeScannedDeviceInfos(
    const multidevice::RemoteDeviceRefList& remote_devices) {
  // At least 4 ScannedDeviceInfos should be created to ensure that all 4 cases
  // described below are tested.
  EXPECT_GT(remote_devices.size(), 3u);

  std::vector<ScannedDeviceInfo> scanned_device_infos;

  for (size_t i = 0; i < remote_devices.size(); ++i) {
    // Four field possibilities:
    // i % 4 == 0: Field is not supplied.
    // i % 4 == 1: Field is below the minimum value (int fields only).
    // i % 4 == 2: Field is within the valid range (int fields only).
    // i % 4 == 3: Field is above the maximium value (int fields only).
    std::string cell_provider_name;
    int battery_percentage;
    int connection_strength;
    switch (i % 4) {
      case 0:
        cell_provider_name = proto_test_util::kDoNotSetStringField;
        battery_percentage = proto_test_util::kDoNotSetIntField;
        connection_strength = proto_test_util::kDoNotSetIntField;
        break;
      case 1:
        cell_provider_name = GenerateCellProviderForDevice(remote_devices[i]);
        battery_percentage = -1 - i;
        connection_strength = -1 - i;
        break;
      case 2:
        cell_provider_name = GenerateCellProviderForDevice(remote_devices[i]);
        battery_percentage = (50 + i) % 100;  // Valid range is [0, 100].
        connection_strength = (1 + i) % 4;    // Valid range is [0, 4].
        break;
      case 3:
        cell_provider_name = GenerateCellProviderForDevice(remote_devices[i]);
        battery_percentage = 101 + i;
        connection_strength = 101 + i;
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        // Set values for |battery_percentage| and |connection_strength| here to
        // prevent a compiler warning which says that they may be unset at this
        // point.
        battery_percentage = 0;
        connection_strength = 0;
        break;
    }

    DeviceStatus device_status = CreateTestDeviceStatus(
        cell_provider_name, battery_percentage, connection_strength);

    // Require set-up for odd-numbered device indices.
    bool setup_required = i % 2 == 0;

    scanned_device_infos.push_back(ScannedDeviceInfo(
        remote_devices[i].GetDeviceId(), remote_devices[i].name(),
        device_status, setup_required, /*notifications_enabled=*/true));
  }

  return scanned_device_infos;
}

}  // namespace

class HostScannerImplTest : public testing::Test {
 public:
  HostScannerImplTest(const HostScannerImplTest&) = delete;
  HostScannerImplTest& operator=(const HostScannerImplTest&) = delete;

 protected:
  HostScannerImplTest()
      : test_devices_(multidevice::CreateRemoteDeviceRefListForTest(4)),
        test_scanned_device_infos(CreateFakeScannedDeviceInfos(test_devices_)) {
  }

  void SetUp() override {
    scanned_device_infos_from_current_scan_.clear();

    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_secure_channel_client_ =
        std::make_unique<secure_channel::FakeSecureChannelClient>();
    session_manager_ = std::make_unique<session_manager::SessionManager>();
    fake_tether_host_fetcher_ =
        std::make_unique<FakeTetherHostFetcher>(test_devices_[0]);
    mock_tether_host_response_recorder_ =
        std::make_unique<MockTetherHostResponseRecorder>();
    gms_core_notifications_state_tracker_ =
        std::make_unique<GmsCoreNotificationsStateTrackerImpl>();
    fake_notification_presenter_ =
        std::make_unique<FakeNotificationPresenter>();
    device_id_tether_network_guid_map_ =
        std::make_unique<DeviceIdTetherNetworkGuidMap>();
    fake_host_scan_cache_ = std::make_unique<FakeHostScanCache>();

    fake_tether_availability_operation_factory_ =
        new FakeTetherAvailabilityOperationOrchestratorFactory();

    fake_connection_preserver_ = std::make_unique<FakeConnectionPreserver>();

    test_clock_ = std::make_unique<base::SimpleTestClock>();

    host_scanner_ = base::WrapUnique(new HostScannerImpl(
        base::WrapUnique<TetherAvailabilityOperationOrchestrator::Factory>(
            fake_tether_availability_operation_factory_),
        helper_.network_state_handler(), session_manager_.get(),
        gms_core_notifications_state_tracker_.get(),
        fake_notification_presenter_.get(),
        device_id_tether_network_guid_map_.get(), fake_host_scan_cache_.get(),
        test_clock_.get()));

    test_observer_ = std::make_unique<TestObserver>();
    host_scanner_->AddObserver(test_observer_.get());
  }

  void TearDown() override {
    host_scanner_->RemoveObserver(test_observer_.get());
  }

  // Causes |fake_orchestrator| to receive the scan result in
  // |test_scanned_device_infos| vector at the index |test_device_index| with
  // the "final result" value of |is_final_scan_result|.
  void ReceiveScanResultAndVerifySuccess(
      FakeTetherAvailabilityOperationOrchestrator* fake_orchestrator,
      size_t test_device_index,
      bool is_final_scan_result,
      NotificationPresenter::PotentialHotspotNotificationState
          expected_notification_state,
      int num_expected_host_scan_histogram_samples = 1) {
    bool already_in_list = false;
    for (auto& scanned_device_info : scanned_device_infos_from_current_scan_) {
      if (scanned_device_info.device_id ==
          test_devices_[test_device_index].GetDeviceId()) {
        already_in_list = true;
        break;
      }
    }

    if (!already_in_list) {
      scanned_device_infos_from_current_scan_.push_back(
          test_scanned_device_infos[test_device_index]);
    }

    int previous_scan_finished_count = test_observer_->scan_finished_count();
    fake_orchestrator->SendScannedDeviceListUpdate(
        scanned_device_infos_from_current_scan_, is_final_scan_result);
    VerifyScanResultsMatchCache();
    EXPECT_EQ(previous_scan_finished_count + (is_final_scan_result ? 1 : 0),
              test_observer_->scan_finished_count());

    EXPECT_EQ(
        expected_notification_state,
        fake_notification_presenter_->GetPotentialHotspotNotificationState());

    if (is_final_scan_result) {
      HostScannerImpl::HostScanResultEventType expected_event_type =
          HostScannerImpl::HostScanResultEventType::NO_HOSTS_FOUND;
      if (!scanned_device_infos_from_current_scan_.empty() &&
          expected_notification_state ==
              NotificationPresenter::PotentialHotspotNotificationState::
                  NO_HOTSPOT_NOTIFICATION_SHOWN) {
        expected_event_type = HostScannerImpl::HostScanResultEventType::
            HOSTS_FOUND_BUT_NO_NOTIFICATION_SHOWN;
      } else if (scanned_device_infos_from_current_scan_.size() == 1) {
        expected_event_type = HostScannerImpl::HostScanResultEventType::
            NOTIFICATION_SHOWN_SINGLE_HOST;
      } else if (scanned_device_infos_from_current_scan_.size() > 1) {
        expected_event_type = HostScannerImpl::HostScanResultEventType::
            NOTIFICATION_SHOWN_MULTIPLE_HOSTS;
      }
      histogram_tester_.ExpectUniqueSample(
          "InstantTethering.HostScanResult", expected_event_type,
          num_expected_host_scan_histogram_samples);
    }
  }

  void VerifyScanResultsMatchCache() {
    std::vector<ScannedDeviceInfo> combined_device_infos =
        scanned_device_infos_from_current_scan_;
    for (const auto& previous_scan_result :
         scanned_device_infos_from_previous_scans_) {
      bool already_in_combined = false;
      for (const auto& combined_device_info : combined_device_infos) {
        if (previous_scan_result.device_id == combined_device_info.device_id) {
          already_in_combined = true;
          break;
        }
      }
      if (!already_in_combined) {
        combined_device_infos.push_back(previous_scan_result);
      }
    }

    ASSERT_EQ(combined_device_infos.size(), fake_host_scan_cache_->size());
    for (auto& scanned_device_info : combined_device_infos) {
      std::string tether_network_guid =
          device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
              scanned_device_info.device_id);
      const HostScanCacheEntry* entry =
          fake_host_scan_cache_->GetCacheEntry(tether_network_guid);
      ASSERT_TRUE(entry);
      VerifyScannedDeviceInfoAndCacheEntryAreEquivalent(scanned_device_info,
                                                        *entry);
    }
  }

  void VerifyScannedDeviceInfoAndCacheEntryAreEquivalent(
      const ScannedDeviceInfo& scanned_device_info,
      const HostScanCacheEntry& entry) {
    EXPECT_EQ(scanned_device_info.device_name, entry.device_name);

    const DeviceStatus& status = scanned_device_info.device_status.value();
    if (!status.has_cell_provider() || status.cell_provider().empty()) {
      EXPECT_EQ("unknown-carrier", entry.carrier);
    } else {
      EXPECT_EQ(status.cell_provider(), entry.carrier);
    }

    if (!status.has_battery_percentage() || status.battery_percentage() > 100) {
      EXPECT_EQ(100, entry.battery_percentage);
    } else if (status.battery_percentage() < 0) {
      EXPECT_EQ(0, entry.battery_percentage);
    } else {
      EXPECT_EQ(status.battery_percentage(), entry.battery_percentage);
    }

    if (!status.has_connection_strength() || status.connection_strength() > 4) {
      EXPECT_EQ(100, entry.signal_strength);
    } else if (status.connection_strength() < 0) {
      EXPECT_EQ(0, entry.signal_strength);
    } else {
      EXPECT_EQ(status.connection_strength() * 25, entry.signal_strength);
    }

    EXPECT_EQ(scanned_device_info.setup_required, entry.setup_required);
  }

  // Clears scan results from |scanned_device_infos_from_current_scan_| and
  // transfers them to |scanned_device_infos_from_previous_scans_|.
  void ClearCurrentScanResults() {
    scanned_device_infos_from_previous_scans_ =
        scanned_device_infos_from_current_scan_;
    scanned_device_infos_from_current_scan_.clear();
  }

  void StartConnectingToWifiNetwork() {
    std::stringstream ss;
    ss << "{" << "  \"GUID\": \"wifiNetworkGuid\"," << "  \"Type\": \""
       << shill::kTypeWifi << "\"," << "  \"State\": \""
       << shill::kStateConfiguration << "\"" << "}";

    helper_.ConfigureService(ss.str());
  }

  void SetScreenLockedState(bool is_locked) {
    session_manager_->SetSessionState(
        is_locked ? session_manager::SessionState::LOCKED
                  : session_manager::SessionState::LOGIN_PRIMARY);
  }

  NetworkStateHandler* network_state_handler() {
    return helper_.network_state_handler();
  }

  base::test::TaskEnvironment task_environment_;

  NetworkStateTestHelper helper_{/*use_default_devices_and_services=*/true};
  const multidevice::RemoteDeviceRefList test_devices_;
  const std::vector<ScannedDeviceInfo> test_scanned_device_infos;

  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<secure_channel::SecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  std::unique_ptr<FakeTetherHostFetcher> fake_tether_host_fetcher_;
  std::unique_ptr<MockTetherHostResponseRecorder>
      mock_tether_host_response_recorder_;
  std::unique_ptr<GmsCoreNotificationsStateTrackerImpl>
      gms_core_notifications_state_tracker_;
  std::unique_ptr<FakeNotificationPresenter> fake_notification_presenter_;
  // TODO(hansberry): Use a fake for this when a real mapping scheme is created.
  std::unique_ptr<DeviceIdTetherNetworkGuidMap>
      device_id_tether_network_guid_map_;
  std::unique_ptr<FakeHostScanCache> fake_host_scan_cache_;
  std::unique_ptr<FakeConnectionPreserver> fake_connection_preserver_;

  std::unique_ptr<base::SimpleTestClock> test_clock_;
  std::unique_ptr<TestObserver> test_observer_;

  raw_ptr<FakeTetherAvailabilityOperationOrchestratorFactory>
      fake_tether_availability_operation_factory_;

  std::vector<ScannedDeviceInfo> scanned_device_infos_from_current_scan_;
  std::vector<ScannedDeviceInfo> scanned_device_infos_from_previous_scans_;

  std::unique_ptr<HostScanner> host_scanner_;

  base::HistogramTester histogram_tester_;
};

TEST_F(HostScannerImplTest, DISABLED_TestScan_ConnectingToExistingNetwork) {
  StartConnectingToWifiNetwork();
  EXPECT_TRUE(network_state_handler()->DefaultNetwork());

  EXPECT_FALSE(host_scanner_->IsScanActive());
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ASSERT_EQ(1u,
            fake_tether_availability_operation_factory_->created_orchestrators()
                .size());
  EXPECT_TRUE(host_scanner_->IsScanActive());

  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[0],
      0u /* test_device_index */, false /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          NO_HOTSPOT_NOTIFICATION_SHOWN);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[0],
      1u /* test_device_index */, false /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          NO_HOTSPOT_NOTIFICATION_SHOWN);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[0],
      2u /* test_device_index */, false /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          NO_HOTSPOT_NOTIFICATION_SHOWN);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[0],
      3u /* test_device_index */, true /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          NO_HOTSPOT_NOTIFICATION_SHOWN);
  EXPECT_FALSE(host_scanner_->IsScanActive());
}

TEST_F(HostScannerImplTest,
       DISABLED_TestNotificationNotDisplayedMultipleTimes) {
  StartConnectingToWifiNetwork();
  EXPECT_TRUE(network_state_handler()->DefaultNetwork());

  EXPECT_FALSE(host_scanner_->IsScanActive());
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ASSERT_EQ(1u,
            fake_tether_availability_operation_factory_->created_orchestrators()
                .size());
  EXPECT_TRUE(host_scanner_->IsScanActive());

  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[0],
      0u /* test_device_index */, false /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          NO_HOTSPOT_NOTIFICATION_SHOWN);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[0],
      1u /* test_device_index */, false /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          NO_HOTSPOT_NOTIFICATION_SHOWN);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[0],
      2u /* test_device_index */, false /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          NO_HOTSPOT_NOTIFICATION_SHOWN);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[0],
      3u /* test_device_index */, true /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          NO_HOTSPOT_NOTIFICATION_SHOWN);
  EXPECT_FALSE(host_scanner_->IsScanActive());
}

TEST_F(HostScannerImplTest,
       DISABLED_TestNotificationDisplaysMultipleTimesWhenUnlocked) {
  // Start a scan and receive a result.
  host_scanner_->StartScan();
  ASSERT_EQ(1u,
            fake_tether_availability_operation_factory_->created_orchestrators()
                .size());
  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[0],
      0u /* test_device_index */, true /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          SINGLE_HOTSPOT_NEARBY_SHOWN);
  EXPECT_FALSE(host_scanner_->IsScanActive());

  // The scan has completed, and the "single hotspot nearby" notification should
  // be shown. Remove it.
  EXPECT_EQ(
      NotificationPresenter::PotentialHotspotNotificationState::
          SINGLE_HOTSPOT_NEARBY_SHOWN,
      fake_notification_presenter_->GetPotentialHotspotNotificationState());
  fake_notification_presenter_->RemovePotentialHotspotNotification();

  // Now, simulate locking then unlocking the device.
  SetScreenLockedState(true /* is_locked */);
  SetScreenLockedState(false /* is_locked */);

  // Start another scan and receive a result.
  host_scanner_->StartScan();
  ASSERT_EQ(2u,
            fake_tether_availability_operation_factory_->created_orchestrators()
                .size());
  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[1],
      0u /* test_device_index */, true /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          SINGLE_HOTSPOT_NEARBY_SHOWN,
      2 /* num_expected_host_scan_histogram_samples */);
  EXPECT_FALSE(host_scanner_->IsScanActive());

  // The notification should be visible since the device was locked and unlocked
  // since the last time it was shown.
  EXPECT_EQ(
      NotificationPresenter::PotentialHotspotNotificationState::
          SINGLE_HOTSPOT_NEARBY_SHOWN,
      fake_notification_presenter_->GetPotentialHotspotNotificationState());
}

TEST_F(HostScannerImplTest, DISABLED_TestScan_ResultsFromAllDevices) {
  EXPECT_FALSE(host_scanner_->IsScanActive());
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ASSERT_EQ(1u,
            fake_tether_availability_operation_factory_->created_orchestrators()
                .size());
  EXPECT_TRUE(host_scanner_->IsScanActive());

  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[0],
      0u /* test_device_index */, false /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          SINGLE_HOTSPOT_NEARBY_SHOWN);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[0],
      1u /* test_device_index */, false /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          MULTIPLE_HOTSPOTS_NEARBY_SHOWN);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[0],
      2u /* test_device_index */, false /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          MULTIPLE_HOTSPOTS_NEARBY_SHOWN);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[0],
      3u /* test_device_index */, true /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          MULTIPLE_HOTSPOTS_NEARBY_SHOWN);
  EXPECT_FALSE(host_scanner_->IsScanActive());
}

TEST_F(HostScannerImplTest, DISABLED_TestScan_ResultsFromNoDevices) {
  EXPECT_FALSE(host_scanner_->IsScanActive());
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ASSERT_EQ(1u,
            fake_tether_availability_operation_factory_->created_orchestrators()
                .size());
  EXPECT_TRUE(host_scanner_->IsScanActive());

  fake_tether_availability_operation_factory_->created_orchestrators()[0]
      ->SendScannedDeviceListUpdate(std::vector<ScannedDeviceInfo>(),
                                    true /* is_final_scan_result */);
  EXPECT_EQ(0u, fake_host_scan_cache_->size());
  EXPECT_FALSE(host_scanner_->IsScanActive());
}

TEST_F(HostScannerImplTest, DISABLED_StopScan) {
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ASSERT_EQ(1u,
            fake_tether_availability_operation_factory_->created_orchestrators()
                .size());
  EXPECT_TRUE(host_scanner_->IsScanActive());

  host_scanner_->StopScan();
  EXPECT_FALSE(host_scanner_->IsScanActive());
}

TEST_F(HostScannerImplTest, DISABLED_TestScan_ResultsFromSomeDevices) {
  EXPECT_FALSE(host_scanner_->IsScanActive());
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ASSERT_EQ(1u,
            fake_tether_availability_operation_factory_->created_orchestrators()
                .size());
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // Only receive updates from the 0th and 1st device.
  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[0],
      0u /* test_device_index */, false /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          SINGLE_HOTSPOT_NEARBY_SHOWN);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[0],
      1u /* test_device_index */, false /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          MULTIPLE_HOTSPOTS_NEARBY_SHOWN);
  EXPECT_TRUE(host_scanner_->IsScanActive());

  fake_tether_availability_operation_factory_->created_orchestrators()[0]
      ->SendScannedDeviceListUpdate(scanned_device_infos_from_current_scan_,
                                    true /* is_final_scan_result */);
  EXPECT_EQ(scanned_device_infos_from_current_scan_.size(),
            fake_host_scan_cache_->size());
  EXPECT_FALSE(host_scanner_->IsScanActive());
}

TEST_F(HostScannerImplTest,
       DISABLED_TestScan_MultipleScanCallsDuringOperation) {
  EXPECT_FALSE(host_scanner_->IsScanActive());
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ASSERT_EQ(1u,
            fake_tether_availability_operation_factory_->created_orchestrators()
                .size());
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // Call StartScan again before the final scan result has been received. This
  // should be a no-op.
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // No devices should have been received yet.
  EXPECT_EQ(0u, fake_host_scan_cache_->size());

  // Receive updates from the 0th device.
  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[0],
      0u /* test_device_index */, false /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          SINGLE_HOTSPOT_NEARBY_SHOWN);
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // Call StartScan again after a scan result has been received but before
  // the final scan result ha been received. This should be a no-op.
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // The scanned devices so far should be the same (i.e., they should not have
  // been affected by the extra call to StartScan()).
  EXPECT_EQ(scanned_device_infos_from_current_scan_.size(),
            fake_host_scan_cache_->size());

  // Finally, finish the scan.
  fake_tether_availability_operation_factory_->created_orchestrators()[0]
      ->SendScannedDeviceListUpdate(scanned_device_infos_from_current_scan_,
                                    true /* is_final_scan_result */);
  EXPECT_EQ(scanned_device_infos_from_current_scan_.size(),
            fake_host_scan_cache_->size());
  EXPECT_FALSE(host_scanner_->IsScanActive());
}

TEST_F(HostScannerImplTest, DISABLED_TestScan_MultipleCompleteScanSessions) {
  // Start the first scan session.
  EXPECT_FALSE(host_scanner_->IsScanActive());
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ASSERT_EQ(1u,
            fake_tether_availability_operation_factory_->created_orchestrators()
                .size());
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // Receive updates from devices 0-2.
  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[0],
      0u /* test_device_index */, false /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          SINGLE_HOTSPOT_NEARBY_SHOWN);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[0],
      1u /* test_device_index */, false /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          MULTIPLE_HOTSPOTS_NEARBY_SHOWN);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[0],
      2u /* test_device_index */, false /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          MULTIPLE_HOTSPOTS_NEARBY_SHOWN);
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // Finish the first scan.
  fake_tether_availability_operation_factory_->created_orchestrators()[0]
      ->SendScannedDeviceListUpdate(scanned_device_infos_from_current_scan_,
                                    true /* is_final_scan_result */);
  EXPECT_EQ(scanned_device_infos_from_current_scan_.size(),
            fake_host_scan_cache_->size());
  EXPECT_FALSE(host_scanner_->IsScanActive());

  // The notification should still be visible.
  EXPECT_EQ(
      NotificationPresenter::PotentialHotspotNotificationState::
          MULTIPLE_HOTSPOTS_NEARBY_SHOWN,
      fake_notification_presenter_->GetPotentialHotspotNotificationState());

  // Now, start the second scan session. Since the notification was still
  // visible from the first scan session, it should still be able to be shown
  // for the second scan session.
  ClearCurrentScanResults();
  EXPECT_FALSE(host_scanner_->IsScanActive());
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ASSERT_EQ(2u,
            fake_tether_availability_operation_factory_->created_orchestrators()
                .size());
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // The cache should be unaffected by the start of a new scan.
  VerifyScanResultsMatchCache();

  // Receive results from devices 0 only. Results from devices 1 and 2 should
  // still be present in the cache even though no results have been received
  // from that device during this scan session.
  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[1],
      0u /* test_device_index */, false /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          MULTIPLE_HOTSPOTS_NEARBY_SHOWN);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  VerifyScanResultsMatchCache();

  // Finish the second scan. Since results were not received from devices 1 or
  // 2, previous results from those devices should now be removed from the
  // cache.
  fake_tether_availability_operation_factory_->created_orchestrators()[1]
      ->SendScannedDeviceListUpdate(scanned_device_infos_from_current_scan_,
                                    true /* is_final_scan_result */);
  EXPECT_FALSE(host_scanner_->IsScanActive());

  // The notification should have been changed to a single hotspot. Remove it
  // before starting the third scan session.
  EXPECT_EQ(
      NotificationPresenter::PotentialHotspotNotificationState::
          SINGLE_HOTSPOT_NEARBY_SHOWN,
      fake_notification_presenter_->GetPotentialHotspotNotificationState());
  fake_notification_presenter_->RemovePotentialHotspotNotification();

  // Now, start the third scan session. Since the notification was hidden before
  // the session started, it should not be shown for this session.
  ClearCurrentScanResults();
  EXPECT_FALSE(host_scanner_->IsScanActive());
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ASSERT_EQ(3u,
            fake_tether_availability_operation_factory_->created_orchestrators()
                .size());
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // The cache should be unaffected by the start of a new scan.
  VerifyScanResultsMatchCache();

  // Receive results from devices 0, 2 and 3. Results from device 1 should still
  // be present in the cache even though no results have been received from that
  // device during this scan session.
  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[2],
      0u /* test_device_index */, false /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          NO_HOTSPOT_NOTIFICATION_SHOWN);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[2],
      2u /* test_device_index */, false /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          NO_HOTSPOT_NOTIFICATION_SHOWN);
  ReceiveScanResultAndVerifySuccess(
      fake_tether_availability_operation_factory_->created_orchestrators()[2],
      3u /* test_device_index */, false /* is_final_scan_result */,
      NotificationPresenter::PotentialHotspotNotificationState::
          NO_HOTSPOT_NOTIFICATION_SHOWN);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  VerifyScanResultsMatchCache();

  // Finish the second scan. Since results were not received from device 1,
  // previous results from device 1 should now be removed from the cache.
  fake_tether_availability_operation_factory_->created_orchestrators()[2]
      ->SendScannedDeviceListUpdate(scanned_device_infos_from_current_scan_,
                                    true /* is_final_scan_result */);
  EXPECT_FALSE(host_scanner_->IsScanActive());

  ClearCurrentScanResults();
  VerifyScanResultsMatchCache();
}

}  // namespace tether

}  // namespace ash
