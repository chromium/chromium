// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/features.h"
#include "content/public/test/browser_test.h"

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
  safety_hub_test_util::UpdateSafetyHubServiceAsync(service);
  ASSERT_EQ(service->GetTrackedUnusedPermissionsForTesting().size(), 1u);

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
  safety_hub_test_util::UpdateSafetyHubServiceAsync(service);
  ASSERT_EQ(service->GetTrackedUnusedPermissionsForTesting().size(), 1u);
  ASSERT_EQ(GetRevokedUnusedPermissions(map).size(), 0u);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(url, url, ContentSettingsType::GEOLOCATION));

  // Travel through time for 40 days to make permissions be revoked.
  clock.Advance(base::Days(40));

  // Check if the content setting turn to ASK, when auto-revocation happens.
  safety_hub_test_util::UpdateSafetyHubServiceAsync(service);
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

  // Allow all content settings in the content setting registry.
  auto* content_settings_registry =
      content_settings::ContentSettingsRegistry::GetInstance();
  for (const content_settings::ContentSettingsInfo* info :
       *content_settings_registry) {
    ContentSettingsType type = info->website_settings_info()->type();

    // Add last visited timestamp if the setting can be tracked.
    content_settings::ContentSettingConstraints constraint;
    if (content_settings::CanTrackLastVisit(type)) {
      constraint.set_track_last_visit_for_autoexpiration(true);
    }

    if (!content_settings_registry->Get(type)->IsSettingValid(
            ContentSetting::CONTENT_SETTING_ALLOW)) {
      continue;
    }

    const GURL url("https://example1.com");
    map->SetContentSettingDefaultScope(GURL(url), GURL(url), type,
                                       ContentSetting::CONTENT_SETTING_ALLOW,
                                       constraint);
  }

  // Travel through time for 70 days to make permissions be revoked.
  clock.Advance(base::Days(70));
  safety_hub_test_util::UpdateSafetyHubServiceAsync(service);
  ASSERT_EQ(GetRevokedUnusedPermissions(map).size(), 1u);

  // Navigate to content settings page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("chrome://settings/content")));
}
