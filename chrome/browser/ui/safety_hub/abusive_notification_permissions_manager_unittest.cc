// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/abusive_notification_permissions_manager.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/constants.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_local_database_manager.h"
#include "content/public/browser/browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const ContentSettingsType notifications_type =
    ContentSettingsType::NOTIFICATIONS;
const ContentSettingsType revoked_notifications_type =
    ContentSettingsType::REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS;

class MockSafeBrowsingDatabaseManager
    : public safe_browsing::TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager()
      : safe_browsing::TestSafeBrowsingDatabaseManager(
            base::SequencedTaskRunner::GetCurrentDefault(),
            base::SequencedTaskRunner::GetCurrentDefault()) {}
  MockSafeBrowsingDatabaseManager(const MockSafeBrowsingDatabaseManager&) =
      delete;
  MockSafeBrowsingDatabaseManager& operator=(
      const MockSafeBrowsingDatabaseManager&) = delete;

  bool CheckBrowseUrl(const GURL& gurl,
                      const safe_browsing::SBThreatTypeSet& threat_types,
                      Client* client,
                      safe_browsing::CheckBrowseUrlType check_type) override {
    CHECK(client);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&MockSafeBrowsingDatabaseManager::OnCheckBrowseURLDone,
                       this, gurl, client->GetWeakPtr()));
    return false;
  }

  void CancelCheck(Client* client) override { called_cancel_check_ = true; }

  bool HasCalledCancelCheck() { return called_cancel_check_; }

  void SetThreatTypeForUrl(GURL gurl, safe_browsing::SBThreatType threat_type) {
    urls_threat_type_[gurl.spec()] = threat_type;
  }

 protected:
  ~MockSafeBrowsingDatabaseManager() override = default;

 private:
  void OnCheckBrowseURLDone(const GURL& gurl, base::WeakPtr<Client> client) {
    if (called_cancel_check_) {
      return;
    }
    CHECK(client);
    client->OnCheckBrowseUrlResult(gurl, urls_threat_type_[gurl.spec()],
                                   safe_browsing::ThreatMetadata());
  }

  base::flat_map<std::string, safe_browsing::SBThreatType> urls_threat_type_;
  bool called_cancel_check_ = false;
};

}  // namespace

class AbusiveNotificationPermissionsManagerTest : public ::testing::Test {
 public:
  void SetUp() override {
    mock_database_manager_ = new MockSafeBrowsingDatabaseManager();
  }

  void TearDown() override { mock_database_manager_.reset(); }

  HostContentSettingsMap* hcsm() {
    return HostContentSettingsMapFactory::GetForProfile(&profile_);
  }

  MockSafeBrowsingDatabaseManager* mock_database_manager() {
    return mock_database_manager_.get();
  }

  void AddAbusiveNotification(std::string url,
                              ContentSetting cs,
                              bool is_ignored) {
    content_settings::ContentSettingConstraints constraint;
    hcsm()->SetContentSettingDefaultScope(GURL(url), GURL(url),
                                          notifications_type, cs, constraint);
    hcsm()->SetWebsiteSettingDefaultScope(
        GURL(url), GURL(url), revoked_notifications_type,
        base::Value(base::Value::Dict().Set(
            safety_hub::kRevokedStatusDictKeyStr,
            is_ignored ? safety_hub::kIgnoreStr : safety_hub::kRevokeStr)),
        constraint);
    mock_database_manager()->SetThreatTypeForUrl(
        GURL(url), safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
  }

  void AddSafeNotification(std::string url, ContentSetting cs) {
    content_settings::ContentSettingConstraints constraint;
    hcsm()->SetContentSettingDefaultScope(GURL(url), GURL(url),
                                          notifications_type, cs, constraint);
    mock_database_manager()->SetThreatTypeForUrl(
        GURL(url), safe_browsing::SBThreatType::SB_THREAT_TYPE_SAFE);
  }

  void VerifyTimeoutCallbackNotCalled() {
    // Verify timeout is not called even after fast forwarding.
    task_environment_.FastForwardBy(base::Milliseconds(kCheckUrlTimeoutMs));
    EXPECT_FALSE(mock_database_manager_->HasCalledCancelCheck());
  }

 private:
  scoped_refptr<MockSafeBrowsingDatabaseManager> mock_database_manager_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
};

TEST_F(AbusiveNotificationPermissionsManagerTest,
       AddAllowedAbusiveNotificationSitesToRevokedOriginSet) {
  std::string url1 = "https://example1.com";
  std::string url2 = "https://example2.com";
  AddAbusiveNotification(url1, ContentSetting::CONTENT_SETTING_ALLOW,
                         /*is_ignored=*/false);
  AddAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_ALLOW,
                         /*is_ignored=*/false);

