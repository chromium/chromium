// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/revoked_permissions_service.h"

#include "base/json/values_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/safety_hub/disruptive_notification_permissions_manager.h"
#include "chrome/browser/ui/safety_hub/mock_safe_browsing_database_manager.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_uma_util.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/constants.h"
#include "components/permissions/features.h"
#include "components/permissions/notifications_engagement_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"

namespace {

using testing::Eq;
using testing::Field;
using testing::Not;
using testing::Optional;

const char histogram_name[] =
    "Settings.SafetyHub.UnusedSitePermissionsModule.AutoRevoked2";

}  // namespace

class RevokedPermissionsServiceBrowserTest : public InProcessBrowserTest {
 public:
  using DisruptiveNotificationRevocationEntry =
      DisruptiveNotificationPermissionsManager::RevocationEntry;
  using DisruptiveNotificationContentSettingHelper =
      DisruptiveNotificationPermissionsManager::ContentSettingHelper;
  using DisruptiveNotificationRevocationState =
      DisruptiveNotificationPermissionsManager::RevocationState;

  RevokedPermissionsServiceBrowserTest() {
    feature_list.InitWithFeatures(
        /*enabled_features=*/
        {content_settings::features::kSafetyCheckUnusedSitePermissions},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  ContentSettingsForOneType GetRevokedUnusedPermissions(
      HostContentSettingsMap* hcsm) {
    return hcsm->GetSettingsForOneType(
        ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS);
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

IN_PROC_BROWSER_TEST_F(RevokedPermissionsServiceBrowserTest,
                       TestNavigationUpdatesLastUsedDate) {
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  auto* service =
      RevokedPermissionsServiceFactory::GetForProfile(browser()->profile());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  // Create content setting 20 days in the past.
  // TODO(crbug.com/40250875): Move code below to a helper method.
  base::Time now(base::Time::Now());
  base::Time past(now - base::Days(20));
  base::SimpleTestClock clock;
  clock.SetNow(past);
  map->SetClockForTesting(&clock);
  service->SetClockForTesting(&clock);
  content_settings::ContentSettingConstraints constraints;
  constraints.set_track_last_visit_for_autoexpiration(true);
  map->SetContentSettingDefaultScope(url, url, ContentSettingsType::GEOLOCATION,
                                     CONTENT_SETTING_ALLOW, constraints);
  clock.SetNow(now);
  safety_hub_test_util::UpdateRevokedPermissionsServiceAsync(service);
  ASSERT_EQ(service->GetTrackedUnusedPermissionsForTesting().size(), 1u);
  ASSERT_EQ(GetRevokedUnusedPermissions(map).size(), 0u);

  // Check that the timestamp is initially in the past.
  content_settings::SettingInfo info;
  map->GetWebsiteSetting(url, url, ContentSettingsType::GEOLOCATION, &info);
  ASSERT_FALSE(info.metadata.last_visited().is_null());
  EXPECT_GE(info.metadata.last_visited(),
            past - content_settings::GetCoarseVisitedTimePrecision());
  EXPECT_LE(info.metadata.last_visited(), past);

  // Navigate to |url|.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Check that the timestamp is updated after a navigation.
  map->GetWebsiteSetting(url, url, ContentSettingsType::GEOLOCATION, &info);
  EXPECT_GE(info.metadata.last_visited(),
            now - content_settings::GetCoarseVisitedTimePrecision());
  EXPECT_LE(info.metadata.last_visited(), now);

  map->SetClockForTesting(base::DefaultClock::GetInstance());
}

// Test that navigations work fine in incognito mode.
IN_PROC_BROWSER_TEST_F(RevokedPermissionsServiceBrowserTest,
                       TestIncognitoProfile) {
  GURL url = embedded_test_server()->GetURL("/title1.html");
  auto* otr_browser = OpenURLOffTheRecord(browser()->profile(), url);
  ASSERT_FALSE(
      RevokedPermissionsServiceFactory::GetForProfile(otr_browser->profile()));
}

// Test that revocation is happen correctly when auto-revoke is on.
IN_PROC_BROWSER_TEST_F(RevokedPermissionsServiceBrowserTest,
                       TestRevokeUnusedPermissions) {
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  auto* service =
      RevokedPermissionsServiceFactory::GetForProfile(browser()->profile());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  // Create content setting 20 days in the past.
  base::Time now(base::Time::Now());
  base::Time past(now - base::Days(20));
  base::SimpleTestClock clock;
  clock.SetNow(past);
  map->SetClockForTesting(&clock);
  service->SetClockForTesting(&clock);
  content_settings::ContentSettingConstraints constraints;
  constraints.set_track_last_visit_for_autoexpiration(true);
  map->SetContentSettingDefaultScope(url, url, ContentSettingsType::GEOLOCATION,
                                     CONTENT_SETTING_ALLOW, constraints);
  clock.SetNow(now);

  // Check if the content setting is still ALLOW, before auto-revocation.
  safety_hub_test_util::UpdateRevokedPermissionsServiceAsync(service);
  ASSERT_EQ(service->GetTrackedUnusedPermissionsForTesting().size(), 1u);
  ASSERT_EQ(GetRevokedUnusedPermissions(map).size(), 0u);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(url, url, ContentSettingsType::GEOLOCATION));

  // Travel through time for 40 days to make permissions be revoked.
  clock.Advance(base::Days(40));

  // Check if the content setting turn to ASK, when auto-revocation happens.
  safety_hub_test_util::UpdateRevokedPermissionsServiceAsync(service);
  ASSERT_EQ(service->GetTrackedUnusedPermissionsForTesting().size(), 0u);
  ASSERT_EQ(GetRevokedUnusedPermissions(map).size(), 1u);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(url, url, ContentSettingsType::GEOLOCATION));
}

// Test that revocation happens correctly for all content setting types.
IN_PROC_BROWSER_TEST_F(RevokedPermissionsServiceBrowserTest,
                       RevokeAllContentSettingTypes) {
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  auto* service =
      RevokedPermissionsServiceFactory::GetForProfile(browser()->profile());

  base::Time time;
  ASSERT_TRUE(base::Time::FromString("2022-09-07 13:00", &time));
  base::SimpleTestClock clock;
  clock.SetNow(time);
  map->SetClockForTesting(&clock);
  service->SetClockForTesting(&clock);

  GURL url = embedded_test_server()->GetURL("/title1.html");
  base::HistogramTester histogram_tester;

  // TODO(b/338365161): Remove the skip list, once the bug is fixed. Currently,
  // when CAMERA_PAN_TILT_ZOOM is allowed, MEDIASTREAM_CAMERA is also allowed
  // under the hood without passing predefined constraints. Since the
  // auto-revokation relies on the constraint, the MEDIASTREAM_CAMERA ends up
  // not being revoked - although it is should be.
  const std::vector<ContentSettingsType> skip_list = {
      ContentSettingsType::CAMERA_PAN_TILT_ZOOM};

  // Allow all content settings in the content setting registry.
  std::vector<ContentSettingsType> allowed_permission_types;
  auto* content_settings_registry =
      content_settings::ContentSettingsRegistry::GetInstance();
  for (const content_settings::ContentSettingsInfo* info :
       *content_settings_registry) {
    ContentSettingsType type = info->website_settings_info()->type();

    // Skip if the setting's last visit can not be tracked.
    content_settings::ContentSettingConstraints constraint;
    if (!content_settings::CanTrackLastVisit(type)) {
      continue;
    }
    // Add last visited timestamp if the setting can be tracked.
    constraint.set_track_last_visit_for_autoexpiration(true);

    // Skip if the setting can not be set to ALLOW.
    if (!content_settings_registry->Get(type)->IsSettingValid(
            ContentSetting::CONTENT_SETTING_ALLOW)) {
      continue;
    }

    // Skip if the setting produces patterns that do not get revoked.
    content_settings::PatternPair patterns =
        map->GetPatternsForContentSettingsType(url, url, type);
    if (!patterns.first.MatchesSingleOrigin() ||
        patterns.second != ContentSettingsPattern::Wildcard()) {
      continue;
    }

    // Skip if the setting in the skip list.
    if (base::Contains(skip_list, type)) {
      continue;
    }

    map->SetContentSettingDefaultScope(GURL(url), GURL(url), type,
                                       ContentSetting::CONTENT_SETTING_ALLOW,
                                       constraint);
    allowed_permission_types.push_back(type);
  }

  // Travel through time for 70 days to make permissions be revoked.
  clock.Advance(base::Days(70));
  safety_hub_test_util::UpdateRevokedPermissionsServiceAsync(service);

  // Assert there are revoked permission only for one origin.
  ContentSettingsForOneType revoked_permissions =
      GetRevokedUnusedPermissions(map);
  EXPECT_EQ(revoked_permissions.size(), 1u);

  // Get the revoked permission types for the origin.
  const base::Value::Dict& permission_types_by_values =
      revoked_permissions[0].setting_value.GetDict();
  const base::Value::List revoked_permission_types =
      permission_types_by_values.FindList(permissions::kRevokedKey)->Clone();

  // Assert all the allowed permissions are revoked.
  EXPECT_EQ(allowed_permission_types.size(), revoked_permission_types.size());

  for (int i = 0; i < (int)revoked_permission_types.size(); i++) {
    ContentSettingsType revoked_permission_type =
        RevokedPermissionsService::ConvertKeyToContentSettingsType(
            revoked_permission_types[i].GetString());
    EXPECT_EQ(allowed_permission_types[i], revoked_permission_type);
  }

  // Assert all auto-revocations are recorded in UMA metrics.
  EXPECT_EQ(allowed_permission_types.size(),
            histogram_tester.GetAllSamples(histogram_name).size());
  for (const ContentSettingsType type : allowed_permission_types) {
    histogram_tester.ExpectBucketCount(
        histogram_name,
        content_settings_uma_util::ContentSettingTypeToHistogramValue(type), 1);
  }

  // Navigate to content settings page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIContentSettingsURL)));
}

class AbusiveNotificationPermissionsRevocationBrowserTest
    : public RevokedPermissionsServiceBrowserTest {
 public:
  AbusiveNotificationPermissionsRevocationBrowserTest() {
    safe_browsing_factory_ =
        std::make_unique<safe_browsing::TestSafeBrowsingServiceFactory>();
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {safe_browsing::kSafetyHubAbusiveNotificationRevocation,
         content_settings::features::kSafetyCheckUnusedSitePermissions},
        /*disabled_features=*/{});
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    fake_database_manager_ =
        base::MakeRefCounted<MockSafeBrowsingDatabaseManager>();
    safe_browsing_factory_->SetTestDatabaseManager(
        fake_database_manager_.get());
    safe_browsing::SafeBrowsingServiceInterface::RegisterFactory(
        safe_browsing_factory_.get());
  }

  void AddDangerousUrl(const GURL& url) {
    mock_database_manager()->SetThreatTypeForUrl(
        url, safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
  }

  MockSafeBrowsingDatabaseManager* mock_database_manager() {
    return fake_database_manager_.get();
  }

 private:
  scoped_refptr<MockSafeBrowsingDatabaseManager> fake_database_manager_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      safe_browsing_factory_;
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/400648091): Re-enable the test when it's fixed.
// Test that revocation is happen correctly when auto-revoke is on.
IN_PROC_BROWSER_TEST_F(AbusiveNotificationPermissionsRevocationBrowserTest,
                       DISABLED_TestRevokeAbusiveNotificationPermissions) {
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  auto* service =
      RevokedPermissionsServiceFactory::GetForProfile(browser()->profile());
  const GURL url("https://example1.com");
  AddDangerousUrl(url);
  base::HistogramTester histogram_tester;

  // Create granted abusive notification permission.
  map->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  ASSERT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(map).size(),
      0u);

  // Check if the content setting turn to ASK, when auto-revocation happens.
  safety_hub_test_util::UpdateRevokedPermissionsServiceAsync(service);
  ASSERT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(map).size(),
      1u);
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      map->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));

  // Assert notification auto-revocation is recorded in UMA metrics.
  EXPECT_EQ(1u, histogram_tester.GetAllSamples(histogram_name).size());
  histogram_tester.ExpectBucketCount(
      histogram_name,
      content_settings_uma_util::ContentSettingTypeToHistogramValue(
          ContentSettingsType::NOTIFICATIONS),
      1);
}

