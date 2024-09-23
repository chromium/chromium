// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/network_portal_notification_controller.h"

#include "base/command_line.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/network/network_state.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

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
  NetworkPortalNotificationControllerTest() : BrowserWithTestWindowTest() {}

  NetworkPortalNotificationControllerTest(
      const NetworkPortalNotificationControllerTest&) = delete;
  NetworkPortalNotificationControllerTest& operator=(
      const NetworkPortalNotificationControllerTest&) = delete;

  ~NetworkPortalNotificationControllerTest() override {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    display_service_ = std::make_unique<NotificationDisplayServiceTester>(
        nullptr /* profile */);
  }

 protected:
  void PortalStateChanged(const NetworkState* network,
                          NetworkState::PortalState portal_state) {
    controller_.PortalStateChanged(network, portal_state);
  }

  bool HasNotification() {
    return !!display_service_->GetNotification(kNotificationId);
  }

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  NetworkPortalNotificationController controller_;
};

TEST_F(NetworkPortalNotificationControllerTest, NetworkStateChangedPortal) {
  TestWiFiNetworkState wifi("wifi");

  // Notification is not displayed for online state.
  PortalStateChanged(&wifi, NetworkState::PortalState::kOnline);
  EXPECT_FALSE(HasNotification());

  // Notification is displayed for portal state
  PortalStateChanged(&wifi, NetworkState::PortalState::kPortal);
  EXPECT_TRUE(HasNotification());

  // Notification is closed for online state.
  PortalStateChanged(&wifi, NetworkState::PortalState::kOnline);
  EXPECT_FALSE(HasNotification());
}

TEST_F(NetworkPortalNotificationControllerTest,
       NetworkStateChangedPortalSuspected) {
  TestWiFiNetworkState wifi("wifi");

  // Notification is not displayed for online state.
  PortalStateChanged(&wifi, NetworkState::PortalState::kOnline);
  EXPECT_FALSE(HasNotification());

  // Notification is displayed for portal-suspected state
  PortalStateChanged(&wifi, NetworkState::PortalState::kPortalSuspected);
  EXPECT_TRUE(HasNotification());

  // Notification is closed for online state.
  PortalStateChanged(&wifi, NetworkState::PortalState::kOnline);
  EXPECT_FALSE(HasNotification());
}

TEST_F(NetworkPortalNotificationControllerTest,
       NetworkStateChangedProxyAuthRequired) {
  TestWiFiNetworkState wifi("wifi");

  // Notification is not displayed for online state.
  PortalStateChanged(&wifi, NetworkState::PortalState::kOnline);
  EXPECT_FALSE(HasNotification());

  // Notification is closed for online state.
  PortalStateChanged(&wifi, NetworkState::PortalState::kOnline);
  EXPECT_FALSE(HasNotification());
}

TEST_F(NetworkPortalNotificationControllerTest, NetworkChanged) {
  TestWiFiNetworkState wifi1("wifi1");
  PortalStateChanged(&wifi1, NetworkState::PortalState::kPortal);
  EXPECT_TRUE(HasNotification());

  display_service_->RemoveNotification(NotificationHandler::Type::TRANSIENT,
                                       kNotificationId, true /* by_user */);
  EXPECT_FALSE(HasNotification());

  // User already closed notification about portal state for this network,
  // so notification shouldn't be displayed second time.
  PortalStateChanged(&wifi1, NetworkState::PortalState::kPortal);
  EXPECT_FALSE(HasNotification());

  TestWiFiNetworkState wifi2("wifi2");
  // Second network is in online state, so there shouldn't be any
  // notifications.
  PortalStateChanged(&wifi2, NetworkState::PortalState::kOnline);
  EXPECT_FALSE(HasNotification());

  // User switches back to the first network, so notification should
  // be displayed.
  PortalStateChanged(&wifi1, NetworkState::PortalState::kPortal);
  EXPECT_TRUE(HasNotification());
}

TEST_F(NetworkPortalNotificationControllerTest, NotificationUpdated) {
  // First network is behind a captive portal, so notification should
  // be displayed.
  TestWiFiNetworkState wifi1("wifi1");
  wifi1.PropertyChanged("Name", base::Value("wifi1"));
  PortalStateChanged(&wifi1, NetworkState::PortalState::kPortal);
  EXPECT_TRUE(HasNotification());
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
  PortalStateChanged(&wifi2, NetworkState::PortalState::kPortal);
  EXPECT_TRUE(HasNotification());
  EXPECT_EQ(1u, display_service_
                    ->GetDisplayedNotificationsForType(
                        NotificationHandler::Type::TRANSIENT)
                    .size());
  EXPECT_NE(initial_message,
            display_service_->GetNotification(kNotificationId)->message());

  // User closes the notification.
  display_service_->RemoveNotification(NotificationHandler::Type::TRANSIENT,
                                       kNotificationId, true /* by_user */);
  EXPECT_FALSE(HasNotification());

  // Portal detector notified that second network is still behind captive
  // portal, but user already closed the notification, so there should
  // not be any notifications.
  PortalStateChanged(&wifi2, NetworkState::PortalState::kPortal);
  EXPECT_FALSE(HasNotification());

  // Network was switched (by shill or by user) to wifi1. Notification
  // should be displayed.
  PortalStateChanged(&wifi1, NetworkState::PortalState::kPortal);
  EXPECT_TRUE(HasNotification());
}

}  // namespace ash
