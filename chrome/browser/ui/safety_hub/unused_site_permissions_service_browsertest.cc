// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/safety_hub/mock_safe_browsing_database_manager.h"
#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service_factory.h"
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
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_test.h"

namespace {

const char histogram_name[] =
    "Settings.SafetyHub.UnusedSitePermissionsModule.AutoRevoked2";

}  // namespace

class UnusedSitePermissionsServiceBrowserTest : public InProcessBrowserTest {
 public:
  UnusedSitePermissionsServiceBrowserTest() {
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

IN_PROC_BROWSER_TEST_F(UnusedSitePermissionsServiceBrowserTest,
                       TestNavigationUpdatesLastUsedDate) {
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  auto* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(browser()->profile());
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
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service);
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
  service->SetClockForTesting(base::DefaultClock::GetInstance());
}

// Test that navigations work fine in incognito mode.
IN_PROC_BROWSER_TEST_F(UnusedSitePermissionsServiceBrowserTest,
                       TestIncognitoProfile) {
  GURL url = embedded_test_server()->GetURL("/title1.html");
  auto* otr_browser = OpenURLOffTheRecord(browser()->profile(), url);
  ASSERT_FALSE(UnusedSitePermissionsServiceFactory::GetForProfile(
      otr_browser->profile()));
}

// Test that revocation is happen correctly when auto-revoke is on.
IN_PROC_BROWSER_TEST_F(UnusedSitePermissionsServiceBrowserTest,
                       TestRevokeUnusedPermissions) {
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  auto* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(browser()->profile());
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
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service);
  ASSERT_EQ(service->GetTrackedUnusedPermissionsForTesting().size(), 1u);
  ASSERT_EQ(GetRevokedUnusedPermissions(map).size(), 0u);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(url, url, ContentSettingsType::GEOLOCATION));

  // Travel through time for 40 days to make permissions be revoked.
  clock.Advance(base::Days(40));

  // Check if the content setting turn to ASK, when auto-revocation happens.
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service);
  ASSERT_EQ(service->GetTrackedUnusedPermissionsForTesting().size(), 0u);
  ASSERT_EQ(GetRevokedUnusedPermissions(map).size(), 1u);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(url, url, ContentSettingsType::GEOLOCATION));

  map->SetClockForTesting(base::DefaultClock::GetInstance());
  service->SetClockForTesting(base::DefaultClock::GetInstance());
}

// Test that revocation happens correctly for all content setting types.
IN_PROC_BROWSER_TEST_F(UnusedSitePermissionsServiceBrowserTest,
                       RevokeAllContentSettingTypes) {
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  auto* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(browser()->profile());

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
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service);

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
        UnusedSitePermissionsService::ConvertKeyToContentSettingsType(
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
    : public UnusedSitePermissionsServiceBrowserTest {
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

// Test that revocation is happen correctly when auto-revoke is on.
IN_PROC_BROWSER_TEST_F(AbusiveNotificationPermissionsRevocationBrowserTest,
                       TestRevokeAbusiveNotificationPermissions) {
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  auto* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(browser()->profile());
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
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service);
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

// Test that revocation is happen correctly when auto-revoke is on for a site
// that is unused then abusive.
IN_PROC_BROWSER_TEST_F(AbusiveNotificationPermissionsRevocationBrowserTest,
                       TestSiteWithFirstUnusedThenAbusivePermissions) {
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  auto* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(browser()->profile());
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
    safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service);
    ASSERT_EQ(GetRevokedUnusedPermissions(map).size(), 0u);
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              map->GetContentSetting(unused_url, unused_url,
                                     ContentSettingsType::GEOLOCATION));

    // Travel through time for 40 days to make permissions be revoked.
    clock.Advance(base::Days(40));

    // Check if the content setting turn to ASK, when auto-revocation happens.
    safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service);
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
    safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service);
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

    map->SetClockForTesting(base::DefaultClock::GetInstance());
    service->SetClockForTesting(base::DefaultClock::GetInstance());
  }
}

// Test that revocation is happen correctly when auto-revoke is on for a site
// that is abusive then unused.
IN_PROC_BROWSER_TEST_F(AbusiveNotificationPermissionsRevocationBrowserTest,
                       TestSiteWithFirstAbusiveThenUnusedPermissions) {
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  auto* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(browser()->profile());

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
    safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service);
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
    safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service);
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
    safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service);
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

    map->SetClockForTesting(base::DefaultClock::GetInstance());
    service->SetClockForTesting(base::DefaultClock::GetInstance());
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
      UnusedSitePermissionsServiceFactory::GetForProfile(browser()->profile());
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
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service);
  ASSERT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(map).size(),
      0u);
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      map->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));
}