// TODO(crbug.com/400645286): Re-enable the test when it's fixed.
// Test that revocation is happen correctly when auto-revoke is on for a site
// that is unused then abusive.
IN_PROC_BROWSER_TEST_F(AbusiveNotificationPermissionsRevocationBrowserTest,
                       DISABLED_TestSiteWithFirstUnusedThenAbusivePermissions) {
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  auto* service =
      RevokedPermissionsServiceFactory::GetForProfile(browser()->profile());
  // Test cases where there is a single URL which is both abusive and unused,
  // then when there are separate abusive and unused URLs.
  for (const auto& [abusive_url, unused_url] :
       std::vector<std::pair<GURL, GURL>>{
           {GURL("https://example1.com"), GURL("https://example1.com")},
           {GURL("https://example2.com"), GURL("https://example3.com")}}) {
    AddDangerousUrl(abusive_url);

    // Create content setting 20 days in the past.
    base::Time now(base::Time::Now());
    base::Time past(now - base::Days(20));
    base::SimpleTestClock clock;
    clock.SetNow(past);
    map->SetClockForTesting(&clock);
    service->SetClockForTesting(&clock);
    content_settings::ContentSettingConstraints constraints;
    constraints.set_track_last_visit_for_autoexpiration(true);
    map->SetContentSettingDefaultScope(unused_url, unused_url,
                                       ContentSettingsType::GEOLOCATION,
                                       CONTENT_SETTING_ALLOW, constraints);
    clock.SetNow(now);

    // Check if the content setting is still ALLOW, before auto-revocation.
    safety_hub_test_util::UpdateRevokedPermissionsServiceAsync(service);
    ASSERT_EQ(GetRevokedUnusedPermissions(map).size(), 0u);
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              map->GetContentSetting(unused_url, unused_url,
                                     ContentSettingsType::GEOLOCATION));

    // Travel through time for 40 days to make permissions be revoked.
    clock.Advance(base::Days(40));

    // Check if the content setting turn to ASK, when auto-revocation happens.
    safety_hub_test_util::UpdateRevokedPermissionsServiceAsync(service);
    ASSERT_EQ(GetRevokedUnusedPermissions(map).size(), 1u);
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(unused_url, unused_url,
                                     ContentSettingsType::GEOLOCATION));

    // Travel through time for 20 more days, then revoke abusive notifications
    // immediately.
    clock.Advance(base::Days(20));
    map->SetContentSettingDefaultScope(abusive_url, abusive_url,
                                       ContentSettingsType::NOTIFICATIONS,
                                       CONTENT_SETTING_ALLOW);
    safety_hub_test_util::UpdateRevokedPermissionsServiceAsync(service);
    ASSERT_GT(
        safety_hub_util::GetRevokedAbusiveNotificationPermissions(map).size(),
        0u);
    EXPECT_TRUE(safety_hub_test_util::IsUrlInSettingsList(
        safety_hub_util::GetRevokedAbusiveNotificationPermissions(map),
        abusive_url));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(unused_url, unused_url,
                                     ContentSettingsType::GEOLOCATION));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(abusive_url, abusive_url,
                                     ContentSettingsType::NOTIFICATIONS));
  }
}

