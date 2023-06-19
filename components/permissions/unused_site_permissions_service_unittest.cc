// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/unused_site_permissions_service.h"
#include <ctime>
#include <list>
#include <memory>
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/constants.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace permissions {
class UnusedSitePermissionsServiceTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    base::Time time;
    ASSERT_TRUE(base::Time::FromString("2022-09-07 13:00", &time));
    clock_.SetNow(time);
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    hcsm_ = base::MakeRefCounted<HostContentSettingsMap>(&prefs_, false, true,
                                                         false, false);
    hcsm_->SetClockForTesting(&clock_);
    service_ = std::make_unique<UnusedSitePermissionsService>(hcsm_.get());
    service_->SetClockForTesting(&clock_);
  }

  void TearDown() override {
    service_->Shutdown();
    hcsm_->ShutdownOnUIThread();
    content::RenderViewHostTestHarness::TearDown();
  }

  base::SimpleTestClock* clock() { return &clock_; }

  UnusedSitePermissionsService* service() { return service_.get(); }

  HostContentSettingsMap* hcsm() { return hcsm_.get(); }

  base::Time GetLastVisitedDate(GURL url, ContentSettingsType type) {
    content_settings::SettingInfo info;
    hcsm()->GetWebsiteSetting(url, url, type, &info);
    return info.metadata.last_visited();
  }

  ContentSettingsForOneType GetRevokedUnusedPermissions(
      HostContentSettingsMap* hcsm) {
    ContentSettingsForOneType settings;
    hcsm->GetSettingsForOneType(
        ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS, &settings);

    return settings;
  }

  base::Value::List GetRevokedPermissionsForOneOrigin(
      HostContentSettingsMap* hcsm,
      const GURL& url) {
    base::Value setting_value(hcsm->GetWebsiteSetting(
        url, url, ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
        nullptr));

    base::Value::List permissions_list;
    if (!setting_value.is_dict() ||
        !setting_value.GetDict().FindList(permissions::kRevokedKey)) {
      return permissions_list;
    }

    permissions_list =
        std::move(*setting_value.GetDict().FindList(permissions::kRevokedKey));

    return permissions_list;
  }

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<UnusedSitePermissionsService> service_;
  scoped_refptr<HostContentSettingsMap> hcsm_;
  base::SimpleTestClock clock_;
};

TEST_F(UnusedSitePermissionsServiceTest, UnusedSitePermissionsServiceTest) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      content_settings::features::kSafetyCheckUnusedSitePermissions);

  const GURL url1("https://example1.com");
  const GURL url2("https://example2.com");
  const ContentSettingsType type1 = ContentSettingsType::GEOLOCATION;
  const ContentSettingsType type2 = ContentSettingsType::MEDIASTREAM_CAMERA;
  content_settings::ContentSettingConstraints constraint;
  constraint.set_track_last_visit_for_autoexpiration(true);

  const base::Time now = clock()->Now();
  const base::TimeDelta precision =
      content_settings::GetCoarseVisitedTimePrecision();

  // Add one setting for `url1` and two settings for `url2`.
  hcsm()->SetContentSettingDefaultScope(
      url1, url1, type1, ContentSetting::CONTENT_SETTING_ALLOW, constraint);
  hcsm()->SetContentSettingDefaultScope(
      url2, url2, type1, ContentSetting::CONTENT_SETTING_ALLOW, constraint);
  hcsm()->SetContentSettingDefaultScope(
      url2, url2, type2, ContentSetting::CONTENT_SETTING_ALLOW, constraint);
  service()->UpdateUnusedPermissionsForTesting();
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 0u);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // Travel through time for 20 days.
  clock()->Advance(base::Days(20));
  base::Time future = clock()->Now();

  // The old settings should now be tracked as unused.
  service()->UpdateUnusedPermissionsForTesting();
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 3u);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // Visit `url2` and check that the corresponding content setting got updated.
  UnusedSitePermissionsService::TabHelper::CreateForWebContents(web_contents(),
                                                                service());
  NavigateAndCommit(url2);
  EXPECT_LE(GetLastVisitedDate(url1, type1), now);
  EXPECT_GE(GetLastVisitedDate(url1, type1), now - precision);
  EXPECT_LE(GetLastVisitedDate(url2, type1), future);
  EXPECT_GE(GetLastVisitedDate(url2, type1), future - precision);
  EXPECT_LE(GetLastVisitedDate(url2, type2), future);
  EXPECT_GE(GetLastVisitedDate(url2, type2), future - precision);

  // Check that the service is only tracking one entry now.
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 1u);

  // Travel through time for 50 days to make permissions be revoked.
  clock()->Advance(base::Days(50));

  // Unused permissions should be auto revoked.
  service()->UpdateUnusedPermissionsForTesting();
  // url2 should be on tracked permissions list.
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 2u);
  std::string url2_str =
      ContentSettingsPattern::FromURLNoWildcard(url2).ToString();
  EXPECT_EQ(url2_str, service()
                          ->GetTrackedUnusedPermissionsForTesting()[0]
                          .source.primary_pattern.ToString());
  EXPECT_EQ(url2_str, service()
                          ->GetTrackedUnusedPermissionsForTesting()[1]
                          .source.primary_pattern.ToString());
  // `url1` should be on revoked permissions list.
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 1u);
  std::string url1_str =
      ContentSettingsPattern::FromURLNoWildcard(url1).ToString();
  EXPECT_EQ(url1_str,
            GetRevokedUnusedPermissions(hcsm())[0].primary_pattern.ToString());
}

