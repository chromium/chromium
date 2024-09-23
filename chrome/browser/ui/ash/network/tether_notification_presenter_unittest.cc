// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/tether_notification_presenter.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/network/network_connect.h"
#include "chromeos/ash/components/tether/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"

namespace ash::tether {

namespace {

const int kTestNetworkSignalStrength = 50;

const char kTetherSettingsSubpage[] = "networks?type=Tether";

const char kDeviceId[] = "device_id";
const char kDeviceName[] = "device_name";

}  // namespace

class TetherNotificationPresenterTest : public BrowserWithTestWindowTest {
 public:
  class TestNetworkConnect : public NetworkConnect {
   public:
    TestNetworkConnect() = default;
    ~TestNetworkConnect() override = default;

    std::string network_id_to_connect() { return network_id_to_connect_; }

    // NetworkConnect:
    void DisconnectFromNetworkId(const std::string& network_id) override {}
    void ShowMobileSetup(const std::string& network_id) override {}
    void ShowCarrierAccountDetail(const std::string& network_id) override {}
    void ShowCarrierUnlockNotification() override {}
    void ShowPortalSignin(const std::string& network_id,
                          NetworkConnect::Source source) override {}
    void ConfigureNetworkIdAndConnect(const std::string& network_id,
                                      const base::Value::Dict& shill_properties,
                                      bool shared) override {}
    void CreateConfigurationAndConnect(base::Value::Dict shill_properties,
                                       bool shared) override {}
    void CreateConfiguration(base::Value::Dict shill_properties,
                             bool shared) override {}

    void ConnectToNetworkId(const std::string& network_id) override {
      network_id_to_connect_ = network_id;
    }

   private:
    std::string network_id_to_connect_;
  };

  class TestSettingsUiDelegate
      : public TetherNotificationPresenter::SettingsUiDelegate {
   public:
    TestSettingsUiDelegate() = default;
    ~TestSettingsUiDelegate() override = default;

    Profile* last_profile() { return last_profile_; }
    std::string last_settings_subpage() { return last_settings_subpage_; }

    // TetherNotificationPresenter::SettingsUiDelegate:
    void ShowSettingsSubPageForProfile(Profile* profile,
                                       const std::string& sub_page) override {
      last_profile_ = profile;
      last_settings_subpage_ = sub_page;
    }

   private:
    raw_ptr<Profile, DanglingUntriaged> last_profile_ = nullptr;
    std::string last_settings_subpage_;
  };

  TetherNotificationPresenterTest(const TetherNotificationPresenterTest&) =
      delete;
  TetherNotificationPresenterTest& operator=(
      const TetherNotificationPresenterTest&) = delete;

 protected:
  TetherNotificationPresenterTest() {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());

    test_network_connect_ = base::WrapUnique(new TestNetworkConnect());

    notification_presenter_ = std::make_unique<TetherNotificationPresenter>(
        profile(), test_network_connect_.get());

    test_settings_ui_delegate_ = new TestSettingsUiDelegate();
    notification_presenter_->SetSettingsUiDelegateForTesting(
        base::WrapUnique(test_settings_ui_delegate_.get()));
    has_verified_metrics_ = false;
  }

  void TearDown() override {
    if (!has_verified_metrics_) {
      VerifyNotificationInteractionMetrics(
          0u /* num_expected_body_tapped_single_host_nearby */,
          0u /* num_expected_body_tapped_multiple_hosts_nearby */,
          0u /* num_expected_body_tapped_setup_required */,
          0u /* num_expected_body_tapped_connection_failed */,
          0u /* num_expected_button_tapped_single_host_nearby */);
    }

    BrowserWithTestWindowTest::TearDown();
  }

  std::string GetActiveHostNotificationId() {
    return std::string(TetherNotificationPresenter::kActiveHostNotificationId);
  }

  std::string GetPotentialHotspotNotificationId() {
    return std::string(
        TetherNotificationPresenter::kPotentialHotspotNotificationId);
  }

  std::string GetSetupRequiredNotificationId() {
    return std::string(
        TetherNotificationPresenter::kSetupRequiredNotificationId);
  }