// TODO(crbug.com/400648762): Re-enable the test when it's fixed.
// Test that revocation is happen correctly when auto-revoke is on for a site
// that is abusive then unused.
IN_PROC_BROWSER_TEST_F(AbusiveNotificationPermissionsRevocationBrowserTest,
                       DISABLED_TestSiteWithFirstAbusiveThenUnusedPermissions) {
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  auto* service =
      RevokedPermissionsServiceFactory::GetForProfile(browser()->profile());

  // Test cases where there is a single URL which is both abusive and unused,
  // then when there are separate abusive and unused URLs.
  for (const auto& [abusive_url, unused_url] :
       std::vector<std::pair<GURL, GURL>>{
           {GURL("https://example1.com"), GURL("https://example1.com")},
           {GURL("https://example2.com"), GURL("https://example3.com")}}) {
    AddDangerousUrl(abusive_url);

    // Create content setting 20 days in the past.
    base::Time now(base::Time::Now());
    base::Time past(now - base::Days(20));
    base::SimpleTestClock clock;
    clock.SetNow(past);
    map->SetClockForTesting(&clock);
    service->SetClockForTesting(&clock);
    content_settings::ContentSettingConstraints constraints;
    constraints.set_track_last_visit_for_autoexpiration(true);
    map->SetContentSettingDefaultScope(unused_url, unused_url,
                                       ContentSettingsType::GEOLOCATION,
                                       CONTENT_SETTING_ALLOW, constraints);
    clock.SetNow(now);

    // Check if the content setting is still ALLOW, before auto-revocation.
    safety_hub_test_util::UpdateRevokedPermissionsServiceAsync(service);
    ASSERT_LT(GetRevokedUnusedPermissions(map).size(), 2u);
    EXPECT_FALSE(safety_hub_test_util::IsUrlInSettingsList(
        GetRevokedUnusedPermissions(map), unused_url));
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              map->GetContentSetting(unused_url, unused_url,
                                     ContentSettingsType::GEOLOCATION));

    // Travel through time for 20 more days, then revoke abusive notifications
    // immediately. Note the unused permission is not yet revoked.
    clock.Advance(base::Days(20));
    map->SetContentSettingDefaultScope(abusive_url, abusive_url,
                                       ContentSettingsType::NOTIFICATIONS,
                                       CONTENT_SETTING_ALLOW);
    safety_hub_test_util::UpdateRevokedPermissionsServiceAsync(service);
    ASSERT_LT(GetRevokedUnusedPermissions(map).size(), 2u);
    EXPECT_FALSE(safety_hub_test_util::IsUrlInSettingsList(
        GetRevokedUnusedPermissions(map), unused_url));
    ASSERT_GT(
        safety_hub_util::GetRevokedAbusiveNotificationPermissions(map).size(),
        0u);
    EXPECT_TRUE(safety_hub_test_util::IsUrlInSettingsList(
        safety_hub_util::GetRevokedAbusiveNotificationPermissions(map),
        abusive_url));
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              map->GetContentSetting(unused_url, unused_url,
                                     ContentSettingsType::GEOLOCATION));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(abusive_url, abusive_url,
                                     ContentSettingsType::NOTIFICATIONS));

    // Travel through time for 20 days to make unused permissions revoked.
    // Abusive permission is still revoked.
    clock.Advance(base::Days(20));
    safety_hub_test_util::UpdateRevokedPermissionsServiceAsync(service);
    ASSERT_GT(GetRevokedUnusedPermissions(map).size(), 0u);
    EXPECT_TRUE(safety_hub_test_util::IsUrlInSettingsList(
        GetRevokedUnusedPermissions(map), unused_url));
    ASSERT_GT(
        safety_hub_util::GetRevokedAbusiveNotificationPermissions(map).size(),
        0u);
    EXPECT_TRUE(safety_hub_test_util::IsUrlInSettingsList(
        safety_hub_util::GetRevokedAbusiveNotificationPermissions(map),
        abusive_url));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(unused_url, unused_url,
                                     ContentSettingsType::GEOLOCATION));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(abusive_url, abusive_url,
                                     ContentSettingsType::NOTIFICATIONS));
  }
}