TEST_F(UnusedSitePermissionsServiceTest, TrackOnlySingleOriginTest) {
  const GURL url1("https://example1.com");
  const GURL url2("https://[*.]example2.com");
  const GURL url3("file:///foo/bar.txt");
  const ContentSettingsType type = ContentSettingsType::GEOLOCATION;
  content_settings::ContentSettingConstraints constraint;
  constraint.set_track_last_visit_for_autoexpiration(true);

  // Add one setting for all urls.
  hcsm()->SetContentSettingDefaultScope(
      url1, url1, type, ContentSetting::CONTENT_SETTING_ALLOW, constraint);
  hcsm()->SetContentSettingDefaultScope(
      url2, url2, type, ContentSetting::CONTENT_SETTING_ALLOW, constraint);
  hcsm()->SetContentSettingDefaultScope(
      url2, url3, type, ContentSetting::CONTENT_SETTING_ALLOW, constraint);
  service()->UpdateUnusedPermissionsForTesting();
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 0u);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // Travel through time for 20 days.
  clock()->Advance(base::Days(20));

  // Only `url1` should be tracked because it is the only single origin url.
  service()->UpdateUnusedPermissionsForTesting();
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 1u);
  auto tracked_origin = service()->GetTrackedUnusedPermissionsForTesting()[0];
  EXPECT_EQ(GURL(tracked_origin.source.primary_pattern.ToString()), url1);
}

TEST_F(UnusedSitePermissionsServiceTest, TrackUnusedButDontRevoke) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      content_settings::features::kSafetyCheckUnusedSitePermissions);

  const GURL url("https://example1.com");
  content_settings::ContentSettingConstraints constraint;
  constraint.set_track_last_visit_for_autoexpiration(true);

  // Grant GEOLOCATION permission for the url.
  hcsm()->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::GEOLOCATION,
      ContentSetting::CONTENT_SETTING_BLOCK, constraint);

  // Travel through time for 20 days.
  clock()->Advance(base::Days(20));

  // GEOLOCATION permission should be on the tracked unused site permissions
  // list as it is denied 20 days before. The permission is not suitable for
  // revocation and this test verifies that RevokeUnusedPermissions() does not
  // enter infinite loop in such case.
  service()->UpdateUnusedPermissionsForTesting();
  auto unused_permissions = service()->GetTrackedUnusedPermissionsForTesting();
  ASSERT_EQ(unused_permissions.size(), 1u);
  EXPECT_EQ(unused_permissions[0].type, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), url).size(), 0u);
}

