// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/notification_remover.h"

#include <memory>

#include "base/test/task_environment.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/tether/fake_active_host.h"
#include "chromeos/ash/components/tether/fake_host_scan_cache.h"
#include "chromeos/ash/components/tether/fake_notification_presenter.h"
#include "chromeos/ash/components/tether/host_scan_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::tether {

namespace {
const int kTestSignalStrength = 100;
}  // namespace

class NotificationRemoverTest : public testing::Test {
 public:
  NotificationRemoverTest(const NotificationRemoverTest&) = delete;
  NotificationRemoverTest& operator=(const NotificationRemoverTest&) = delete;

 protected:
  NotificationRemoverTest()
      : test_entries_(host_scan_test_util::CreateTestEntries()) {}

  void SetUp() override {
    notification_presenter_ = std::make_unique<FakeNotificationPresenter>();
    host_scan_cache_ = std::make_unique<FakeHostScanCache>();
    active_host_ = std::make_unique<FakeActiveHost>();

    notification_remover_ = std::make_unique<NotificationRemover>(
        helper_.network_state_handler(), notification_presenter_.get(),
        host_scan_cache_.get(), active_host_.get());
  }

  void TearDown() override { notification_remover_.reset(); }

  void NotifyPotentialHotspotNearby() {
    notification_presenter_->NotifyPotentialHotspotNearby(
        "device id", "device name", 100 /* signal_strength */);
  }

  void SetAndRemoveHostScanResult() {
    host_scan_cache_->SetHostScanResult(
        test_entries_.at(host_scan_test_util::kTetherGuid0));
    EXPECT_FALSE(host_scan_cache_->empty());

    host_scan_cache_->RemoveHostScanResult(host_scan_test_util::kTetherGuid0);
    EXPECT_TRUE(host_scan_cache_->empty());
  }

  void StartConnectingToWifiNetwork() {
    std::stringstream ss;
    ss << "{"
       << "  \"GUID\": \"wifiNetworkGuid\","
       << "  \"Type\": \"" << shill::kTypeWifi << "\","
       << "  \"State\": \"" << shill::kStateConfiguration << "\""
       << "}";

    helper_.ConfigureService(ss.str());
  }

  base::test::TaskEnvironment task_environment_;
  NetworkStateTestHelper helper_{/*use_default_devices_and_services=*/true};

  const std::unordered_map<std::string, HostScanCacheEntry> test_entries_;

  std::unique_ptr<FakeNotificationPresenter> notification_presenter_;
  std::unique_ptr<FakeHostScanCache> host_scan_cache_;
  std::unique_ptr<FakeActiveHost> active_host_;
  std::unique_ptr<NotificationRemover> notification_remover_;
};

TEST_F(NotificationRemoverTest, TestCacheBecameEmpty) {
  NotifyPotentialHotspotNearby();
  SetAndRemoveHostScanResult();
  EXPECT_EQ(NotificationPresenter::PotentialHotspotNotificationState::
                NO_HOTSPOT_NOTIFICATION_SHOWN,
            notification_presenter_->GetPotentialHotspotNotificationState());

  notification_presenter_->NotifyMultiplePotentialHotspotsNearby();
  SetAndRemoveHostScanResult();
  EXPECT_EQ(NotificationPresenter::PotentialHotspotNotificationState::
                NO_HOTSPOT_NOTIFICATION_SHOWN,
            notification_presenter_->GetPotentialHotspotNotificationState());
}

TEST_F(NotificationRemoverTest, TestStartConnectingToWifiNetwork) {
  NotifyPotentialHotspotNearby();

  StartConnectingToWifiNetwork();
  EXPECT_EQ(NotificationPresenter::PotentialHotspotNotificationState::
                NO_HOTSPOT_NOTIFICATION_SHOWN,
            notification_presenter_->GetPotentialHotspotNotificationState());
}

TEST_F(NotificationRemoverTest, TestTetherDisabled) {
  NotifyPotentialHotspotNearby();
  notification_presenter_->NotifySetupRequired("testDevice",
                                               kTestSignalStrength);
  notification_presenter_->NotifyConnectionToHostFailed();

  notification_remover_.reset();
  EXPECT_EQ(NotificationPresenter::PotentialHotspotNotificationState::
                NO_HOTSPOT_NOTIFICATION_SHOWN,
            notification_presenter_->GetPotentialHotspotNotificationState());
  EXPECT_FALSE(notification_presenter_->is_setup_required_notification_shown());
  EXPECT_FALSE(
      notification_presenter_->is_connection_failed_notification_shown());
}

TEST_F(NotificationRemoverTest, TestActiveHostConnecting) {
  NotifyPotentialHotspotNearby();

  active_host_->SetActiveHostDisconnected();
  EXPECT_EQ(NotificationPresenter::PotentialHotspotNotificationState::
                SINGLE_HOTSPOT_NEARBY_SHOWN,
            notification_presenter_->GetPotentialHotspotNotificationState());

  active_host_->SetActiveHostConnecting("testDeviceId",
                                        host_scan_test_util::kTetherGuid0);
  EXPECT_EQ(NotificationPresenter::PotentialHotspotNotificationState::
                NO_HOTSPOT_NOTIFICATION_SHOWN,
            notification_presenter_->GetPotentialHotspotNotificationState());
}

}  // namespace ash::tether