  auto manager =
      AbusiveNotificationPermissionsManager(mock_database_manager(), hcsm());
  EXPECT_EQ(manager.GetLastAbusiveOrigins().size(), 0u);
  manager.CheckNotificationPermissionOrigins();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(manager.GetLastAbusiveOrigins().size(), 2u);
  EXPECT_TRUE(
      manager.GetLastAbusiveOrigins().contains("https://example1.com/"));
  EXPECT_TRUE(
      manager.GetLastAbusiveOrigins().contains("https://example2.com/"));

  VerifyTimeoutCallbackNotCalled();
}

TEST_F(AbusiveNotificationPermissionsManagerTest,
       DoesNotAddSafeAbusiveNotificationSitesToRevokedOriginSet) {
  std::string url1 = "https://example1.com";
  std::string url2 = "https://example2.com";
  AddSafeNotification(url1, ContentSetting::CONTENT_SETTING_ALLOW);
  AddAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_ALLOW,
                         /*is_ignored=*/false);

  auto manager =
      AbusiveNotificationPermissionsManager(mock_database_manager(), hcsm());
  EXPECT_EQ(manager.GetLastAbusiveOrigins().size(), 0u);
  manager.CheckNotificationPermissionOrigins();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(manager.GetLastAbusiveOrigins().size(), 1u);
  EXPECT_TRUE(
      manager.GetLastAbusiveOrigins().contains("https://example2.com/"));

  VerifyTimeoutCallbackNotCalled();
}

TEST_F(AbusiveNotificationPermissionsManagerTest,
       DoesNotAddBlockedSettingToRevokedList) {
  std::string url1 = "https://example1.com";
  std::string url2 = "https://example2.com";
  std::string url3 = "https://example3.com";
  AddAbusiveNotification(url1, ContentSetting::CONTENT_SETTING_ALLOW,
                         /*is_ignored=*/false);
  AddAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_BLOCK,
                         /*is_ignored=*/false);
  AddAbusiveNotification(url3, ContentSetting::CONTENT_SETTING_BLOCK,
                         /*is_ignored=*/true);

  auto manager =
      AbusiveNotificationPermissionsManager(mock_database_manager(), hcsm());
  EXPECT_EQ(manager.GetLastAbusiveOrigins().size(), 0u);
  manager.CheckNotificationPermissionOrigins();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(manager.GetLastAbusiveOrigins().size(), 1u);
  EXPECT_TRUE(
      manager.GetLastAbusiveOrigins().contains("https://example1.com/"));

  VerifyTimeoutCallbackNotCalled();
}

TEST_F(AbusiveNotificationPermissionsManagerTest,
       DoesNotAddIgnoredSettingToRevokedList) {
  std::string url1 = "https://example1.com";
  std::string url2 = "https://example2.com";
  std::string url3 = "https://example3.com";
  AddAbusiveNotification(url1, ContentSetting::CONTENT_SETTING_ALLOW,
                         /*is_ignored=*/false);
  AddAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_BLOCK,
                         /*is_ignored=*/true);
  AddAbusiveNotification(url3, ContentSetting::CONTENT_SETTING_ALLOW,
                         /*is_ignored=*/true);

  auto manager =
      AbusiveNotificationPermissionsManager(mock_database_manager(), hcsm());
  EXPECT_EQ(manager.GetLastAbusiveOrigins().size(), 0u);
  manager.CheckNotificationPermissionOrigins();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(manager.GetLastAbusiveOrigins().size(), 1u);
  EXPECT_TRUE(
      manager.GetLastAbusiveOrigins().contains("https://example1.com/"));

  VerifyTimeoutCallbackNotCalled();
}

TEST_F(AbusiveNotificationPermissionsManagerTest,
       DoesNotAddAbusiveNotificationSitesOnTimeout) {
  std::string url1 = "https://example1.com";
  std::string url2 = "https://example2.com";
  AddAbusiveNotification(url1, ContentSetting::CONTENT_SETTING_ALLOW,
                         /*is_ignored=*/false);
  AddAbusiveNotification(url2, ContentSetting::CONTENT_SETTING_ALLOW,
                         /*is_ignored=*/false);

  auto manager =
      AbusiveNotificationPermissionsManager(mock_database_manager(), hcsm());
  manager.SetNullSBCheckDelayForTesting();
  EXPECT_EQ(manager.GetLastAbusiveOrigins().size(), 0u);
  EXPECT_FALSE(mock_database_manager()->HasCalledCancelCheck());
  manager.CheckNotificationPermissionOrigins();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_database_manager()->HasCalledCancelCheck());
  EXPECT_EQ(manager.GetLastAbusiveOrigins().size(), 0u);
}