  void VerifySettingsOpened(const std::string& expected_subpage) {
    EXPECT_EQ(profile(), test_settings_ui_delegate_->last_profile());
    EXPECT_EQ(expected_subpage,
              test_settings_ui_delegate_->last_settings_subpage());
  }

  void VerifySettingsNotOpened() {
    EXPECT_FALSE(test_settings_ui_delegate_->last_profile());
    EXPECT_TRUE(test_settings_ui_delegate_->last_settings_subpage().empty());
  }

  void VerifyNotificationInteractionMetrics(
      size_t num_expected_body_tapped_single_host_nearby,
      size_t num_expected_body_tapped_multiple_hosts_nearby,
      size_t num_expected_body_tapped_setup_required,
      size_t num_expected_body_tapped_connection_failed,
      size_t num_expected_button_tapped_single_host_nearby) {
    if (num_expected_body_tapped_single_host_nearby +
            num_expected_body_tapped_multiple_hosts_nearby +
            num_expected_body_tapped_setup_required +
            num_expected_body_tapped_connection_failed +
            num_expected_button_tapped_single_host_nearby ==
        0u) {
      histogram_tester_.ExpectTotalCount(
          "InstantTethering.NotificationInteractionType", 0u);
      return;
    }

    histogram_tester_.ExpectBucketCount(
        "InstantTethering.NotificationInteractionType",
        TetherNotificationPresenter::
            NOTIFICATION_BODY_TAPPED_SINGLE_HOST_NEARBY,
        num_expected_body_tapped_single_host_nearby);
    histogram_tester_.ExpectBucketCount(
        "InstantTethering.NotificationInteractionType",
        TetherNotificationPresenter::
            NOTIFICATION_BODY_TAPPED_MULTIPLE_HOSTS_NEARBY,
        num_expected_body_tapped_multiple_hosts_nearby);
    histogram_tester_.ExpectBucketCount(
        "InstantTethering.NotificationInteractionType",
        TetherNotificationPresenter::NOTIFICATION_BODY_TAPPED_SETUP_REQUIRED,
        num_expected_body_tapped_setup_required);
    histogram_tester_.ExpectBucketCount(
        "InstantTethering.NotificationInteractionType",
        TetherNotificationPresenter::NOTIFICATION_BODY_TAPPED_CONNECTION_FAILED,
        num_expected_body_tapped_connection_failed);
    histogram_tester_.ExpectBucketCount(
        "InstantTethering.NotificationInteractionType",
        TetherNotificationPresenter::NOTIFICATION_BUTTON_TAPPED_HOST_NEARBY,
        num_expected_button_tapped_single_host_nearby);

    has_verified_metrics_ = true;
  }

  base::HistogramTester histogram_tester_;
  bool has_verified_metrics_;

  std::unique_ptr<TestNetworkConnect> test_network_connect_;
  raw_ptr<TestSettingsUiDelegate, DanglingUntriaged> test_settings_ui_delegate_;
  std::unique_ptr<TetherNotificationPresenter> notification_presenter_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
};

TEST_F(TetherNotificationPresenterTest,
       TestHostConnectionFailedNotification_RemoveProgrammatically) {
  EXPECT_FALSE(
      display_service_->GetNotification(GetActiveHostNotificationId()));
  notification_presenter_->NotifyConnectionToHostFailed();

  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(GetActiveHostNotificationId());
  ASSERT_TRUE(notification);
  EXPECT_EQ(GetActiveHostNotificationId(), notification->id());

  notification_presenter_->RemoveConnectionToHostFailedNotification();
  EXPECT_FALSE(
      display_service_->GetNotification(GetActiveHostNotificationId()));

  VerifySettingsNotOpened();
}