TEST_F(UnusedSitePermissionsServiceTest, SecondaryPatternAlwaysWildcard) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      content_settings::features::kSafetyCheckUnusedSitePermissions);

  const ContentSettingsType types[] = {
      ContentSettingsType::GEOLOCATION,
      ContentSettingsType::AUTOMATIC_DOWNLOADS};
  content_settings::ContentSettingConstraints constraint;
  constraint.set_track_last_visit_for_autoexpiration(true);

  // Test combinations of a single origin |primary_pattern| and different
  // |secondary_pattern|s: equal to primary pattern, different single origin
  // pattern, with domain with wildcard, wildcard.
  for (const auto type : types) {
    hcsm()->SetContentSettingDefaultScope(
        GURL("https://example1.com"), GURL("https://example1.com"), type,
        ContentSetting::CONTENT_SETTING_ALLOW, constraint);
    hcsm()->SetContentSettingDefaultScope(
        GURL("https://example2.com"), GURL("https://example3.com"), type,
        ContentSetting::CONTENT_SETTING_ALLOW, constraint);
    hcsm()->SetContentSettingDefaultScope(
        GURL("https://example3.com"), GURL("https://[*.]example1.com"), type,
        ContentSetting::CONTENT_SETTING_ALLOW, constraint);
    hcsm()->SetContentSettingDefaultScope(
        GURL("https://example4.com"), GURL("*"), type,
        ContentSetting::CONTENT_SETTING_ALLOW, constraint);
  }

  service()->UpdateUnusedPermissionsForTesting();
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // Travel through time for 70 days so that permissions are revoked.
  clock()->Advance(base::Days(70));
  service()->UpdateUnusedPermissionsForTesting();

  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 4u);
  for (auto unused_permission : GetRevokedUnusedPermissions(hcsm())) {
    EXPECT_EQ(unused_permission.secondary_pattern,
              ContentSettingsPattern::Wildcard());
  }
}

TEST_F(UnusedSitePermissionsServiceTest, MultipleRevocationsForSameOrigin) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      content_settings::features::kSafetyCheckUnusedSitePermissions);

  const GURL url("https://example1.com");
  content_settings::ContentSettingConstraints constraint;
  constraint.set_track_last_visit_for_autoexpiration(true);

  // Grant GEOLOCATION permission for the url.
  hcsm()->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::GEOLOCATION,
      ContentSetting::CONTENT_SETTING_ALLOW, constraint);
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 0u);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // Travel through time for 20 days.
  clock()->Advance(base::Days(20));

  // Grant MEDIASTREAM_CAMERA permission for the url.
  hcsm()->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::MEDIASTREAM_CAMERA,
      ContentSetting::CONTENT_SETTING_ALLOW, constraint);

  // GEOLOCATION permission should be on the tracked unused site permissions
  // list as it is granted 20 days before. MEDIASTREAM_CAMERA permission should
  // not be tracked as it is just granted.
  service()->UpdateUnusedPermissionsForTesting();
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 1u);
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting()[0].type,
            ContentSettingsType::GEOLOCATION);

  // Travel through time for 50 days.
  clock()->Advance(base::Days(50));

  // GEOLOCATION permission should be on the revoked permissions list as it is
  // granted 70 days before. MEDIASTREAM_CAMERA permission should be on the
  // recently unused permissions list as it is granted 50 days before.
  service()->UpdateUnusedPermissionsForTesting();
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), url).size(), 1u);
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), url)[0].GetInt(),
            static_cast<int32_t>(ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 1u);
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting()[0].type,
            ContentSettingsType::MEDIASTREAM_CAMERA);
}

TEST_F(UnusedSitePermissionsServiceTest, ClearRevokedPermissionsListAfter30d) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      content_settings::features::kSafetyCheckUnusedSitePermissions);

  const GURL url("https://example1.com");
  content_settings::ContentSettingConstraints constraint;
  constraint.set_track_last_visit_for_autoexpiration(true);

  hcsm()->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::MEDIASTREAM_CAMERA,
      ContentSetting::CONTENT_SETTING_ALLOW, constraint);
  hcsm()->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::GEOLOCATION,
      ContentSetting::CONTENT_SETTING_ALLOW, constraint);

  // Travel through time for 70 days.
  clock()->Advance(base::Days(70));

  // Both GEOLOCATION and MEDIASTREAM_CAMERA permissions should be on the
  // revoked permissions list as they are granted more than 60 days before.
  service()->UpdateUnusedPermissionsForTesting();
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), url).size(), 2u);
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), url)[0].GetInt(),
            static_cast<int32_t>(ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), url)[1].GetInt(),
            static_cast<int32_t>(ContentSettingsType::MEDIASTREAM_CAMERA));

  // Travel through time for 30 days.
  clock()->Advance(base::Days(30));

  // No permission should be on the revoked permissions list as they are revoked
  // more than 30 days before.
  service()->UpdateUnusedPermissionsForTesting();
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), url).size(), 0u);
}

