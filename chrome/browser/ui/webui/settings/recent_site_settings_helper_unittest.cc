// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/recent_site_settings_helper.h"

#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace site_settings {

namespace {

ContentSetting kBlocked = ContentSetting::CONTENT_SETTING_BLOCK;
ContentSetting kAllowed = ContentSetting::CONTENT_SETTING_ALLOW;
ContentSetting kDefault = ContentSetting::CONTENT_SETTING_DEFAULT;
SiteSettingSource kEmbargo = site_settings::SiteSettingSource::kEmbargo;
SiteSettingSource kPreference = site_settings::SiteSettingSource::kPreference;
ContentSettingsType kNotifications = ContentSettingsType::NOTIFICATIONS;
ContentSettingsType kImages = ContentSettingsType::IMAGES;
ContentSettingsType kPopups = ContentSettingsType::POPUPS;
ContentSettingsType kLocation = ContentSettingsType::GEOLOCATION;

base::Time GetSettingLastModifiedDate(HostContentSettingsMap* map,
                                      GURL primary_url,
                                      GURL secondary_url,
                                      ContentSettingsType type) {
  content_settings::SettingInfo info;
  map->GetWebsiteSetting(primary_url, secondary_url, type, &info);
  return info.metadata.last_modified();
}

}  // namespace

class RecentSiteSettingsHelperTest : public testing::Test {
 protected:
  TestingProfile* profile() { return &profile_; }
  TestingProfile* incognito_profile() { return incognito_profile_; }

  base::SimpleTestClock* clock() { return &clock_; }

