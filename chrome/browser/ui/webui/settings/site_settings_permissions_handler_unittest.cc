// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ctime>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/browser/ui/webui/settings/site_settings_permissions_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

constexpr char kRevokedKey[] = "revoked";

class SiteSettingsPermissionsHandlerTest : public testing::Test {
 public:
  SiteSettingsPermissionsHandlerTest() = default;

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
  }

  void TearDown() override {
    if (profile_) {
      auto* partition = profile_->GetDefaultStoragePartition();
      if (partition) {
        partition->WaitForDeletionTasksForTesting();
      }
    }
  }

  TestingProfile* profile() { return profile_.get(); }
  content::TestWebUI* web_ui() { return &web_ui_; }
  SiteSettingsPermissionsHandler* handler() { return handler_.get(); }
  HostContentSettingsMap* hcsm() { return hcsm_.get(); }
  base::SimpleTestClock* clock() { return &clock_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<SiteSettingsPermissionsHandler> handler_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebUI web_ui_;
  scoped_refptr<HostContentSettingsMap> hcsm_;
  base::SimpleTestClock clock_;
};

TEST_F(SiteSettingsPermissionsHandlerTest, PopulateUnusedSitePermissionsData) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      content_settings::features::kSafetyCheckUnusedSitePermissions);

  const std::string url1 = "https://example1.com";
  const std::string url2 = "https://example2.com";

  base::Value::Dict dict = base::Value::Dict();
  base::Value::List permission_type_list = base::Value::List();
  permission_type_list.Append(
      static_cast<int32_t>(ContentSettingsType::GEOLOCATION));
  dict.Set(kRevokedKey, base::Value::List(std::move(permission_type_list)));

  // Add url1 to rovoked permissions list.
  hcsm()->SetWebsiteSettingDefaultScope(
      GURL(url1), GURL(url1),
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      base::Value(std::move(dict)));

  // Add GEOLOCATION setting for url2 but do not add to revoked list.
  const content_settings::ContentSettingConstraints constraint{
      .track_last_visit_for_autoexpiration = true};
  hcsm()->SetContentSettingDefaultScope(
      GURL(url2), GURL(url2), ContentSettingsType::GEOLOCATION,
      ContentSetting::CONTENT_SETTING_ALLOW, constraint);

  // Only url1 should be in the revoked permissions list, as permissions of
  // url2 is not revoked.
  const auto& revoked_permissions =
      handler()->PopulateUnusedSitePermissionsData();
  EXPECT_EQ(revoked_permissions.size(), 1UL);
  EXPECT_EQ(url1,
            *revoked_permissions[0].FindStringKey(site_settings::kOrigin));
}