class AbusiveNotificationPermissionsRevocationDisabledBrowserTest
    : public AbusiveNotificationPermissionsRevocationBrowserTest {
 public:
  AbusiveNotificationPermissionsRevocationDisabledBrowserTest() {
    safe_browsing_factory_ =
        std::make_unique<safe_browsing::TestSafeBrowsingServiceFactory>();
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {content_settings::features::kSafetyCheckUnusedSitePermissions},
        /*disabled_features=*/
        {safe_browsing::kSafetyHubAbusiveNotificationRevocation});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      safe_browsing_factory_;
};

// Test that revocation is happen correctly when auto-revoke is on.
IN_PROC_BROWSER_TEST_F(
    AbusiveNotificationPermissionsRevocationDisabledBrowserTest,
    TestNoRevokeAbusiveNotificationPermissions) {
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  auto* service =
      RevokedPermissionsServiceFactory::GetForProfile(browser()->profile());
  const GURL url("https://example1.com");
  AddDangerousUrl(url);

  // Create granted abusive notification permission.
  map->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  ASSERT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(map).size(),
      0u);

  // Check if the content setting is still ALLOW and that auto-revocation does
  // not happen.
  safety_hub_test_util::UpdateRevokedPermissionsServiceAsync(service);
  ASSERT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(map).size(),
      0u);
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      map->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));
}