TEST_F(TetherNotificationPresenterTest,
       TestHostConnectionFailedNotification_TapNotification) {
  EXPECT_FALSE(
      display_service_->GetNotification(GetActiveHostNotificationId()));
  notification_presenter_->NotifyConnectionToHostFailed();

  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(GetActiveHostNotificationId());
  ASSERT_TRUE(notification);
  EXPECT_EQ(GetActiveHostNotificationId(), notification->id());

  // Tap the notification.
  ASSERT_TRUE(notification->delegate());
  notification->delegate()->Click(std::nullopt, std::nullopt);
  VerifySettingsOpened(kTetherSettingsSubpage);
  EXPECT_FALSE(
      display_service_->GetNotification(GetActiveHostNotificationId()));

  VerifyNotificationInteractionMetrics(
      0u /* num_expected_body_tapped_single_host_nearby */,
      0u /* num_expected_body_tapped_multiple_hosts_nearby */,
      0u /* num_expected_body_tapped_setup_required */,
      1u /* num_expected_body_tapped_connection_failed */,
      0u /* num_expected_button_tapped_single_host_nearby */);
}

TEST_F(TetherNotificationPresenterTest,
       TestHostConnectionFailedNotification_NotShownWhenNotificationsDisabled) {
  EXPECT_FALSE(
      display_service_->GetNotification(GetActiveHostNotificationId()));

  profile()->GetPrefs()->SetBoolean(prefs::kNotificationsEnabled,
                                    /*value=*/false);

  notification_presenter_->NotifyConnectionToHostFailed();

  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(GetActiveHostNotificationId());
  ASSERT_FALSE(notification);
}

TEST_F(TetherNotificationPresenterTest,
       TestSetupRequiredNotification_RemoveProgrammatically) {
  EXPECT_FALSE(
      display_service_->GetNotification(GetSetupRequiredNotificationId()));
  notification_presenter_->NotifySetupRequired(kDeviceName,
                                               kTestNetworkSignalStrength);

  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(GetSetupRequiredNotificationId());
  ASSERT_TRUE(notification);
  EXPECT_EQ(GetSetupRequiredNotificationId(), notification->id());

  notification_presenter_->RemoveSetupRequiredNotification();
  EXPECT_FALSE(
      display_service_->GetNotification(GetSetupRequiredNotificationId()));

  VerifySettingsNotOpened();
}

TEST_F(TetherNotificationPresenterTest,
       TestSetupRequiredNotification_TapNotification) {
  EXPECT_FALSE(
      display_service_->GetNotification(GetSetupRequiredNotificationId()));
  notification_presenter_->NotifySetupRequired(kDeviceName,
                                               kTestNetworkSignalStrength);

  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(GetSetupRequiredNotificationId());
  ASSERT_TRUE(notification);
  EXPECT_EQ(GetSetupRequiredNotificationId(), notification->id());

  // Tap the notification.
  ASSERT_TRUE(notification->delegate());
  notification->delegate()->Click(std::nullopt, std::nullopt);
  VerifySettingsOpened(kTetherSettingsSubpage);
  EXPECT_FALSE(
      display_service_->GetNotification(GetSetupRequiredNotificationId()));

  VerifyNotificationInteractionMetrics(
      0u /* num_expected_body_tapped_single_host_nearby */,
      0u /* num_expected_body_tapped_multiple_hosts_nearby */,
      1u /* num_expected_body_tapped_setup_required */,
      0u /* num_expected_body_tapped_connection_failed */,
      0u /* num_expected_button_tapped_single_host_nearby */);
}

TEST_F(TetherNotificationPresenterTest,
       TestInstantHotspotNotification_NeverDismiss) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(ash::features::kInstantHotspotRebrand);
  EXPECT_FALSE(
      display_service_->GetNotification(GetPotentialHotspotNotificationId()));
  notification_presenter_->NotifyPotentialHotspotNearby(
      kDeviceId, kDeviceName, kTestNetworkSignalStrength);

  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(GetPotentialHotspotNotificationId());

  EXPECT_TRUE(notification->never_timeout());
}

TEST_F(TetherNotificationPresenterTest,
       TestInstantHotspotNotification_NeverDismissNoFF) {
  EXPECT_FALSE(
      display_service_->GetNotification(GetPotentialHotspotNotificationId()));
  notification_presenter_->NotifyPotentialHotspotNearby(
      kDeviceId, kDeviceName, kTestNetworkSignalStrength);

  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(GetPotentialHotspotNotificationId());

  EXPECT_FALSE(notification->never_timeout());
}

