// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/frozen_update/frozen_update_notification.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {

namespace {

// Vendor and device PCIIDs which will see a notification.
constexpr int kVendor = 0x8086;
constexpr int kDevice = 0x2a42;

}  // namespace

class FrozenUpdateNotificationTestBase : public AshTestBase {
 public:
  FrozenUpdateNotificationTestBase() = default;
  ~FrozenUpdateNotificationTestBase() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // Ensure the notification has not been dismissed yet.
    prefs_.registry()->RegisterBooleanPref(
        prefs::kFrozenUpdateNotificationDismissed, false);

    // Ensure this is not a child account.
    prefs_.registry()->RegisterStringPref(::prefs::kSupervisedUserId,
                                          std::string());

    // Ensure this is not an enterprise managed device.
    scoped_stub_install_attributes_.Get()->SetConsumerOwned();

    notification_ = std::make_unique<FrozenUpdateNotification>(prefs_);
  }

  void TearDown() override {
    notification_.reset();
    AshTestBase::TearDown();
  }

  void DismissNotification() {
    notification_->Click(FrozenUpdateNotification::kDismissButtonIndex,
                         std::nullopt);
  }

  void MoreInfoNotification() {
    notification_->Click(FrozenUpdateNotification::kMoreInfoButtonIndex,
                         std::nullopt);
  }

  message_center::Notification* GetNotification() {
    return message_center::MessageCenter::Get()->FindVisibleNotificationById(
        FrozenUpdateNotification::kFrozenUpdateNotificationId);
  }

 protected:
  TestingPrefServiceSimple prefs_;
  ash::ScopedStubInstallAttributes scoped_stub_install_attributes_;
  std::unique_ptr<FrozenUpdateNotification> notification_;
};

class FrozenUpdateNotificationTest : public FrozenUpdateNotificationTestBase {
 public:
  FrozenUpdateNotificationTest() {
    // Ensure this is treated as a reven device.
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kRevenBranding);

    // Force the feature on.
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kShowFrozenUpdateNotification);
  }

  ~FrozenUpdateNotificationTest() override = default;

 protected:
  base::test::ScopedCommandLine scoped_command_line_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class FrozenUpdateNotificationNotRevenTest
    : public FrozenUpdateNotificationTestBase {
 public:
  FrozenUpdateNotificationNotRevenTest() {
    // Force the feature on.
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kShowFrozenUpdateNotification);
  }

  ~FrozenUpdateNotificationNotRevenTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class FrozenUpdateNotificationFeatureDisabledTest
    : public FrozenUpdateNotificationTestBase {
 public:
  FrozenUpdateNotificationFeatureDisabledTest() {
    // Ensure this is treated as a reven device.
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kRevenBranding);
  }

  ~FrozenUpdateNotificationFeatureDisabledTest() override = default;

 protected:
  base::test::ScopedCommandLine scoped_command_line_;
};

TEST_F(FrozenUpdateNotificationTest, TestNoMatchNoNotification) {
  // Override the GPU with a nonexistent GPU so it won't match.
  notification_->OverrideGpuForTesting(0x1234, 0x5678);
  notification_->MaybeShowNotification();
  auto* notification = GetNotification();
  ASSERT_FALSE(notification);
}

TEST_F(FrozenUpdateNotificationTest, TestMatchShowNotification) {
  // Override the GPU with one which will have updates blocked.
  notification_->OverrideGpuForTesting(kVendor, kDevice);
  notification_->MaybeShowNotification();
  auto* notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(notification->title(), u"Final update");
  EXPECT_EQ(notification->message(),
            u"Your ChromeOS Flex device will stop receiving updates "
            u"soon. Consider upgrading to a ChromeOS device.");
}

TEST_F(FrozenUpdateNotificationTest, TestMoreInfoNotification) {
  ASSERT_TRUE(
      FrozenUpdateNotification::ShouldShowFrozenUpdateNotification(prefs_));
  notification_->OverrideGpuForTesting(kVendor, kDevice);
  notification_->MaybeShowNotification();
  auto* notification = GetNotification();
  ASSERT_TRUE(notification);
  EXPECT_EQ(notification->title(), u"Final update");
  EXPECT_EQ(notification->message(),
            u"Your ChromeOS Flex device will stop receiving updates "
            u"soon. Consider upgrading to a ChromeOS device.");

  // Now click the more info button on the notification.
  MoreInfoNotification();

  // And we won't see another one shown after the first is interacted with.
  ASSERT_FALSE(
      FrozenUpdateNotification::ShouldShowFrozenUpdateNotification(prefs_));
}

TEST_F(FrozenUpdateNotificationTest, TestDismissNotification) {
  ASSERT_TRUE(
      FrozenUpdateNotification::ShouldShowFrozenUpdateNotification(prefs_));
  notification_->OverrideGpuForTesting(kVendor, kDevice);
  notification_->MaybeShowNotification();
  auto* notification = GetNotification();
  ASSERT_TRUE(notification);
  std::u16string title = u"Final update";
  EXPECT_EQ(notification->title(), title);
  EXPECT_EQ(notification->message(),
            u"Your ChromeOS Flex device will stop receiving updates "
            u"soon. Consider upgrading to a ChromeOS device.");

  // Now dismiss the notification.
  DismissNotification();

  // And we won't see another one shown after the first is dismissed.
  ASSERT_FALSE(
      FrozenUpdateNotification::ShouldShowFrozenUpdateNotification(prefs_));
}

TEST_F(FrozenUpdateNotificationTest, DoesNotShowForEnterpriseManaged) {
  // Override the stub to simulate an enterprise-managed device
  scoped_stub_install_attributes_.Get()->SetCloudManaged("example.com",
                                                         "fake-id");

  // Now it should return false
  EXPECT_FALSE(
      FrozenUpdateNotification::ShouldShowFrozenUpdateNotification(prefs_));
}

TEST_F(FrozenUpdateNotificationTest, DoesNotShowForChildAccount) {
  prefs_.SetString(::prefs::kSupervisedUserId,
                   supervised_user::kChildAccountSUID);
  ASSERT_FALSE(
      FrozenUpdateNotification::ShouldShowFrozenUpdateNotification(prefs_));
}

TEST_F(FrozenUpdateNotificationTest, DoesNotShowForAlreadyDismissed) {
  prefs_.SetBoolean(prefs::kFrozenUpdateNotificationDismissed, true);
  ASSERT_FALSE(
      FrozenUpdateNotification::ShouldShowFrozenUpdateNotification(prefs_));
}

TEST_F(FrozenUpdateNotificationNotRevenTest, DoesNotShowNotification) {
  EXPECT_FALSE(
      FrozenUpdateNotification::ShouldShowFrozenUpdateNotification(prefs_));
}

TEST_F(FrozenUpdateNotificationFeatureDisabledTest, DoesNotShowNotification) {
  ASSERT_FALSE(
      FrozenUpdateNotification::ShouldShowFrozenUpdateNotification(prefs_));
}

}  // namespace ash