class DisruptiveNotificationPermissionsRevocationShadowRunBrowserTest
    : public RevokedPermissionsServiceBrowserTest {
 public:
  DisruptiveNotificationPermissionsRevocationShadowRunBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kSafetyHubDisruptiveNotificationRevocation,
        {
            {features::kSafetyHubDisruptiveNotificationRevocationShadowRun.name,
             "true"},
        });
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    DisruptiveNotificationPermissionsRevocationShadowRunBrowserTest,
    TestProposeRevokeDisruptiveNotificationPermissions) {
  auto* hcsm =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  auto* service =
      RevokedPermissionsServiceFactory::GetForProfile(browser()->profile());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  // Set up a disruptive notification permission.
  hcsm->SetContentSettingDefaultScope(
      url, GURL(), ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  auto* notifications_engagement_service =
      NotificationsEngagementServiceFactory::GetForProfile(
          browser()->profile());
  notifications_engagement_service->RecordNotificationDisplayed(url, 50);

  safety_hub_test_util::UpdateRevokedPermissionsServiceAsync(service);

  // The url was stored in the disruptive notification content setting.
  EXPECT_THAT(
      DisruptiveNotificationContentSettingHelper(*hcsm).GetRevocationEntry(url),
      Not(Eq(std::nullopt)));

  safety_hub_test_util::UpdateRevokedPermissionsServiceAsync(service);
  std::optional<DisruptiveNotificationRevocationEntry> revocation_entry =
      DisruptiveNotificationContentSettingHelper(*hcsm).GetRevocationEntry(url);
  EXPECT_THAT(
      revocation_entry,
      Optional(Field(&DisruptiveNotificationRevocationEntry::revocation_state,
                     DisruptiveNotificationRevocationState::kProposed)));
  ASSERT_EQ(GetRevokedUnusedPermissions(hcsm).size(), 0u);
  std::optional<std::unique_ptr<SafetyHubService::Result>> opt_result =
      service->GetCachedResult();
  ASSERT_TRUE(opt_result.has_value());
  auto* result =
      static_cast<RevokedPermissionsService::RevokedPermissionsResult*>(
          opt_result.value().get());
  EXPECT_EQ(result->GetRevokedPermissions().size(), 0u);
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      hcsm->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));
}

class DisruptiveNotificationPermissionsRevocationBrowserTest
    : public RevokedPermissionsServiceBrowserTest {
 public:
  DisruptiveNotificationPermissionsRevocationBrowserTest() {
#if BUILDFLAG(IS_ANDROID)
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kSafetyHubDisruptiveNotificationRevocation,
        {{features::kSafetyHubDisruptiveNotificationRevocationShadowRun.name,
          "false"},
         {features::
              kSafetyHubDisruptiveNotificationRevocationWaitingForMetricsDays
                  .name,
          "7"}});
#else
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kSafetyHubDisruptiveNotificationRevocation,
          {{features::kSafetyHubDisruptiveNotificationRevocationShadowRun.name,
            "false"},
           {features::
                kSafetyHubDisruptiveNotificationRevocationWaitingForMetricsDays
                    .name,
            "7"}}}},
        /*disabled_features=*/{
            safe_browsing::kShowWarningsForSuspiciousNotifications});
