// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/revoked_permissions_os_notification_display_manager.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/safety_hub/abusive_notification_permissions_manager.h"
#include "chrome/browser/ui/safety_hub/disruptive_notification_permissions_manager.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kUrl1[] = "https://example1.com";
const char kUrl2[] = "https://example2.com";
const char kUrl3[] = "https://example3.com";

class MockSafetyHubNotificationWrapper
    : public RevokedPermissionsOSNotificationDisplayManager::
          SafetyHubNotificationWrapper {
 public:
  MockSafetyHubNotificationWrapper() = default;
  ~MockSafetyHubNotificationWrapper() override = default;

  MOCK_METHOD(void,
              DisplayNotification,
              (int num_revoked_permissions),
              (override));
  MOCK_METHOD(void,
              UpdateNotification,
              (int num_revoked_permissions),
              (override));
};

}  // namespace

class RevokedPermissionsOSNotificationDisplayManagerTest
    : public ::testing::Test {
 public:
  RevokedPermissionsOSNotificationDisplayManagerTest() {
    feature_list_.InitAndEnableFeature(
        safe_browsing::kAutoRevokeSuspiciousNotification);
  }

  void SetUp() override {
    auto mock_wrapper = std::make_unique<MockSafetyHubNotificationWrapper>();
    mock_wrapper_ = mock_wrapper.get();
    manager_ = std::make_unique<RevokedPermissionsOSNotificationDisplayManager>(
        hcsm(), std::move(mock_wrapper));
  }

  HostContentSettingsMap* hcsm() {
    return HostContentSettingsMapFactory::GetForProfile(profile());
  }

  void AddAbusiveRevocation(
      const GURL& url,
      safe_browsing::NotificationRevocationSource source) {
    AbusiveNotificationPermissionsManager::
        SetRevokedAbusiveNotificationPermission(hcsm(), url,
                                                /*is_ignored=*/false, source);
  }

  void AddDisruptiveRevocation(const GURL& url) {
    DisruptiveNotificationPermissionsManager::RevocationEntry entry(
        DisruptiveNotificationPermissionsManager::RevocationState::kRevoked,
        /*site_engagement=*/0.0,
        /*daily_notification_count=*/4,
        /*timestamp=*/base::Time::Now());
    DisruptiveNotificationPermissionsManager::ContentSettingHelper(*hcsm())
        .PersistRevocationEntry(url, entry);
  }

  TestingProfile* profile() { return &profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<RevokedPermissionsOSNotificationDisplayManager> manager_;
  raw_ptr<MockSafetyHubNotificationWrapper> mock_wrapper_;
  TestingProfile profile_;
};

TEST_F(RevokedPermissionsOSNotificationDisplayManagerTest,
       OnlySuspiciousRevocationsCounted) {
  AddAbusiveRevocation(GURL(kUrl1),
                       safe_browsing::NotificationRevocationSource::
                           kSuspiciousContentAutoRevocation);
  AddAbusiveRevocation(
      GURL(kUrl2),
      safe_browsing::NotificationRevocationSource::kSocialEngineeringBlocklist);

  EXPECT_CALL(*mock_wrapper_, DisplayNotification(1));
  manager_->DisplayNotification();
}

TEST_F(RevokedPermissionsOSNotificationDisplayManagerTest,
       DisruptiveRevocationsCounted) {
  AddDisruptiveRevocation(GURL(kUrl1));
  AddDisruptiveRevocation(GURL(kUrl2));

  EXPECT_CALL(*mock_wrapper_, DisplayNotification(2));
  manager_->DisplayNotification();
}

TEST_F(RevokedPermissionsOSNotificationDisplayManagerTest,
       CombinedRevocationsCounted) {
  AddAbusiveRevocation(GURL(kUrl1),
                       safe_browsing::NotificationRevocationSource::
                           kSuspiciousContentAutoRevocation);
  AddDisruptiveRevocation(GURL(kUrl2));
  AddDisruptiveRevocation(GURL(kUrl3));

  EXPECT_CALL(*mock_wrapper_, DisplayNotification(3));
  manager_->DisplayNotification();
}

TEST_F(RevokedPermissionsOSNotificationDisplayManagerTest,
       CombinedRevocationsWithOverlap) {
  AddAbusiveRevocation(GURL(kUrl1),
                       safe_browsing::NotificationRevocationSource::
                           kSuspiciousContentAutoRevocation);
  AddDisruptiveRevocation(GURL(kUrl1));
  AddDisruptiveRevocation(GURL(kUrl2));

  EXPECT_CALL(*mock_wrapper_, DisplayNotification(2));
  manager_->DisplayNotification();
}

TEST_F(RevokedPermissionsOSNotificationDisplayManagerTest, UpdateNotification) {
  AddAbusiveRevocation(GURL(kUrl1),
                       safe_browsing::NotificationRevocationSource::
                           kSuspiciousContentAutoRevocation);
  AddDisruptiveRevocation(GURL(kUrl2));

  EXPECT_CALL(*mock_wrapper_, DisplayNotification(2));
  manager_->DisplayNotification();

  testing::Mock::VerifyAndClearExpectations(mock_wrapper_);

  AddDisruptiveRevocation(GURL(kUrl3));

  EXPECT_CALL(*mock_wrapper_, UpdateNotification(3));
  manager_->UpdateNotification();
}

TEST_F(RevokedPermissionsOSNotificationDisplayManagerTest, FeatureDisabled) {
  feature_list_.Reset();
  AddAbusiveRevocation(GURL(kUrl1),
                       safe_browsing::NotificationRevocationSource::
                           kSuspiciousContentAutoRevocation);
  AddDisruptiveRevocation(GURL(kUrl2));

  // Only disruptive should be counted.
  EXPECT_CALL(*mock_wrapper_, DisplayNotification(1));
  manager_->DisplayNotification();
}