TEST_F(TetherNotificationPresenterTest,
       TestPotentialHotspotNotification_RemoveProgrammatically) {
  EXPECT_FALSE(
      display_service_->GetNotification(GetPotentialHotspotNotificationId()));
  notification_presenter_->NotifyPotentialHotspotNearby(
      kDeviceId, kDeviceName, kTestNetworkSignalStrength);

  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(GetPotentialHotspotNotificationId());
  ASSERT_TRUE(notification);
  EXPECT_EQ(GetPotentialHotspotNotificationId(), notification->id());

  notification_presenter_->RemovePotentialHotspotNotification();
  EXPECT_FALSE(
      display_service_->GetNotification(GetPotentialHotspotNotificationId()));

  VerifySettingsNotOpened();
}

TEST_F(TetherNotificationPresenterTest,
       TestPotentialHotspotNotification_NotificationsDisabled) {
  EXPECT_FALSE(
      display_service_->GetNotification(GetPotentialHotspotNotificationId()));

  profile()->GetPrefs()->SetBoolean(prefs::kNotificationsEnabled,
                                    /*value=*/false);

  notification_presenter_->NotifyPotentialHotspotNearby(
      kDeviceId, kDeviceName, kTestNetworkSignalStrength);

  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(GetPotentialHotspotNotificationId());
  ASSERT_FALSE(notification);
}

TEST_F(TetherNotificationPresenterTest,
       TestPotentialHotspotNotification_TapNotification) {
  EXPECT_FALSE(
      display_service_->GetNotification(GetPotentialHotspotNotificationId()));
  notification_presenter_->NotifyPotentialHotspotNearby(
      kDeviceId, kDeviceName, kTestNetworkSignalStrength);

  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(GetPotentialHotspotNotificationId());
  ASSERT_TRUE(notification);
  EXPECT_EQ(GetPotentialHotspotNotificationId(), notification->id());

  // Tap the notification.
  ASSERT_TRUE(notification->delegate());
  notification->delegate()->Click(std::nullopt, std::nullopt);
  VerifySettingsOpened(kTetherSettingsSubpage);
  EXPECT_FALSE(
      display_service_->GetNotification(GetPotentialHotspotNotificationId()));
  VerifyNotificationInteractionMetrics(
      1u /* num_expected_body_tapped_single_host_nearby */,
      0u /* num_expected_body_tapped_multiple_hosts_nearby */,
      0u /* num_expected_body_tapped_setup_required */,
      0u /* num_expected_body_tapped_connection_failed */,
      0u /* num_expected_button_tapped_single_host_nearby */);
}

TEST_F(TetherNotificationPresenterTest,
       TestSinglePotentialHotspotNotification_TapNotificationButton) {
  EXPECT_FALSE(
      display_service_->GetNotification(GetPotentialHotspotNotificationId()));
  notification_presenter_->NotifyPotentialHotspotNearby(
      kDeviceId, kDeviceName, kTestNetworkSignalStrength);

  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(GetPotentialHotspotNotificationId());
  ASSERT_TRUE(notification);
  EXPECT_EQ(GetPotentialHotspotNotificationId(), notification->id());

  // Tap the notification's button.
  ASSERT_TRUE(notification->delegate());
  notification->delegate()->Click(0, std::nullopt);
  EXPECT_FALSE(
      display_service_->GetNotification(GetPotentialHotspotNotificationId()));

  EXPECT_EQ(kDeviceId, test_network_connect_->network_id_to_connect());

  VerifyNotificationInteractionMetrics(
      0u /* num_expected_body_tapped_single_host_nearby */,
      0u /* num_expected_body_tapped_multiple_hosts_nearby */,
      0u /* num_expected_body_tapped_setup_required */,
      0u /* num_expected_body_tapped_connection_failed */,
      1u /* num_expected_button_tapped_single_host_nearby */);
}