#endif
  }

  void SetUpOnMainThread() override {
    RevokedPermissionsServiceBrowserTest::SetUpOnMainThread();
    recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  site_engagement::SiteEngagementService* site_engagement_service() {
    return site_engagement::SiteEngagementServiceFactory::GetForProfile(
        browser()->profile());
  }

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> recorder_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that revocation is happening correctly when auto-revoke is on.
IN_PROC_BROWSER_TEST_F(DisruptiveNotificationPermissionsRevocationBrowserTest,
                       TestRevokeDisruptiveNotificationPermissions) {
  auto* hcsm =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  auto* service =
      RevokedPermissionsServiceFactory::GetForProfile(browser()->profile());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  base::SimpleTestClock clock;
  hcsm->SetClockForTesting(&clock);
  service->SetClockForTesting(&clock);
  clock.SetNow(base::Time::Now());

  // Force for the initial safety check to be complete before setting up a
  // disruptive notification. Otherwise, sometimes the check is finished before
  // and sometimes after the setup which causes flakes.
  safety_hub_test_util::UpdateRevokedPermissionsServiceAsync(service);

  // Set up a disruptive notification permission.
  hcsm->SetContentSettingDefaultScope(
      url, GURL(), ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  auto* notifications_engagement_service =
      NotificationsEngagementServiceFactory::GetForProfile(
          browser()->profile());
  notifications_engagement_service->RecordNotificationDisplayed(url, 50);

  safety_hub_test_util::UpdateRevokedPermissionsServiceAsync(service);

  // The url was stored in the disruptive notification content setting.
  std::optional<DisruptiveNotificationRevocationEntry> revocation_entry =
      DisruptiveNotificationContentSettingHelper(*hcsm).GetRevocationEntry(url);
  EXPECT_THAT(
      revocation_entry,
      Optional(Field(&DisruptiveNotificationRevocationEntry::revocation_state,
                     DisruptiveNotificationRevocationState::kProposed)));

  // Wait for the disruptive metrics cooldown to expire.
  clock.Advance(base::Days(8));

  safety_hub_test_util::UpdateRevokedPermissionsServiceAsync(service);
  revocation_entry =
      DisruptiveNotificationContentSettingHelper(*hcsm).GetRevocationEntry(url);
  EXPECT_THAT(
      revocation_entry,
      Optional(Field(&DisruptiveNotificationRevocationEntry::revocation_state,
                     DisruptiveNotificationRevocationState::kRevoked)));

  std::optional<std::unique_ptr<SafetyHubService::Result>> opt_result =
      service->GetCachedResult();
  ASSERT_TRUE(opt_result.has_value());
  auto* result =
      static_cast<RevokedPermissionsService::RevokedPermissionsResult*>(
          opt_result.value().get());
  EXPECT_EQ(result->GetRevokedPermissions().size(), 1u);
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      hcsm->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));
}