  void CreateIncognitoProfile() {
    incognito_profile_ = TestingProfile::Builder().BuildIncognito(profile());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::SimpleTestClock clock_;
  TestingProfile profile_;
  raw_ptr<TestingProfile> incognito_profile_;
};

TEST_F(RecentSiteSettingsHelperTest, IncognitoPermissionTimestamps) {
  // Confirm our expectation that content settings prefs copied to the incognito
  // profile have a timestamp of base::Time().
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  const GURL url("http://example.com");
  map->SetContentSettingDefaultScope(url, url, kNotifications, kBlocked);

  CreateIncognitoProfile();
  HostContentSettingsMap* incognito_map =
      HostContentSettingsMapFactory::GetForProfile(incognito_profile());
  EXPECT_NE(base::Time(),
            GetSettingLastModifiedDate(map, url, url, kNotifications));
  EXPECT_EQ(base::Time(), GetSettingLastModifiedDate(incognito_map, url, url,
                                                     kNotifications));
}

TEST_F(RecentSiteSettingsHelperTest, CheckRecentSitePermissions) {
  const GURL url1("https://example.com");
  const GURL url2("http://example.com");
  std::vector<ContentSettingsType> content_types = {kNotifications, kImages,
                                                    kPopups, kLocation};
  clock()->SetNow(base::Time::Now());

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetClockForTesting(clock());

  auto* auto_blocker =
      PermissionDecisionAutoBlockerFactory::GetForProfile(profile());
  auto_blocker->SetClockForTesting(clock());

  // Check that with default permissions nothing is returned.
  auto recent_permissions =
      GetRecentSitePermissions(profile(), content_types, 10);
  EXPECT_EQ(0UL, recent_permissions.size());

  // Add two permissions for different urls via different mechanisms and
  // confirm they're returned correctly. Ensure that simply creating an
  // ingocnito profile does not change recent permissions.
  for (int i = 0; i < 3; ++i) {
    auto_blocker->RecordDismissAndEmbargo(url1, kNotifications, false);
  }

  clock()->Advance(base::Hours(2));
  map->SetContentSettingDefaultScope(url2, url2, kImages, kAllowed);
  CreateIncognitoProfile();

  recent_permissions = GetRecentSitePermissions(profile(), content_types, 10);
  {
    EXPECT_EQ(2UL, recent_permissions.size());
    EXPECT_EQ(1UL, recent_permissions[0].settings.size());
    EXPECT_EQ(1UL, recent_permissions[1].settings.size());

    EXPECT_EQ(url1.spec(), recent_permissions[1].origin);
    EXPECT_EQ(url2.spec(), recent_permissions[0].origin);

    auto url1_permissions = recent_permissions[1].settings;
    auto url2_permissions = recent_permissions[0].settings;

    EXPECT_EQ(kNotifications, url1_permissions[0].content_type);
    EXPECT_EQ(kBlocked, url1_permissions[0].content_setting);
    EXPECT_EQ(kEmbargo, url1_permissions[0].setting_source);

    EXPECT_EQ(kImages, url2_permissions[0].content_type);
    EXPECT_EQ(kAllowed, url2_permissions[0].content_setting);
    EXPECT_EQ(kPreference, url2_permissions[0].setting_source);
  }

  // Ensure incognito generated permissions are separated correctly.
  clock()->Advance(base::Hours(1));
  HostContentSettingsMap* incognito_map =
      HostContentSettingsMapFactory::GetForProfile(incognito_profile());
  incognito_map->SetClockForTesting(clock());
  incognito_map->SetContentSettingDefaultScope(url1, url1, kImages, kAllowed);

  clock()->Advance(base::Hours(1));
  permissions::PermissionDecisionAutoBlocker* incognito_auto_blocker =
      PermissionDecisionAutoBlockerFactory::GetForProfile(incognito_profile());
  incognito_auto_blocker->SetClockForTesting(clock());
  for (int i = 0; i < 3; ++i) {
    incognito_auto_blocker->RecordDismissAndEmbargo(url1, kNotifications,
                                                    false);
  }

  recent_permissions = GetRecentSitePermissions(profile(), content_types, 10);
  {
    EXPECT_EQ(3UL, recent_permissions.size());
    EXPECT_EQ(2UL, recent_permissions[0].settings.size());
    EXPECT_EQ(1UL, recent_permissions[1].settings.size());
    EXPECT_EQ(1UL, recent_permissions[2].settings.size());

    EXPECT_EQ(url1.spec(), recent_permissions[2].origin);
    EXPECT_EQ(url2.spec(), recent_permissions[1].origin);
    EXPECT_EQ(url1.spec(), recent_permissions[0].origin);

    EXPECT_EQ("example.com", recent_permissions[0].display_name);
    EXPECT_EQ("example.com", recent_permissions[1].display_name);
    EXPECT_EQ("example.com", recent_permissions[2].display_name);

    EXPECT_TRUE(recent_permissions[0].incognito);
    EXPECT_FALSE(recent_permissions[1].incognito);
    EXPECT_FALSE(recent_permissions[2].incognito);

    auto incognito_url1_permissions = recent_permissions[0].settings;

    EXPECT_EQ(kNotifications, incognito_url1_permissions[0].content_type);
    EXPECT_EQ(kBlocked, incognito_url1_permissions[0].content_setting);
    EXPECT_EQ(kEmbargo, incognito_url1_permissions[0].setting_source);

    EXPECT_EQ(kImages, incognito_url1_permissions[1].content_type);
    EXPECT_EQ(kAllowed, incognito_url1_permissions[1].content_setting);
    EXPECT_EQ(kPreference, incognito_url1_permissions[1].setting_source);
  }

  // Test additional permissions are correctly compacted down to the provided
  // maximum number of sources and that order of origins is based on the
  // most recent permission for that source.
  const GURL url3("https://example.com:8443");
  clock()->Advance(base::Hours(1));
  map->SetContentSettingDefaultScope(url1, url1, kImages, kBlocked);
  clock()->Advance(base::Hours(1));
  for (int i = 0; i < 4; ++i) {
    auto_blocker->RecordIgnoreAndEmbargo(url3, kNotifications, false);
  }
  clock()->Advance(base::Hours(1));
  map->SetContentSettingDefaultScope(url2, url2, kPopups, kAllowed);
  clock()->Advance(base::Hours(1));
  map->SetContentSettingDefaultScope(url3, url3, kPopups, kBlocked);

  recent_permissions = GetRecentSitePermissions(profile(), content_types, 3);
  {
    EXPECT_EQ(3UL, recent_permissions.size());
    EXPECT_EQ(2UL, recent_permissions[0].settings.size());
    EXPECT_EQ(1UL, recent_permissions[1].settings.size());
    EXPECT_EQ(1UL, recent_permissions[2].settings.size());

    EXPECT_EQ(url3.spec(), recent_permissions[0].origin);
    EXPECT_EQ(url2.spec(), recent_permissions[1].origin);
    EXPECT_EQ(url1.spec(), recent_permissions[2].origin);

    // The oldest record will those for the inconito URL1, they should have
    // been removed.
    EXPECT_FALSE(recent_permissions[2].incognito);
    auto url1_permissions = recent_permissions[2].settings;
    auto url2_permissions = recent_permissions[1].settings;
    auto url3_permissions = recent_permissions[0].settings;

    EXPECT_EQ(kImages, url1_permissions[0].content_type);
    EXPECT_EQ(kBlocked, url1_permissions[0].content_setting);
    EXPECT_EQ(kPreference, url1_permissions[0].setting_source);

    EXPECT_EQ(kPopups, url2_permissions[0].content_type);
    EXPECT_EQ(kAllowed, url2_permissions[0].content_setting);
    EXPECT_EQ(kPreference, url2_permissions[0].setting_source);

    EXPECT_EQ(kNotifications, url3_permissions[0].content_type);
    EXPECT_EQ(kBlocked, url3_permissions[0].content_setting);
    EXPECT_EQ(kEmbargo, url3_permissions[0].setting_source);

    EXPECT_EQ(kPopups, url3_permissions[1].content_type);
    EXPECT_EQ(kBlocked, url3_permissions[1].content_setting);
    EXPECT_EQ(kPreference, url3_permissions[1].setting_source);
  }

  // Assign a new permission to a previously recorded site whose other
  // permissions are too old and ensure only the recent permission is returned.
  clock()->Advance(base::Hours(1));
  incognito_map->SetContentSettingDefaultScope(url1, url1, kPopups, kBlocked);
  recent_permissions = GetRecentSitePermissions(profile(), content_types, 3);
  {
    EXPECT_EQ(3UL, recent_permissions.size());
    EXPECT_EQ(1UL, recent_permissions[0].settings.size());
    EXPECT_EQ(url1.spec(), recent_permissions[0].origin);
    EXPECT_TRUE(recent_permissions[0].incognito);

    EXPECT_EQ(kPopups, recent_permissions[0].settings[0].content_type);
    EXPECT_EQ(kBlocked, recent_permissions[0].settings[0].content_setting);
    EXPECT_EQ(kPreference, recent_permissions[0].settings[0].setting_source);
  }

  // Reset a changed permission to default and confirm it does not appear as a
  // recent permission change.
  clock()->Advance(base::Hours(1));
  map->SetContentSettingDefaultScope(url3, url3, kPopups, kDefault);
  recent_permissions = GetRecentSitePermissions(profile(), content_types, 3);
  {
    EXPECT_EQ(3UL, recent_permissions.size());
    EXPECT_EQ(1UL, recent_permissions[0].settings.size());
    EXPECT_NE(url3.spec(), recent_permissions[0].origin);
  }

  // Expire all existing embargoes, then re-embargo a  site with a single
  // dismissal and confirm this is recorded as a new permission change. Also
  // confirm that sources with no permission changes associated are not
  // considered. I.e. url3 now has an expired embargo and a default setting. It
  // should not be considered and should allow the images setting for url1 to
  // be included as a recent change.
  clock()->Advance(base::Days(7));
  auto_blocker->RecordDismissAndEmbargo(url1, kNotifications, false);
  recent_permissions = GetRecentSitePermissions(profile(), content_types, 3);
  {
    EXPECT_EQ(3UL, recent_permissions.size());
    EXPECT_EQ(2UL, recent_permissions[0].settings.size());
    EXPECT_EQ(2UL, recent_permissions[1].settings.size());
    EXPECT_EQ(2UL, recent_permissions[2].settings.size());

    EXPECT_EQ(url1.spec(), recent_permissions[0].origin);
    EXPECT_FALSE(recent_permissions[0].incognito);
    auto url1_permissions = recent_permissions[0].settings;

    EXPECT_EQ(kNotifications, url1_permissions[0].content_type);
    EXPECT_EQ(kBlocked, url1_permissions[0].content_setting);
    EXPECT_EQ(kEmbargo, url1_permissions[0].setting_source);

    EXPECT_EQ(kImages, url1_permissions[1].content_type);
    EXPECT_EQ(kBlocked, url1_permissions[1].content_setting);
    EXPECT_EQ(kPreference, url1_permissions[1].setting_source);
  }

  // Confirm that powerful permissions are listed first, and that other
  // permissions remain sorted by time.
  clock()->Advance(base::Hours(1));
  map->SetContentSettingDefaultScope(url1, url1, kPopups, kBlocked);
  clock()->Advance(base::Hours(1));
  map->SetContentSettingDefaultScope(url1, url1, kLocation, kAllowed);
  clock()->Advance(base::Hours(1));
  map->SetContentSettingDefaultScope(url1, url1, kImages, kAllowed);

  recent_permissions = GetRecentSitePermissions(profile(), content_types, 3);
  {
    EXPECT_EQ(3UL, recent_permissions.size());

    auto url1_permissions = recent_permissions[0].settings;
    EXPECT_EQ(4UL, url1_permissions.size());

    EXPECT_EQ(kLocation, url1_permissions[0].content_type);
    EXPECT_EQ(kNotifications, url1_permissions[1].content_type);
    EXPECT_EQ(kImages, url1_permissions[2].content_type);
    EXPECT_EQ(kPopups, url1_permissions[3].content_type);
  }

  // Check that adding a conflicting permission to the regular profile after
  // the incognito profile has been created returns correctly for each profile.
  const GURL url4("http://example.com:8443");
  clock()->Advance(base::Hours(1));
  incognito_map->SetContentSettingDefaultScope(url4, url4, kLocation, kAllowed);
  clock()->Advance(base::Hours(1));
  map->SetContentSettingDefaultScope(url4, url4, kLocation, kBlocked);

  recent_permissions = GetRecentSitePermissions(profile(), content_types, 3);
  {
    EXPECT_EQ(3UL, recent_permissions.size());
    EXPECT_EQ(url4.spec(), recent_permissions[0].origin);
    EXPECT_EQ(url4.spec(), recent_permissions[1].origin);
    EXPECT_EQ(1UL, recent_permissions[0].settings.size());
    EXPECT_EQ(1UL, recent_permissions[1].settings.size());
    EXPECT_FALSE(recent_permissions[0].incognito);
    EXPECT_TRUE(recent_permissions[1].incognito);
    EXPECT_EQ(kLocation, recent_permissions[0].settings[0].content_type);
    EXPECT_EQ(kLocation, recent_permissions[1].settings[0].content_type);
    EXPECT_EQ(kBlocked, recent_permissions[0].settings[0].content_setting);
    EXPECT_EQ(kAllowed, recent_permissions[1].settings[0].content_setting);
  }

  // Check that resetting the permission to default in the regular profile
  // does not affect the permission in the incognito profile.
  clock()->Advance(base::Hours(1));
  map->SetContentSettingDefaultScope(url4, url4, kLocation, kDefault);

  recent_permissions = GetRecentSitePermissions(profile(), content_types, 3);
  {
    EXPECT_EQ(3UL, recent_permissions.size());
    EXPECT_EQ(url4.spec(), recent_permissions[0].origin);
    EXPECT_EQ(1UL, recent_permissions[0].settings.size());
    EXPECT_TRUE(recent_permissions[0].incognito);
    EXPECT_EQ(kLocation, recent_permissions[0].settings[0].content_type);
    EXPECT_EQ(kAllowed, recent_permissions[0].settings[0].content_setting);
  }
}

}  // namespace site_settings
