// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/unused_site_permissions_service.h"
#include <ctime>
#include <memory>
#include "base/memory/scoped_refptr.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

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
                                                         false);
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
  const base::TimeDelta precision = content_settings::GetCoarseTimePrecision();

  // Add one setting for url1 and two settings for url2.
  hcsm()->SetContentSettingDefaultScope(
      url1, url1, type1, ContentSetting::CONTENT_SETTING_ALLOW, constraint);
  hcsm()->SetContentSettingDefaultScope(
      url2, url2, type1, ContentSetting::CONTENT_SETTING_ALLOW, constraint);
  hcsm()->SetContentSettingDefaultScope(
      url2, url2, type2, ContentSetting::CONTENT_SETTING_ALLOW, constraint);
  service()->UpdateUnusedPermissionsForTesting();
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 0u);

  // Travel through time for 20 days.
  clock()->Advance(base::Days(20));
  base::Time future = clock()->Now();

  // The old settings should now be tracked as unused.
  service()->UpdateUnusedPermissionsForTesting();
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 3u);

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
}

}  // namespace permissions