IN_PROC_BROWSER_TEST_F(
    DisruptiveNotificationPermissionsRevocationBrowserTest,
    TestRevokeFirstUnusedThenDisruptiveNotificationPermissions) {
  auto* hcsm =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  auto* service =
      RevokedPermissionsServiceFactory::GetForProfile(browser()->profile());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  hcsm->SetContentSettingDefaultScope(
      url, GURL(), ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);

  // Create content setting 20 days in the past.
  base::Time now(base::Time::Now());
  base::Time past(now - base::Days(20));
  base::SimpleTestClock clock;
  clock.SetNow(past);
  hcsm->SetClockForTesting(&clock);
  service->SetClockForTesting(&clock);
  content_settings::ContentSettingConstraints constraints;
  constraints.set_track_last_visit_for_autoexpiration(true);
  hcsm->SetContentSettingDefaultScope(url, url,
                                      ContentSettingsType::GEOLOCATION,
                                      CONTENT_SETTING_ALLOW, constraints);
  clock.SetNow(now);

  safety_hub_test_util::UpdateRevokedPermissionsServiceAsync(service);

  // Travel through time for 40 days.
  clock.Advance(base::Days(40));

  // Set up a disruptive notification permission.
  auto* notifications_engagement_service =
      NotificationsEngagementServiceFactory::GetForProfile(
          browser()->profile());
  notifications_engagement_service->RecordNotificationDisplayed(url, 50);

  // Check if the content setting turn to ASK, when auto-revocation happens.
  safety_hub_test_util::UpdateRevokedPermissionsServiceAsync(service);
  ASSERT_EQ(GetRevokedUnusedPermissions(hcsm).size(), 1u);
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      hcsm->GetContentSetting(url, url, ContentSettingsType::GEOLOCATION));

  // The url was stored in the disruptive notification content setting as
  // proposed revocation.
  std::optional<DisruptiveNotificationRevocationEntry> revocation_entry =
      DisruptiveNotificationContentSettingHelper(*hcsm).GetRevocationEntry(url);
  EXPECT_THAT(
      revocation_entry,
      Optional(Field(&DisruptiveNotificationRevocationEntry::revocation_state,
                     DisruptiveNotificationRevocationState::kProposed)));

  // Wait for the disruptive metrics cooldown to expire.
  clock.Advance(base::Days(8));

  safety_hub_test_util::UpdateRevokedPermissionsServiceAsync(service);
  // Both disruptive notifications and unused permissions were revoked for the
  // URL.
  std::optional<std::unique_ptr<SafetyHubService::Result>> opt_result =
      service->GetCachedResult();
  ASSERT_TRUE(opt_result.has_value());
  auto* result =
      static_cast<RevokedPermissionsService::RevokedPermissionsResult*>(
          opt_result.value().get());
  EXPECT_EQ(result->GetRevokedPermissions().size(), 1u);

  revocation_entry =
      DisruptiveNotificationContentSettingHelper(*hcsm).GetRevocationEntry(url);
  EXPECT_THAT(
      revocation_entry,
      Optional(Field(&DisruptiveNotificationRevocationEntry::revocation_state,
                     DisruptiveNotificationRevocationState::kRevoked)));

  EXPECT_EQ(result->GetRevokedPermissions().size(), 1u);
  EXPECT_EQ(result->GetRevokedPermissions().front().permission_types.size(),
            2u);
  EXPECT_TRUE(result->GetRevokedPermissions().front().permission_types.contains(
      ContentSettingsType::GEOLOCATION));
  EXPECT_TRUE(result->GetRevokedPermissions().front().permission_types.contains(
      ContentSettingsType::NOTIFICATIONS));

  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      hcsm->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      hcsm->GetContentSetting(url, url, ContentSettingsType::GEOLOCATION));
}

IN_PROC_BROWSER_TEST_F(DisruptiveNotificationPermissionsRevocationBrowserTest,
                       TestProposedFalsePositiveOnPageVisit) {
  auto* hcsm =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  // Set up a proposed revoked notification.
  DisruptiveNotificationRevocationEntry revoked_entry(
      /*revocation_state=*/DisruptiveNotificationRevocationState::kProposed,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/5,
      /*timestamp=*/base::Time::Now() - base::Days(3));
  DisruptiveNotificationContentSettingHelper(*hcsm).PersistRevocationEntry(
      url, revoked_entry);

  // Visit the site.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  auto interaction_entries = recorder_->GetEntriesByName(
      "SafetyHub.DisruptiveNotificationRevocations.FalsePositiveInteraction");
  ASSERT_EQ(1u, interaction_entries.size());
  auto* interaction_entry = interaction_entries[0].get();
  recorder_->ExpectEntryMetric(interaction_entry, "DaysSinceRevocation", 3);
  recorder_->ExpectEntryMetric(
      interaction_entry, "Reason",
      static_cast<int>(DisruptiveNotificationPermissionsManager::
                           FalsePositiveReason::kPageVisit));
  // Site engagement hasn't been updated yet.
  recorder_->ExpectEntryMetric(interaction_entry, "NewSiteEngagement", 0.0);
  recorder_->ExpectEntryMetric(interaction_entry, "OldSiteEngagement", 0.0);
  recorder_->ExpectEntryMetric(interaction_entry, "DailyAverageVolume", 5);
}

