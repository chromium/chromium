// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ctime>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/browser/ui/webui/settings/site_settings_permissions_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/constants.h"
#include "components/permissions/unused_site_permissions_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

constexpr char kUnusedTestSite[] = "https://example1.com";
constexpr char kUsedTestSite[] = "https://example2.com";
constexpr ContentSettingsType kUnusedPermission =
    ContentSettingsType::GEOLOCATION;

class SiteSettingsPermissionsHandlerTest : public testing::Test {
 public:
  SiteSettingsPermissionsHandlerTest() {
    feature_list_.InitAndEnableFeature(
        content_settings::features::kSafetyCheckUnusedSitePermissions);
  }

  void SetUp() override {
    // Fully initialize |profile_| in the constructor since some children
    // classes need it right away for SetUp().
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();

    // Set clock for HostContentSettingsMap.
    base::Time time;
    ASSERT_TRUE(base::Time::FromString("2022-09-07 13:00", &time));
    clock_.SetNow(time);
    hcsm_ = HostContentSettingsMapFactory::GetForProfile(profile());
    hcsm_->SetClockForTesting(&clock_);

    handler_ = std::make_unique<SiteSettingsPermissionsHandler>(profile());
    handler()->set_web_ui(web_ui());
    handler()->AllowJavascript();

    // Create a revoked permission.
    base::Value::Dict dict = base::Value::Dict();
    base::Value::List permission_type_list = base::Value::List();
    permission_type_list.Append(
        static_cast<int32_t>(ContentSettingsType::GEOLOCATION));
    dict.Set(permissions::kRevokedKey,
             base::Value::List(std::move(permission_type_list)));
    const content_settings::ContentSettingConstraints constraint{
        .expiration =
            clock()->Now() +
            content_settings::features::
                kSafetyCheckUnusedSitePermissionsRevocationCleanUpThreshold
                    .Get()};

    hcsm()->SetWebsiteSettingDefaultScope(
        GURL(kUnusedTestSite), GURL(kUnusedTestSite),
        ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
        base::Value(dict.Clone()), constraint);

    // There should be only an unused URL in the revoked permissions list.
    const auto& revoked_permissions =
        handler()->PopulateUnusedSitePermissionsData();
    EXPECT_EQ(revoked_permissions.size(), 1UL);
    EXPECT_EQ(GURL(kUnusedTestSite),
              GURL(*revoked_permissions[0].GetDict().FindString(
                  site_settings::kOrigin)));
    handler()->SetClockForTesting(&clock_);
  }

  void TearDown() override {
    if (profile_) {
      auto* partition = profile_->GetDefaultStoragePartition();
      if (partition) {
        partition->WaitForDeletionTasksForTesting();
      }
    }
  }

  void ExpectRevokedPermission() {
    ContentSettingsForOneType revoked_permissions_list;
    hcsm()->GetSettingsForOneType(
        ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
        &revoked_permissions_list);
    EXPECT_EQ(1U, revoked_permissions_list.size());
    EXPECT_EQ(
        ContentSetting::CONTENT_SETTING_ASK,
        hcsm()->GetContentSetting(GURL(kUnusedTestSite), GURL(kUnusedTestSite),
                                  kUnusedPermission));
  }

  TestingProfile* profile() { return profile_.get(); }
  content::TestWebUI* web_ui() { return &web_ui_; }
  SiteSettingsPermissionsHandler* handler() { return handler_.get(); }
  HostContentSettingsMap* hcsm() { return hcsm_.get(); }
  base::SimpleTestClock* clock() { return &clock_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<SiteSettingsPermissionsHandler> handler_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebUI web_ui_;
  scoped_refptr<HostContentSettingsMap> hcsm_;
  base::SimpleTestClock clock_;
};

TEST_F(SiteSettingsPermissionsHandlerTest, PopulateUnusedSitePermissionsData) {
  // Add GEOLOCATION setting for url but do not add to revoked list.
  const content_settings::ContentSettingConstraints constraint{
      .expiration =
          clock()->Now() +
          content_settings::features::
              kSafetyCheckUnusedSitePermissionsRevocationCleanUpThreshold.Get(),
      .track_last_visit_for_autoexpiration = true};
  hcsm()->SetContentSettingDefaultScope(
      GURL(kUsedTestSite), GURL(kUsedTestSite),
      ContentSettingsType::GEOLOCATION, ContentSetting::CONTENT_SETTING_ALLOW,
      constraint);

  // Revoked permissions list should still only contain the initial unused site.
  const auto& revoked_permissions =
      handler()->PopulateUnusedSitePermissionsData();
  EXPECT_EQ(revoked_permissions.size(), 1UL);
  EXPECT_EQ(GURL(kUnusedTestSite),
            GURL(*revoked_permissions[0].GetDict().FindString(
                site_settings::kOrigin)));
}

TEST_F(SiteSettingsPermissionsHandlerTest,
       HandleAllowPermissionsAgainForUnusedSite) {
  // Advance 14 days; this will be the expected histogram sample.
  clock()->Advance(base::Days(14));
  base::HistogramTester histogram_tester;
  base::Value::List initial_unused_site_permissions =
      handler()->PopulateUnusedSitePermissionsData();
  ExpectRevokedPermission();

  // Allow the revoked permission for the unused site again.
  base::Value::List args;
  args.Append(base::Value(kUnusedTestSite));
  handler()->HandleAllowPermissionsAgainForUnusedSite(args);

  // Only a single entry should be recorded in the histogram.
  const std::vector<base::Bucket> buckets = histogram_tester.GetAllSamples(
      "Settings.SafetyCheck.UnusedSitePermissionsAllowAgainDays");
  EXPECT_EQ(1U, buckets.size());
  // The recorded metric should be the elapsed days since the revocation.
  histogram_tester.ExpectUniqueSample(
      "Settings.SafetyCheck.UnusedSitePermissionsAllowAgainDays", 14, 1);

  // Check there is no origin in revoked permissions list.
  ContentSettingsForOneType revoked_permissions_list;
  hcsm()->GetSettingsForOneType(
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      &revoked_permissions_list);
  EXPECT_EQ(0U, revoked_permissions_list.size());
  // Check if the permissions of url is regranted.
  EXPECT_EQ(
      ContentSetting::CONTENT_SETTING_ALLOW,
      hcsm()->GetContentSetting(GURL(kUnusedTestSite), GURL(kUnusedTestSite),
                                kUnusedPermission));

  // Undoing restores the initial state.
  handler()->HandleUndoAllowPermissionsAgainForUnusedSite(
      std::move(initial_unused_site_permissions));
  ExpectRevokedPermission();
}

TEST_F(SiteSettingsPermissionsHandlerTest,
       HandleAcknowledgeRevokedUnusedSitePermissionsList) {
  const auto& revoked_permissions_before =
      handler()->PopulateUnusedSitePermissionsData();
  EXPECT_GT(revoked_permissions_before.size(), 0U);
  // Acknowledging revoked permissions from unused sites clears the list.
  base::Value::List args;
  handler()->HandleAcknowledgeRevokedUnusedSitePermissionsList(args);
  const auto& revoked_permissions_after =
      handler()->PopulateUnusedSitePermissionsData();
  EXPECT_EQ(revoked_permissions_after.size(), 0U);

  // Undo reverts the list to its initial state.
  base::Value::List undo_args;
  undo_args.Append(revoked_permissions_before.Clone());
  handler()->HandleUndoAcknowledgeRevokedUnusedSitePermissionsList(undo_args);
  EXPECT_EQ(revoked_permissions_before,
            handler()->PopulateUnusedSitePermissionsData());
}