TEST_F(UnusedSitePermissionsServiceTest, RegrantPermissionsForOrigin) {
  const std::string url1 = "https://example1.com:443";
  const std::string url2 = "https://example2.com:443";
  const ContentSettingsType type = ContentSettingsType::GEOLOCATION;

  base::Value::Dict dict = base::Value::Dict();
  base::Value::List permission_type_list = base::Value::List();
  permission_type_list.Append(static_cast<int32_t>(type));
  dict.Set(kRevokedKey, base::Value::List(std::move(permission_type_list)));

  // Add `url1` and `url2` to revoked permissions list.
  hcsm()->SetWebsiteSettingDefaultScope(
      GURL(url1), GURL(url1),
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      base::Value(dict.Clone()));
  hcsm()->SetWebsiteSettingDefaultScope(
      GURL(url2), GURL(url2),
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      base::Value(dict.Clone()));

  // Check there are 2 origin in revoked permissions list.
  ContentSettingsForOneType revoked_permissions_list;
  hcsm()->GetSettingsForOneType(
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      &revoked_permissions_list);
  EXPECT_EQ(2U, revoked_permissions_list.size());

  // Allow the permission for `url1` again
  service()->RegrantPermissionsForOrigin(url::Origin::Create(GURL(url1)));

  // Check there is only `url2` in revoked permissions list.
  hcsm()->GetSettingsForOneType(
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      &revoked_permissions_list);
  EXPECT_EQ(1U, revoked_permissions_list.size());

  // Check if the permissions of `url1` is regranted.
  EXPECT_EQ(ContentSetting::CONTENT_SETTING_ALLOW,
            hcsm()->GetContentSetting(GURL(url1), GURL(url1), type));

  // Undoing the changes should add `url1` back to the list of revoked
  // permissions and reset its permissions.
  service()->UndoRegrantPermissionsForOrigin({type}, absl::nullopt,
                                             url::Origin::Create(GURL(url1)));

  hcsm()->GetSettingsForOneType(
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      &revoked_permissions_list);
  EXPECT_EQ(2U, revoked_permissions_list.size());
  EXPECT_EQ(ContentSetting::CONTENT_SETTING_ASK,
            hcsm()->GetContentSetting(GURL(url1), GURL(url1), type));
}

TEST_F(UnusedSitePermissionsServiceTest, RegrantPreventsAutorevoke) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      content_settings::features::kSafetyCheckUnusedSitePermissions);

  const GURL url1 = GURL("https://example1.com:443");
  content_settings::ContentSettingConstraints constraint;
  constraint.set_track_last_visit_for_autoexpiration(true);

  hcsm()->SetContentSettingDefaultScope(
      url1, url1, ContentSettingsType::GEOLOCATION,
      ContentSetting::CONTENT_SETTING_ALLOW, constraint);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // Travel 70 days through time so that the granted permission is revoked.
  clock()->Advance(base::Days(70));
  service()->UpdateUnusedPermissionsForTesting();
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 1u);

  // After regranting permissions they are not revoked again even after >60 days
  // pass.
  service()->RegrantPermissionsForOrigin(url::Origin::Create(url1));
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  clock()->Advance(base::Days(70));
  service()->UpdateUnusedPermissionsForTesting();
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);
}

TEST_F(UnusedSitePermissionsServiceTest, UndoRegrantPermissionsForOrigin) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      content_settings::features::kSafetyCheckUnusedSitePermissions);

  const GURL url1 = GURL("https://example1.com:443");
  const ContentSettingsType type = ContentSettingsType::GEOLOCATION;
  content_settings::ContentSettingConstraints constraint;
  constraint.set_track_last_visit_for_autoexpiration(true);

  hcsm()->SetContentSettingDefaultScope(
      url1, url1, type, ContentSetting::CONTENT_SETTING_ALLOW, constraint);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // Travel 70 days through time so that the granted permission is revoked.
  clock()->Advance(base::Days(70));
  service()->UpdateUnusedPermissionsForTesting();
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 1u);
  const ContentSettingPatternSource revoked_permission =
      GetRevokedUnusedPermissions(hcsm())[0];

  // Permission remains revoked after regrant and undo.
  content_settings::ContentSettingConstraints expiration_constraint(
      revoked_permission.metadata.expiration() -
      revoked_permission.metadata.lifetime());
  expiration_constraint.set_lifetime(revoked_permission.metadata.lifetime());
  service()->RegrantPermissionsForOrigin(url::Origin::Create(url1));
  service()->UndoRegrantPermissionsForOrigin({type}, expiration_constraint,
                                             url::Origin::Create(url1));
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 1u);

  // Revoked permission is cleaned up after >30 days.
  clock()->Advance(base::Days(40));
  service()->UpdateUnusedPermissionsForTesting();
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // If that permission is granted again, it will still be autorevoked.
  hcsm()->SetContentSettingDefaultScope(
      url1, url1, type, ContentSetting::CONTENT_SETTING_ALLOW, constraint);
  clock()->Advance(base::Days(70));
  service()->UpdateUnusedPermissionsForTesting();
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 1u);
}

