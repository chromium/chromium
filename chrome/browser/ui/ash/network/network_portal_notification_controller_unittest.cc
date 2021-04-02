// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/network_portal_notification_controller.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/network/network_state.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace {

const char* const kNotificationId =
    NetworkPortalNotificationController::kNotificationId;

class TestWiFiNetworkState : public NetworkState {
 public:
  explicit TestWiFiNetworkState(const std::string& name) : NetworkState(name) {
    SetGuid(name);
    set_type(shill::kTypeWifi);
  }
};

}  // namespace

// A BrowserWithTestWindowTest will set up profiles for us.
class NetworkPortalNotificationControllerTest
    : public BrowserWithTestWindowTest {
 public:
  NetworkPortalNotificationControllerTest() : controller_(nullptr) {}
  ~NetworkPortalNotificationControllerTest() override {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    display_service_ = std::make_unique<NotificationDisplayServiceTester>(
        nullptr /* profile */);
  }

 protected:
  void OnPortalDetectionCompleted(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalStatus status) {
    controller_.OnPortalDetectionCompleted(network, status);
  }

  bool HasNotification() {
    return !!display_service_->GetNotification(kNotificationId);
  }

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  NetworkPortalNotificationController controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkPortalNotificationControllerTest);
};

TEST_F(NetworkPortalNotificationControllerTest, NetworkStateChanged) {
  TestWiFiNetworkState wifi("wifi");

  // Notification is not displayed for online state.
  OnPortalDetectionCompleted(
      &wifi, NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  ASSERT_FALSE(HasNotification());

  // Notification is displayed for portal state
  OnPortalDetectionCompleted(
      &wifi, NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);
  ASSERT_TRUE(HasNotification());

  // Notification is closed for online state.
  OnPortalDetectionCompleted(
      &wifi, NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  ASSERT_FALSE(HasNotification());
}

TEST_F(NetworkPortalNotificationControllerTest, NetworkChanged) {
  TestWiFiNetworkState wifi1("wifi1");
  OnPortalDetectionCompleted(
      &wifi1, NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);
  ASSERT_TRUE(HasNotification());

  display_service_->RemoveNotification(NotificationHandler::Type::TRANSIENT,
                                       kNotificationId, true /* by_user */);
  ASSERT_FALSE(HasNotification());

  // User already closed notification about portal state for this network,
  // so notification shouldn't be displayed second time.
  OnPortalDetectionCompleted(
      &wifi1, NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);
  ASSERT_FALSE(HasNotification());

  TestWiFiNetworkState wifi2("wifi2");
  // Second network is in online state, so there shouldn't be any
  // notifications.
  OnPortalDetectionCompleted(
      &wifi2, NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  ASSERT_FALSE(HasNotification());

  // User switches back to the first network, so notification should
  // be displayed.
  OnPortalDetectionCompleted(
      &wifi1, NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);
  ASSERT_TRUE(HasNotification());
}

TEST_F(NetworkPortalNotificationControllerTest, NotificationUpdated) {
  // First network is behind a captive portal, so notification should
  // be displayed.
  TestWiFiNetworkState wifi1("wifi1");
  wifi1.PropertyChanged("Name", base::Value("wifi1"));
  OnPortalDetectionCompleted(
      &wifi1, NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);
  ASSERT_TRUE(HasNotification());
  EXPECT_EQ(1u, display_service_
                    ->GetDisplayedNotificationsForType(
                        NotificationHandler::Type::TRANSIENT)
                    .size());
  const std::u16string initial_message =
      display_service_->GetNotification(kNotificationId)->message();

  // Second network is also behind a captive portal, so notification
  // should be updated.
  TestWiFiNetworkState wifi2("wifi2");
  wifi2.PropertyChanged("Name", base::Value("wifi2"));
  OnPortalDetectionCompleted(
      &wifi2, NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);
  ASSERT_TRUE(HasNotification());
  EXPECT_EQ(1u, display_service_
                    ->GetDisplayedNotificationsForType(
                        NotificationHandler::Type::TRANSIENT)
                    .size());
  EXPECT_NE(initial_message,
            display_service_->GetNotification(kNotificationId)->message());

  // User closes the notification.
  display_service_->RemoveNotification(NotificationHandler::Type::TRANSIENT,
                                       kNotificationId, true /* by_user */);
  ASSERT_FALSE(HasNotification());

  // Portal detector notified that second network is still behind captive
  // portal, but user already closed the notification, so there should
  // not be any notifications.
  OnPortalDetectionCompleted(
      &wifi2, NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);
  ASSERT_FALSE(HasNotification());

  // Network was switched (by shill or by user) to wifi1. Notification
  // should be displayed.
  OnPortalDetectionCompleted(
      &wifi1, NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);
  ASSERT_TRUE(HasNotification());
}

}  // namespace chromeos
