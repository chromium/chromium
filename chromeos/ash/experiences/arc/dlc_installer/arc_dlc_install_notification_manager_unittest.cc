// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_install_notification_manager.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/experiences/arc/session/arc_service_manager.h"
#include "components/strings/grit/components_strings.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/fake_message_center.h"

namespace arc {

class ArcDlcInstallNotificationManagerTest : public testing::Test {
 protected:
  ArcDlcInstallNotificationManagerTest() = default;
  ~ArcDlcInstallNotificationManagerTest() override = default;

  void SetUp() override {
    auto fake_message_center =
        std::make_unique<message_center::FakeMessageCenter>();
    fake_message_center_ = fake_message_center.get();
    message_center::MessageCenter::InitializeForTesting(
        std::move(fake_message_center));
  }

  void TearDown() override {
    fake_message_center_ = nullptr;
    message_center::MessageCenter::Shutdown();
  }

  raw_ptr<message_center::FakeMessageCenter> fake_message_center_ = nullptr;
};

TEST_F(ArcDlcInstallNotificationManagerTest, DisplayNotification_Success) {
  arc_dlc_install_notification_manager::Show(
      arc_dlc_install_notification_manager::NotificationType::
          kArcVmPreloadSucceeded);

  const auto& notifications = fake_message_center_->GetNotifications();
  ASSERT_FALSE(notifications.empty()) << "No notifications found.";

  const auto& notification = *notifications.begin();

  ASSERT_EQ(1u, notifications.size());
  EXPECT_EQ(notification->title(),
            l10n_util::GetStringUTF16(IDS_ARC_VM_PRELOAD_NOTIFICATION_TITLE));
  EXPECT_EQ(notification->message(),
            l10n_util::GetStringUTF16(IDS_ARC_VM_PRELOAD_SUCCEEDED_MESSAGE));
  EXPECT_EQ(notification->id(),
            arc_dlc_install_notification_manager::kArcVmPreloadSucceededId);
}

TEST_F(ArcDlcInstallNotificationManagerTest, DisplayNotification_Failure) {
  arc_dlc_install_notification_manager::Show(
      arc_dlc_install_notification_manager::NotificationType::
          kArcVmPreloadFailed);

  const auto& notifications = fake_message_center_->GetNotifications();
  ASSERT_FALSE(notifications.empty()) << "No notifications found.";
  const auto& notification = *notifications.begin();

  ASSERT_EQ(1u, notifications.size());
  EXPECT_EQ(notification->title(),
            l10n_util::GetStringUTF16(IDS_ARC_VM_PRELOAD_NOTIFICATION_TITLE));
  EXPECT_EQ(notification->message(),
            l10n_util::GetStringUTF16(IDS_ARC_VM_PRELOAD_FAILED_MESSAGE));
  EXPECT_EQ(notification->id(),
            arc_dlc_install_notification_manager::kArcVmPreloadFailedId);
}

TEST_F(ArcDlcInstallNotificationManagerTest,
       DisplayNotification_PreloadStarted) {
  arc_dlc_install_notification_manager::Show(
      arc_dlc_install_notification_manager::NotificationType::
          kArcVmPreloadStarted);

  const auto& notifications = fake_message_center_->GetNotifications();
  ASSERT_FALSE(notifications.empty()) << "No notifications found.";
  const auto& notification = *notifications.begin();

  ASSERT_EQ(1u, notifications.size());
  EXPECT_EQ(notification->title(),
            l10n_util::GetStringUTF16(IDS_ARC_VM_PRELOAD_NOTIFICATION_TITLE));
  EXPECT_EQ(notification->message(),
            l10n_util::GetStringUTF16(IDS_ARC_VM_PRELOAD_STARTED_MESSAGE));
  EXPECT_EQ(notification->id(),
            arc_dlc_install_notification_manager::kArcVmPreloadStartedId);
}

}  // namespace arc