TEST_F(TetherNotificationPresenterTest,
       TestMultiplePotentialHotspotNotification_RemoveProgrammatically) {
  EXPECT_FALSE(
      display_service_->GetNotification(GetPotentialHotspotNotificationId()));
  notification_presenter_->NotifyMultiplePotentialHotspotsNearby();

  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(GetPotentialHotspotNotificationId());
  ASSERT_TRUE(notification);
  EXPECT_EQ(GetPotentialHotspotNotificationId(), notification->id());

  notification_presenter_->RemovePotentialHotspotNotification();
  EXPECT_FALSE(
      display_service_->GetNotification(GetPotentialHotspotNotificationId()));

  VerifySettingsNotOpened();
}

TEST_F(TetherNotificationPresenterTest,
       TestMultiplePotentialHotspotNotification_TapNotification) {
  EXPECT_FALSE(
      display_service_->GetNotification(GetPotentialHotspotNotificationId()));
  notification_presenter_->NotifyMultiplePotentialHotspotsNearby();

  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(GetPotentialHotspotNotificationId());
  ASSERT_TRUE(notification);
  EXPECT_EQ(GetPotentialHotspotNotificationId(), notification->id());

  // Tap the notification.
  ASSERT_TRUE(notification->delegate());
  notification->delegate()->Click(std::nullopt, std::nullopt);
  VerifySettingsOpened(kTetherSettingsSubpage);
  EXPECT_FALSE(
      display_service_->GetNotification(GetPotentialHotspotNotificationId()));

  VerifyNotificationInteractionMetrics(
      0u /* num_expected_body_tapped_single_host_nearby */,
      1u /* num_expected_body_tapped_multiple_hosts_nearby */,
      0u /* num_expected_body_tapped_setup_required */,
      0u /* num_expected_body_tapped_connection_failed */,
      0u /* num_expected_button_tapped_single_host_nearby */);
}

TEST_F(TetherNotificationPresenterTest,
       TestPotentialHotspotNotifications_UpdatesOneNotification) {
  EXPECT_FALSE(
      display_service_->GetNotification(GetPotentialHotspotNotificationId()));
  notification_presenter_->NotifyPotentialHotspotNearby(
      kDeviceId, kDeviceName, kTestNetworkSignalStrength);

  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(GetPotentialHotspotNotificationId());
  ASSERT_TRUE(notification);
  EXPECT_EQ(GetPotentialHotspotNotificationId(), notification->id());

  // Simulate more device results coming in. Display the potential hotspots
  // notification for multiple devices.
  notification_presenter_->NotifyMultiplePotentialHotspotsNearby();

  // The existing notification should have been updated instead of creating a
  // new one.
  notification =
      display_service_->GetNotification(GetPotentialHotspotNotificationId());
  ASSERT_TRUE(notification);
  EXPECT_EQ(GetPotentialHotspotNotificationId(), notification->id());

  notification_presenter_->RemovePotentialHotspotNotification();
  EXPECT_FALSE(
      display_service_->GetNotification(GetPotentialHotspotNotificationId()));

  VerifySettingsNotOpened();
}