TEST_F(UnusedSitePermissionsServiceTest, NotRevokeNotificationPermission) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      content_settings::features::kSafetyCheckUnusedSitePermissions);

  const GURL url("https://example1.com");
  content_settings::ContentSettingConstraints constraint;
  constraint.set_track_last_visit_for_autoexpiration(true);

  // Grant GEOLOCATION and NOTIFICATION permission for the url.
  hcsm()->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::GEOLOCATION,
      ContentSetting::CONTENT_SETTING_ALLOW, constraint);
  hcsm()->SetContentSettingDefaultScope(url, url,
                                        ContentSettingsType::NOTIFICATIONS,
                                        ContentSetting::CONTENT_SETTING_ALLOW);
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 0u);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // Travel through time for 70 days.
  clock()->Advance(base::Days(70));

  // GEOLOCATION permission should be on the revoked permissions list, but
  // NOTIFICATION permissions should not be as notification permissions are out
  // of scope.
  service()->UpdateUnusedPermissionsForTesting();
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), url).size(), 1u);
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), url)[0].GetInt(),
            static_cast<int32_t>(ContentSettingsType::GEOLOCATION));

  // Clearing revoked permissions list should delete unused GEOLOCATION from it
  // but leave used NOTIFICATION permissions intact.
  service()->ClearRevokedPermissionsList();
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), url).size(), 0u);
  EXPECT_EQ(hcsm()->GetContentSetting(GURL(url), GURL(url),
                                      ContentSettingsType::GEOLOCATION),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_EQ(hcsm()->GetContentSetting(GURL(url), GURL(url),
                                      ContentSettingsType::NOTIFICATIONS),
            ContentSetting::CONTENT_SETTING_ALLOW);
}

TEST_F(UnusedSitePermissionsServiceTest, ClearRevokedPermissionsList) {
  const std::string url1 = "https://example1.com:443";
  const std::string url2 = "https://example2.com:443";
  const ContentSettingsType type = ContentSettingsType::GEOLOCATION;

  base::Value::Dict dict = base::Value::Dict();
  base::Value::List permission_type_list = base::Value::List();
  permission_type_list.Append(static_cast<int32_t>(type));
  dict.Set(kRevokedKey, base::Value::List(std::move(permission_type_list)));

  // Add `url1` and `url2` to revoked permissions list.
  hcsm()->SetWebsiteSettingDefaultScope(
      GURL(url1), GURL(url1),
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      base::Value(dict.Clone()));
  hcsm()->SetWebsiteSettingDefaultScope(
      GURL(url2), GURL(url2),
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      base::Value(dict.Clone()));

  // Check there are 2 origins in the revoked permissions list.
  ContentSettingsForOneType revoked_permissions_list;
  hcsm()->GetSettingsForOneType(
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      &revoked_permissions_list);
  EXPECT_EQ(2U, revoked_permissions_list.size());

  service()->ClearRevokedPermissionsList();

  // Revoked permissions list should be empty.
  hcsm()->GetSettingsForOneType(
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      &revoked_permissions_list);
  EXPECT_EQ(revoked_permissions_list.size(), 0U);
}

TEST_F(UnusedSitePermissionsServiceTest, GetDaysSinceRevocation) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      content_settings::features::kSafetyCheckUnusedSitePermissions);

  const GURL url = GURL("https://example1.com:443");
  const ContentSettingsType type = ContentSettingsType::GEOLOCATION;
  content_settings::ContentSettingConstraints constraint;
  constraint.set_track_last_visit_for_autoexpiration(true);

  absl::optional<uint32_t> days_since_revocation;

  // Permission has not yet been revoked, so shouldn't return a number of days
  // since revocation.
  days_since_revocation = UnusedSitePermissionsService::GetDaysSinceRevocation(
      url, ContentSettingsType::GEOLOCATION, clock()->Now(), hcsm());
  ASSERT_FALSE(days_since_revocation.has_value());

  hcsm()->SetContentSettingDefaultScope(
      url, url, type, ContentSetting::CONTENT_SETTING_ALLOW, constraint);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // Travel 70 days through time so that the granted permission is revoked.
  clock()->Advance(base::Days(70));
  service()->UpdateUnusedPermissionsForTesting();
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 1u);

  days_since_revocation = UnusedSitePermissionsService::GetDaysSinceRevocation(
      url, ContentSettingsType::GEOLOCATION, clock()->Now(), hcsm());
  ASSERT_TRUE(days_since_revocation.has_value());
  EXPECT_EQ(days_since_revocation.value(), 0u);

  // Forward the clock for five days, which would be the number of days since
  // revocation.
  clock()->Advance(base::Days(5));

  days_since_revocation = UnusedSitePermissionsService::GetDaysSinceRevocation(
      url, ContentSettingsType::GEOLOCATION, clock()->Now(), hcsm());
  ASSERT_TRUE(days_since_revocation.has_value());
  EXPECT_EQ(days_since_revocation.value(), 5u);

  // Revoked permission is cleaned up after >30 days.
  clock()->Advance(base::Days(40));
  days_since_revocation = UnusedSitePermissionsService::GetDaysSinceRevocation(
      url, ContentSettingsType::GEOLOCATION, clock()->Now(), hcsm());
  ASSERT_FALSE(days_since_revocation.has_value());
}