IN_PROC_BROWSER_TEST_F(DisruptiveNotificationPermissionsRevocationBrowserTest,
                       TestRevokedFalsePositiveOnPageVisit) {
  auto* hcsm =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  // Set up a proposed revoked notification.
  DisruptiveNotificationRevocationEntry proposed_entry(
      /*revocation_state=*/DisruptiveNotificationRevocationState::kProposed,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/5,
      /*timestamp=*/base::Time::Now() - base::Days(3));

  DisruptiveNotificationContentSettingHelper(*hcsm).PersistRevocationEntry(
      url, proposed_entry);

  // Visit the page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  auto interaction_entries = recorder_->GetEntriesByName(
      "SafetyHub.DisruptiveNotificationRevocations.FalsePositiveInteraction");
  ASSERT_EQ(1u, interaction_entries.size());
  auto* interaction_entry = interaction_entries[0].get();
  recorder_->ExpectEntryMetric(interaction_entry, "DaysSinceRevocation", 3);
  recorder_->ExpectEntryMetric(
      interaction_entry, "Reason",
      static_cast<int>(DisruptiveNotificationPermissionsManager::
                           FalsePositiveReason::kPageVisit));
  // Site engagement hasn't been updated yet.
  recorder_->ExpectEntryMetric(interaction_entry, "NewSiteEngagement", 0.0);
  recorder_->ExpectEntryMetric(interaction_entry, "OldSiteEngagement", 0.0);
  recorder_->ExpectEntryMetric(interaction_entry, "DailyAverageVolume", 5);

  auto revocation_entries = recorder_->GetEntriesByName(
      "SafetyHub.DisruptiveNotificationRevocations.FalsePositiveRevocation");
  ASSERT_EQ(1u, revocation_entries.size());
  auto* revocation_entry = revocation_entries[0].get();
  recorder_->ExpectEntryMetric(revocation_entry, "DaysSinceRevocation", 3);
  recorder_->ExpectEntryMetric(revocation_entry, "PageVisitCount", 1);
  recorder_->ExpectEntryMetric(revocation_entry, "NotificationClickCount", 0);
  recorder_->ExpectEntryMetric(revocation_entry, "NewSiteEngagement", 0.0);
  recorder_->ExpectEntryMetric(revocation_entry, "OldSiteEngagement", 0.0);
  recorder_->ExpectEntryMetric(revocation_entry, "DailyAverageVolume", 5);

  // Switch to a different site.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("https://www.another.site")));

  // Visit the page again, the revocation has already reported so it won't be
  // reported again.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(
      1u, recorder_
              ->GetEntriesByName("SafetyHub.DisruptiveNotificationRevocations."
                                 "FalsePositiveInteraction")
              .size());
  EXPECT_EQ(
      1u, recorder_
              ->GetEntriesByName("SafetyHub.DisruptiveNotificationRevocations."
                                 "FalsePositiveRevocation")
              .size());
}

// TODO(crbug.com/406472515): Add a test for non persistent notification click.
IN_PROC_BROWSER_TEST_F(DisruptiveNotificationPermissionsRevocationBrowserTest,
                       TestProposedFalsePositiveOnPersistentNotificationClick) {
  auto* hcsm =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  auto display_service_tester =
      std::make_unique<NotificationDisplayServiceTester>(browser()->profile());
  auto* notifications_service =
      PlatformNotificationServiceFactory::GetForProfile(browser()->profile());

  GURL url = embedded_test_server()->GetURL("/title1.html");
  const char kNotificationId[] = "my-notification-id";

  // Set up a proposed revoked disruptive notification. The notification are not
  // yet revoked.
  hcsm->SetContentSettingDefaultScope(
      url, GURL(), ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);

  DisruptiveNotificationRevocationEntry proposed_entry(
      /*revocation_state=*/DisruptiveNotificationRevocationState::kProposed,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/5,
      /*timestamp=*/base::Time::Now() - base::Days(3));
  DisruptiveNotificationContentSettingHelper(*hcsm).PersistRevocationEntry(
      url, proposed_entry);

  // Show a notification.
  blink::PlatformNotificationData data;
  data.title = u"My notification's title";
  data.body = u"Hello, world!";
  notifications_service->DisplayPersistentNotification(
      kNotificationId, url, url, data, blink::NotificationResources());

  // Click on the notification.
  display_service_tester->SimulateClick(
      NotificationHandler::Type::WEB_PERSISTENT, kNotificationId,
      0 /*action_index*/, std::nullopt);

  auto interaction_entries = recorder_->GetEntriesByName(
      "SafetyHub.DisruptiveNotificationRevocations.FalsePositiveInteraction");
  ASSERT_EQ(1u, interaction_entries.size());
  auto* interaction_entry = interaction_entries[0].get();
  recorder_->ExpectEntryMetric(interaction_entry, "DaysSinceRevocation", 3);
  recorder_->ExpectEntryMetric(
      interaction_entry, "Reason",
      static_cast<int>(DisruptiveNotificationPermissionsManager::
                           FalsePositiveReason::kPersistentNotificationClick));
  recorder_->ExpectEntryMetric(interaction_entry, "NewSiteEngagement", 2.0);
  recorder_->ExpectEntryMetric(interaction_entry, "OldSiteEngagement", 0.0);
  recorder_->ExpectEntryMetric(interaction_entry, "DailyAverageVolume", 5);

  auto revocation_entries = recorder_->GetEntriesByName(
      "SafetyHub.DisruptiveNotificationRevocations.FalsePositiveRevocation");
  ASSERT_EQ(1u, revocation_entries.size());
  auto* revocation_entry = revocation_entries[0].get();
  recorder_->ExpectEntryMetric(revocation_entry, "DaysSinceRevocation", 3);
  recorder_->ExpectEntryMetric(revocation_entry, "PageVisitCount", 0);
  recorder_->ExpectEntryMetric(revocation_entry, "NotificationClickCount", 1);
  recorder_->ExpectEntryMetric(revocation_entry, "NewSiteEngagement", 2.0);
  recorder_->ExpectEntryMetric(revocation_entry, "OldSiteEngagement", 0.0);
  recorder_->ExpectEntryMetric(revocation_entry, "DailyAverageVolume", 5);
}