TEST_F(TetherNotificationPresenterTest,
       TestGetPotentialHotspotNotificationState) {
  EXPECT_EQ(notification_presenter_->GetPotentialHotspotNotificationState(),
            NotificationPresenter::PotentialHotspotNotificationState::
                NO_HOTSPOT_NOTIFICATION_SHOWN);

  // Notify single host available and remove.
  notification_presenter_->NotifyPotentialHotspotNearby(
      kDeviceId, kDeviceName, kTestNetworkSignalStrength);
  EXPECT_EQ(notification_presenter_->GetPotentialHotspotNotificationState(),
            NotificationPresenter::PotentialHotspotNotificationState::
                SINGLE_HOTSPOT_NEARBY_SHOWN);
  notification_presenter_->RemovePotentialHotspotNotification();
  EXPECT_EQ(notification_presenter_->GetPotentialHotspotNotificationState(),
            NotificationPresenter::PotentialHotspotNotificationState::
                NO_HOTSPOT_NOTIFICATION_SHOWN);

  // Notify single host available and remove by tapping notification.
  notification_presenter_->NotifyPotentialHotspotNearby(
      kDeviceId, kDeviceName, kTestNetworkSignalStrength);
  EXPECT_EQ(notification_presenter_->GetPotentialHotspotNotificationState(),
            NotificationPresenter::PotentialHotspotNotificationState::
                SINGLE_HOTSPOT_NEARBY_SHOWN);
  display_service_->GetNotification(GetPotentialHotspotNotificationId())
      ->delegate()
      ->Click(std::nullopt, std::nullopt);
  EXPECT_EQ(notification_presenter_->GetPotentialHotspotNotificationState(),
            NotificationPresenter::PotentialHotspotNotificationState::
                NO_HOTSPOT_NOTIFICATION_SHOWN);

  // Notify single host available and remove by tapping notification button.
  notification_presenter_->NotifyPotentialHotspotNearby(
      kDeviceId, kDeviceName, kTestNetworkSignalStrength);
  EXPECT_EQ(notification_presenter_->GetPotentialHotspotNotificationState(),
            NotificationPresenter::PotentialHotspotNotificationState::
                SINGLE_HOTSPOT_NEARBY_SHOWN);
  display_service_->GetNotification(GetPotentialHotspotNotificationId())
      ->delegate()
      ->Click(0, std::nullopt);
  EXPECT_EQ(notification_presenter_->GetPotentialHotspotNotificationState(),
            NotificationPresenter::PotentialHotspotNotificationState::
                NO_HOTSPOT_NOTIFICATION_SHOWN);

  // Notify single, then multiple hosts available and remove.
  notification_presenter_->NotifyPotentialHotspotNearby(
      kDeviceId, kDeviceName, kTestNetworkSignalStrength);
  EXPECT_EQ(notification_presenter_->GetPotentialHotspotNotificationState(),
            NotificationPresenter::PotentialHotspotNotificationState::
                SINGLE_HOTSPOT_NEARBY_SHOWN);
  notification_presenter_->NotifyMultiplePotentialHotspotsNearby();
  EXPECT_EQ(notification_presenter_->GetPotentialHotspotNotificationState(),
            NotificationPresenter::PotentialHotspotNotificationState::
                MULTIPLE_HOTSPOTS_NEARBY_SHOWN);
  notification_presenter_->RemovePotentialHotspotNotification();
  EXPECT_EQ(notification_presenter_->GetPotentialHotspotNotificationState(),
            NotificationPresenter::PotentialHotspotNotificationState::
                NO_HOTSPOT_NOTIFICATION_SHOWN);

  // Notify single, then multiple hosts available and remove by tapping
  // notification.
  notification_presenter_->NotifyPotentialHotspotNearby(
      kDeviceId, kDeviceName, kTestNetworkSignalStrength);
  EXPECT_EQ(notification_presenter_->GetPotentialHotspotNotificationState(),
            NotificationPresenter::PotentialHotspotNotificationState::
                SINGLE_HOTSPOT_NEARBY_SHOWN);
  notification_presenter_->NotifyMultiplePotentialHotspotsNearby();
  EXPECT_EQ(notification_presenter_->GetPotentialHotspotNotificationState(),
            NotificationPresenter::PotentialHotspotNotificationState::
                MULTIPLE_HOTSPOTS_NEARBY_SHOWN);
  display_service_->GetNotification(GetPotentialHotspotNotificationId())
      ->delegate()
      ->Click(std::nullopt, std::nullopt);
  EXPECT_EQ(notification_presenter_->GetPotentialHotspotNotificationState(),
            NotificationPresenter::PotentialHotspotNotificationState::
                NO_HOTSPOT_NOTIFICATION_SHOWN);

  VerifyNotificationInteractionMetrics(
      1u /* num_expected_body_tapped_single_host_nearby */,
      1u /* num_expected_body_tapped_multiple_hosts_nearby */,
      0u /* num_expected_body_tapped_setup_required */,
      0u /* num_expected_body_tapped_connection_failed */,
      1u /* num_expected_button_tapped_single_host_nearby */);
}

}  // namespace ash::tether