TEST_F(UnusedSitePermissionsServiceTest, RecordRegrantMetricForAllowAgain) {
  const std::string url = "https://example.com:443";
  base::Value::Dict dict = base::Value::Dict();
  base::Value::List permission_type_list = base::Value::List();
  permission_type_list.Append(
      static_cast<int32_t>(ContentSettingsType::GEOLOCATION));
  dict.Set(kRevokedKey, base::Value::List(std::move(permission_type_list)));

  auto cleanUpThreshold =
      content_settings::features::
          kSafetyCheckUnusedSitePermissionsRevocationCleanUpThreshold.Get();
  content_settings::ContentSettingConstraints constraint(clock()->Now());
  constraint.set_lifetime(cleanUpThreshold);

  // Add `url` to revoked permissions list.
  hcsm()->SetWebsiteSettingDefaultScope(
      GURL(url), GURL(url),
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      base::Value(dict.Clone()), constraint);

  // Assert there is 1 origin in revoked permissions list.
  ContentSettingsForOneType revoked_permissions_list;
  hcsm()->GetSettingsForOneType(
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      &revoked_permissions_list);
  ASSERT_EQ(1U, revoked_permissions_list.size());

  // Advance 14 days; this will be the expected histogram sample.
  clock()->Advance(base::Days(14));
  base::HistogramTester histogram_tester;

  // Allow the permission for `url` again
  service()->RegrantPermissionsForOrigin(url::Origin::Create(GURL(url)));

  // Only a single entry should be recorded in the histogram.
  const std::vector<base::Bucket> buckets = histogram_tester.GetAllSamples(
      "Settings.SafetyCheck.UnusedSitePermissionsAllowAgainDays");
  EXPECT_EQ(1U, buckets.size());
  // The recorded metric should be the elapsed days since the revocation.
  histogram_tester.ExpectUniqueSample(
      "Settings.SafetyCheck.UnusedSitePermissionsAllowAgainDays", 14, 1);
}

TEST_F(UnusedSitePermissionsServiceTest,
       RemoveSiteFromRevokedPermissionsListOnPermissionChange) {
  const GURL url1 = GURL("https://example1.com:443");
  const GURL url2 = GURL("https://example2.com:443");
  const ContentSettingsType type = ContentSettingsType::GEOLOCATION;

  base::Value::Dict dict = base::Value::Dict();
  base::Value::List permission_type_list = base::Value::List();
  permission_type_list.Append(static_cast<int32_t>(type));
  dict.Set(kRevokedKey, base::Value::List(std::move(permission_type_list)));

  // Add url1 and url2 to revoked permissions list.
  hcsm()->SetWebsiteSettingDefaultScope(
      url1, url1, ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      base::Value(dict.Clone()));
  hcsm()->SetWebsiteSettingDefaultScope(
      url2, url2, ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      base::Value(dict.Clone()));

  ContentSettingsForOneType revoked_permissions_list;
  hcsm()->GetSettingsForOneType(
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      &revoked_permissions_list);

  EXPECT_EQ(2U, revoked_permissions_list.size());

  // For a site where permissions have been revoked, granting a revoked
  // permission again will remove the site from the list.
  hcsm()->SetContentSettingDefaultScope(
      url1, GURL(), ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);
  // Check there is only url2 in revoked permissions list.
  hcsm()->GetSettingsForOneType(
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      &revoked_permissions_list);
  EXPECT_EQ(1U, revoked_permissions_list.size());
}

}  // namespace permissions
