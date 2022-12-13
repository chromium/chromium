// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/unused_site_permissions_service.h"
#include <ctime>
#include <list>
#include <memory>
#include "base/memory/scoped_refptr.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"

constexpr char kRevokedKey[] = "revoked";

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
    return info.metadata.last_visited;
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

    base::Value::List permissions;
    if (!setting_value.is_dict() ||
        !setting_value.GetDict().FindList(kRevokedKey))
      return permissions;

    base::Value::List permissions_list =
        std::move(*setting_value.GetDict().FindList(kRevokedKey));

    return permissions_list;
  }

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<UnusedSitePermissionsService> service_;
  scoped_refptr<HostContentSettingsMap> hcsm_;
  base::SimpleTestClock clock_;
};

TEST_F(UnusedSitePermissionsServiceTest, UnusedSitePermissionsServiceTest) {
  const GURL url1("https://example1.com");
  const GURL url2("https://example2.com");
  const ContentSettingsType type1 = ContentSettingsType::GEOLOCATION;
  const ContentSettingsType type2 = ContentSettingsType::MEDIASTREAM_CAMERA;
  const content_settings::ContentSettingConstraints constraint{
      .track_last_visit_for_autoexpiration = true};

  const base::Time now = clock()->Now();
  const base::TimeDelta precision =
      content_settings::GetCoarseVisitedTimePrecision();

  // Add one setting for url1 and two settings for url2.
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

  // Visit url2 and check that the corresponding content setting got updated.
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
  // url1 should be on revoked permissions list.
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
  const content_settings::ContentSettingConstraints constraint{
      .track_last_visit_for_autoexpiration = true};

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

  // Only url1 should be tracked because it is the only single origin url.
  service()->UpdateUnusedPermissionsForTesting();
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 1u);
  auto tracked_origin = service()->GetTrackedUnusedPermissionsForTesting()[0];
  EXPECT_EQ(GURL(tracked_origin.source.primary_pattern.ToString()), url1);
}

TEST_F(UnusedSitePermissionsServiceTest, MultipleRevocationsForSameOrigin) {
  const GURL url("https://example1.com");
  const content_settings::ContentSettingConstraints constraint{
      .track_last_visit_for_autoexpiration = true};

  // Grant GEOLOCATION permission for the url.
  hcsm()->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::GEOLOCATION,
      ContentSetting::CONTENT_SETTING_ALLOW, constraint);
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 0u);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // Travel through time for 50 days.
  clock()->Advance(base::Days(50));

  // Grant MEDIASTREAM_CAMERA permission for the url.
  hcsm()->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::MEDIASTREAM_CAMERA,
      ContentSetting::CONTENT_SETTING_ALLOW, constraint);

  // Travel through time for 20 days.
  clock()->Advance(base::Days(20));

  // GEOLOCATION permission should be on the revoked permissions list.
  // MEDIASTREAM_CAMERA permissions should be on the recently unused permissions
  // list.
  service()->UpdateUnusedPermissionsForTesting();
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), url).size(), 1u);
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), url)[0].GetInt(),
            static_cast<int32_t>(ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 1u);
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting()[0].type,
            ContentSettingsType::MEDIASTREAM_CAMERA);

  // Travel through time for 70 days.
  clock()->Advance(base::Days(70));

  // Both GEOLOCATION and MEDIASTREAM_CAMERA permissions should be on the
  // revoked permissions list.
  service()->UpdateUnusedPermissionsForTesting();
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), url).size(), 2u);
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), url)[0].GetInt(),
            static_cast<int32_t>(ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), url)[1].GetInt(),
            static_cast<int32_t>(ContentSettingsType::MEDIASTREAM_CAMERA));
}

}  // namespace permissions
