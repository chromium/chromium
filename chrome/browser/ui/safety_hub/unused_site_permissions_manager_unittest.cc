// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/unused_site_permissions_manager.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/permissions/constants.h"
#include "components/permissions/features.h"
#include "components/safety_check/safety_check.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const char url1[] = "https://example1.com:443";
const char url2[] = "https://example2.com:443";
const char url4[] = "https://example4.com:443";
const char url5[] = "https://example5.com:443";
const ContentSettingsType geolocation_type = ContentSettingsType::GEOLOCATION;
const ContentSettingsType notifications_type =
    ContentSettingsType::NOTIFICATIONS;
const ContentSettingsType mediastream_type =
    ContentSettingsType::MEDIASTREAM_CAMERA;
const ContentSettingsType automatic_downloads_type =
    ContentSettingsType::AUTOMATIC_DOWNLOADS;
const ContentSettingsType revoked_unused_site_type =
    ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS;
// An arbitrary large number that doesn't match any ContentSettingsType;
const int32_t unknown_type = 300000;

void PopulateWebsiteSettingsLists(base::ListValue& integer_keyed,
                                  base::ListValue& string_keyed) {
  auto* website_settings_registry =
      content_settings::WebsiteSettingsRegistry::GetInstance();
  for (const auto* info : *website_settings_registry) {
    ContentSettingsType type = info->type();
    if (content_settings::CanTrackLastVisit(type)) {
      integer_keyed.Append(static_cast<int32_t>(type));
      string_keyed.Append(
          UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(type));
    }
  }
}

std::unique_ptr<KeyedService> BuildTestHistoryService(
    content::BrowserContext* context) {
  auto service = std::make_unique<history::HistoryService>();
  service->Init(history::TestHistoryDatabaseParamsForPath(context->GetPath()));
  return service;
}

}  // namespace

class UnusedSitePermissionsManagerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {// Needed for background UKM reporting.
            TestingProfile::TestingFactory{
                HistoryServiceFactory::GetInstance(),
                base::BindRepeating(&BuildTestHistoryService)}};
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    base::Time time;
    ASSERT_TRUE(base::Time::FromString("2022-09-07 13:00", &time));
    clock_.SetNow(time);

    manager_ =
        std::make_unique<UnusedSitePermissionsManager>(profile(), prefs());
    prefs()->SetBoolean(
        safety_hub_prefs::kUnusedSitePermissionsRevocationEnabled, true);

    // The following lines also serve to first access and thus create the two
    // services.
    hcsm()->SetClockForTesting(&clock_);
    manager()->SetClockForTesting(&clock_);
  }

  void TearDown() override {
    manager_ = nullptr;
    // ~BrowserTaskEnvironment() will properly call Shutdown on the services.
    ChromeRenderViewHostTestHarness::TearDown();
  }

  base::SimpleTestClock* clock() { return &clock_; }

  UnusedSitePermissionsManager* manager() { return manager_.get(); }

  HostContentSettingsMap* hcsm() {
    return HostContentSettingsMapFactory::GetForProfile(profile());
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile()->GetTestingPrefService();
  }

  ContentSettingsForOneType GetRevokedUnusedPermissions(
      HostContentSettingsMap* hcsm) {
    return hcsm->GetSettingsForOneType(revoked_unused_site_type);
  }

  void SetupRevokedUnusedPermissionSite(
      std::string url,
      ContentSettingsType type = geolocation_type,
      base::TimeDelta lifetime =
          safety_check::GetUnusedSitePermissionsRevocationCleanUpThreshold()) {
    content_settings::ContentSettingConstraints constraint(clock()->Now());
    constraint.set_lifetime(lifetime);

    // `REVOKED_UNUSED_SITE_PERMISSIONS` stores base::DictValue with a key
    // for a string list of revoked permission types.
    // {
    //  "revoked": [geolocation, ... ],
    // }
    auto dict = base::DictValue().Set(
        permissions::kRevokedKey,
        base::ListValue().Append(
            UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(
                type)));

    hcsm()->SetWebsiteSettingDefaultScope(
        GURL(url), GURL(url), revoked_unused_site_type,
        base::Value(dict.Clone()), constraint);
  }

  void SetTrackedContentSettingForType(
      std::string url,
      ContentSettingsType setting_type,
      ContentSetting setting_value = ContentSetting::CONTENT_SETTING_ALLOW) {
    content_settings::ContentSettingConstraints constraint;
    constraint.set_track_last_visit_for_autoexpiration(true);
    hcsm()->SetContentSettingDefaultScope(GURL(url), GURL(url), setting_type,
                                          setting_value, constraint);
  }

  base::ListValue GetRevokedPermissionsForOneOrigin(
      HostContentSettingsMap* hcsm,
      const GURL& url) {
    base::Value setting_value(
        hcsm->GetWebsiteSetting(url, url, revoked_unused_site_type, nullptr));

    base::ListValue permissions_list;
    if (!setting_value.is_dict() ||
        !setting_value.GetDict().FindList(permissions::kRevokedKey)) {
      return permissions_list;
    }

    permissions_list =
        std::move(*setting_value.GetDict().FindList(permissions::kRevokedKey));

    return permissions_list;
  }

  bool GetAutorevocationBypassedByUser(const GURL& url,
                                       ContentSettingsType type) {
    content_settings::SettingInfo info;
    hcsm()->GetWebsiteSetting(url, url, type, &info);
    return info.metadata.autorevocation_bypassed_by_user();
  }

  void UpdateManager() {
    auto result = UnusedSitePermissionsManager::UpdateOnBackgroundThread(
        clock(), hcsm(),
        prefs()->GetBoolean(
            safety_hub_prefs::
                kUnusedSitePermissionsRevocationBackfillCompleted));
    manager()->RevokeUnusedPermissions(std::move(result));
  }

 private:
  base::SimpleTestClock clock_;
  std::unique_ptr<UnusedSitePermissionsManager> manager_;
  base::test::ScopedFeatureList feature_list_{
      content_settings::features::kSafetyCheckUnusedSitePermissions};
};

TEST_F(UnusedSitePermissionsManagerTest,
       UpdateIntegerValuesToGroupName_AllContentSettings) {
  base::ListValue permissions_list_int;
  base::ListValue permissions_list_string;
  PopulateWebsiteSettingsLists(permissions_list_int, permissions_list_string);

  auto dict = base::DictValue().Set(permissions::kRevokedKey,
                                    permissions_list_int.Clone());

  hcsm()->SetWebsiteSettingDefaultScope(GURL(url1), GURL(url1),
                                        revoked_unused_site_type,
                                        base::Value(dict.Clone()));

  ContentSettingsForOneType revoked_permissions_content_settings =
      hcsm()->GetSettingsForOneType(
          ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS);

  // Expecting no-op, stored integer values of content settings on disk.
  EXPECT_EQ(permissions_list_int, GetRevokedUnusedPermissions(hcsm())[0]
                                      .setting_value.GetDict()
                                      .Find(permissions::kRevokedKey)
                                      ->GetList());

  // Update disk stored content settings values from integers to strings.
  manager()->UpdateIntegerValuesToGroupName();

  // Validate content settings are stored in group name strings.
  revoked_permissions_content_settings =
      hcsm()->GetSettingsForOneType(revoked_unused_site_type);
  EXPECT_EQ(permissions_list_string, GetRevokedUnusedPermissions(hcsm())[0]
                                         .setting_value.GetDict()
                                         .Find(permissions::kRevokedKey)
                                         ->GetList());
}

TEST_F(UnusedSitePermissionsManagerTest,
       UpdateIntegerValuesToGroupName_SubsetOfContentSettings) {
  base::ListValue permissions_list_int;
  permissions_list_int.Append(static_cast<int32_t>(geolocation_type));
  permissions_list_int.Append(static_cast<int32_t>(mediastream_type));

  auto dict = base::DictValue().Set(permissions::kRevokedKey,
                                    permissions_list_int.Clone());
  hcsm()->SetWebsiteSettingDefaultScope(GURL(url1), GURL(url1),
                                        revoked_unused_site_type,
                                        base::Value(dict.Clone()));

  ContentSettingsForOneType revoked_permissions_content_settings =
      hcsm()->GetSettingsForOneType(revoked_unused_site_type);

  // Expecting no-op, stored integer values of content settings on disk.
  EXPECT_EQ(permissions_list_int, GetRevokedUnusedPermissions(hcsm())[0]
                                      .setting_value.GetDict()
                                      .Find(permissions::kRevokedKey)
                                      ->GetList());

  // Update disk stored content settings values from integers to strings.
  manager()->UpdateIntegerValuesToGroupName();

  // Validate content settings are stored in group name strings.
  auto permissions_list_string =
      base::ListValue()
          .Append(UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(
              geolocation_type))
          .Append(UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(
              mediastream_type));
  revoked_permissions_content_settings =
      hcsm()->GetSettingsForOneType(revoked_unused_site_type);
  EXPECT_EQ(permissions_list_string, GetRevokedUnusedPermissions(hcsm())[0]
                                         .setting_value.GetDict()
                                         .Find(permissions::kRevokedKey)
                                         ->GetList());
}

TEST_F(UnusedSitePermissionsManagerTest,
       UpdateIntegerValuesToGroupName_UnknownContentSettings) {
  base::ListValue permissions_list_int;
  permissions_list_int.Append(static_cast<int32_t>(geolocation_type));
  // Append a large number that does not match to any content settings type.
  permissions_list_int.Append(unknown_type);

  auto dict = base::DictValue().Set(permissions::kRevokedKey,
                                    permissions_list_int.Clone());
  hcsm()->SetWebsiteSettingDefaultScope(GURL(url1), GURL(url1),
                                        revoked_unused_site_type,
                                        base::Value(dict.Clone()));

  ContentSettingsForOneType revoked_permissions_content_settings =
      hcsm()->GetSettingsForOneType(revoked_unused_site_type);

  // Expecting no-op, stored integer values of content settings on disk.
  EXPECT_EQ(permissions_list_int, GetRevokedUnusedPermissions(hcsm())[0]
                                      .setting_value.GetDict()
                                      .Find(permissions::kRevokedKey)
                                      ->GetList());

  // Update disk stored content settings values from integers to strings.
  manager()->UpdateIntegerValuesToGroupName();

  // Validate content settings are stored in group name strings.
  auto permissions_list_string =
      base::ListValue()
          .Append(UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(
              geolocation_type))
          .Append(unknown_type);
  revoked_permissions_content_settings =
      hcsm()->GetSettingsForOneType(revoked_unused_site_type);
  EXPECT_EQ(permissions_list_string, GetRevokedUnusedPermissions(hcsm())[0]
                                         .setting_value.GetDict()
                                         .Find(permissions::kRevokedKey)
                                         ->GetList());
}

TEST_F(UnusedSitePermissionsManagerTest, RecordRegrantMetricForAllowAgain) {
  SetupRevokedUnusedPermissionSite(url1);
  SetupRevokedUnusedPermissionSite(url2);
  EXPECT_EQ(2U, GetRevokedUnusedPermissions(hcsm()).size());

  // Advance 14 days; this will be the expected histogram sample.
  clock()->Advance(base::Days(14));
  base::HistogramTester histogram_tester;

  // Allow the permission for `url` again
  manager()->RegrantPermissionsForOrigin(url::Origin::Create(GURL(url1)));

  // Only a single entry should be recorded in the histogram.
  const std::vector<base::Bucket> buckets = histogram_tester.GetAllSamples(
      "Settings.SafetyCheck.UnusedSitePermissionsAllowAgainDays");
  EXPECT_EQ(1U, buckets.size());
  // The recorded metric should be the elapsed days since the revocation.
  histogram_tester.ExpectUniqueSample(
      "Settings.SafetyCheck.UnusedSitePermissionsAllowAgainDays", 14, 1);
}

// TODO(crbug.com/415227458): Remove migration code for unused site permissions
// using strings.
// Tests the migration of using strings for the unused site permissions instead
// of integers when the UnusedSitePermissionsManager first starts up.
class UnusedSitePermissionsManagerNameMigrationTest
    : public ChromeRenderViewHostTestHarness {
 public:
  UnusedSitePermissionsManagerNameMigrationTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {content_settings::features::kSafetyCheckUnusedSitePermissions},
        /*disabled_features=*/{});
  }

  ContentSettingsForOneType GetRevokedUnusedPermissions(
      HostContentSettingsMap* hcsm) {
    return hcsm->GetSettingsForOneType(
        ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS);
  }

  HostContentSettingsMap* hcsm() {
    return HostContentSettingsMapFactory::GetForProfile(profile());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(UnusedSitePermissionsManagerNameMigrationTest,
       UpdateIntegerValuesToGroupName_OnlyIntegerKeys) {
  base::ListValue permissions_list_int;
  base::ListValue permissions_list_string;
  PopulateWebsiteSettingsLists(permissions_list_int, permissions_list_string);
  auto dict = base::DictValue().Set(permissions::kRevokedKey,
                                    permissions_list_int.Clone());

  hcsm()->SetWebsiteSettingDefaultScope(GURL(url1), GURL(url1),
                                        revoked_unused_site_type,
                                        base::Value(dict.Clone()));

  // Expect migration completion to be false at the beginning of the test before
  // starting the service.
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      safety_hub_prefs::kUnusedSitePermissionsRevocationMigrationCompleted));

  // When we start up a new manager instance, locally stored revoked permissions
  // should be updated from integers to strings.
  auto new_manager = std::make_unique<UnusedSitePermissionsManager>(
      profile(), profile()->GetPrefs());

  // Verify the migration is completed on after the service has started and pref
  // is set accordingly.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      safety_hub_prefs::kUnusedSitePermissionsRevocationMigrationCompleted));
  EXPECT_EQ(permissions_list_string, GetRevokedUnusedPermissions(hcsm())[0]
                                         .setting_value.GetDict()
                                         .Find(permissions::kRevokedKey)
                                         ->GetList());
}

TEST_F(UnusedSitePermissionsManagerNameMigrationTest,
       UpdateIntegerValuesToGroupName_MixedKeys) {
  // Setting up two entries one with integers and one with strings to simulate
  // partial migration in case of a crash.
  auto dict_int = base::DictValue().Set(
      permissions::kRevokedKey,
      base::ListValue().Append(static_cast<int32_t>(mediastream_type)));
  auto dict_string = base::DictValue().Set(
      permissions::kRevokedKey,
      base::ListValue().Append(
          UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(
              geolocation_type)));
  hcsm()->SetWebsiteSettingDefaultScope(GURL(url1), GURL(url1),
                                        revoked_unused_site_type,
                                        base::Value(dict_int.Clone()));
  hcsm()->SetWebsiteSettingDefaultScope(GURL(url2), GURL(url2),
                                        revoked_unused_site_type,
                                        base::Value(dict_string.Clone()));

  // Expect migration completion to be false at the beginning of the test before
  // starting the service.
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      safety_hub_prefs::kUnusedSitePermissionsRevocationMigrationCompleted));

  // When we start up a new manager instance, locally stored revoked permissions
  // should be updated from integers to strings.
  auto new_manager = std::make_unique<UnusedSitePermissionsManager>(
      profile(), profile()->GetPrefs());

  // Verify the migration is completed on after the service has started and pref
  // is set accordingly.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      safety_hub_prefs::kUnusedSitePermissionsRevocationMigrationCompleted));
  auto expected_permissions_list_url1 = base::ListValue().Append(
      UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(
          mediastream_type));
  auto expected_permissions_list_url2 = base::ListValue().Append(
      UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(
          geolocation_type));
  EXPECT_EQ(expected_permissions_list_url1,
            GetRevokedUnusedPermissions(hcsm())[0]
                .setting_value.GetDict()
                .Find(permissions::kRevokedKey)
                ->GetList());
  EXPECT_EQ(expected_permissions_list_url2,
            GetRevokedUnusedPermissions(hcsm())[1]
                .setting_value.GetDict()
                .Find(permissions::kRevokedKey)
                ->GetList());
}

TEST_F(UnusedSitePermissionsManagerNameMigrationTest,
       UpdateIntegerValuesToGroupName_MixedKeysWithUnknownTypes) {
  base::HistogramTester histogram_tester;
  // Setting up two entries one with integers and one with strings to simulate
  // partial migration in case of a crash.
  auto dict_int =
      base::DictValue().Set(permissions::kRevokedKey,
                            base::ListValue()
                                .Append(static_cast<int32_t>(mediastream_type))
                                // Append a large number that does not match to
                                // any content settings type.
                                .Append(unknown_type));
  auto dict_string = base::DictValue().Set(
      permissions::kRevokedKey,
      base::ListValue().Append(
          UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(
              geolocation_type)));
  hcsm()->SetWebsiteSettingDefaultScope(GURL(url1), GURL(url1),
                                        revoked_unused_site_type,
                                        base::Value(dict_int.Clone()));
  hcsm()->SetWebsiteSettingDefaultScope(GURL(url2), GURL(url2),
                                        revoked_unused_site_type,
                                        base::Value(dict_string.Clone()));

  // Expect migration completion to be false at the beginning of the test before
  // starting the service.
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      safety_hub_prefs::kUnusedSitePermissionsRevocationMigrationCompleted));

  // No histogram entries should be recorded for failed migration.
  histogram_tester.ExpectUniqueSample(
      "Settings.SafetyCheck.UnusedSitePermissionsMigrationFail", unknown_type,
      0);

  // When we start up a new manager instance, locally stored revoked permissions
  // should be updated from integers to strings.
  auto new_manager = std::make_unique<UnusedSitePermissionsManager>(
      profile(), profile()->GetPrefs());

  // Verify the migration is not completed on after the service has started due
  // to the unknown integer value.
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      safety_hub_prefs::kUnusedSitePermissionsRevocationMigrationCompleted));
  // Histogram entries should include the unknown type after failed migration.
  histogram_tester.ExpectUniqueSample(
      "Settings.SafetyCheck.UnusedSitePermissionsMigrationFail", unknown_type,
      1);
  auto expected_permissions_list_url1 =
      base::ListValue()
          .Append(UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(
              mediastream_type))
          .Append(unknown_type);
  auto expected_permissions_list_url2 = base::ListValue().Append(
      UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(
          geolocation_type));
  EXPECT_EQ(expected_permissions_list_url1,
            GetRevokedUnusedPermissions(hcsm())[0]
                .setting_value.GetDict()
                .Find(permissions::kRevokedKey)
                ->GetList());
  EXPECT_EQ(expected_permissions_list_url2,
            GetRevokedUnusedPermissions(hcsm())[1]
                .setting_value.GetDict()
                .Find(permissions::kRevokedKey)
                ->GetList());
}

TEST_F(UnusedSitePermissionsManagerTest, GetRevokedPermissions) {
  EXPECT_TRUE(manager()->GetRevokedPermissions().empty());

  SetupRevokedUnusedPermissionSite(url1, geolocation_type);
  SetupRevokedUnusedPermissionSite(url2, mediastream_type);

  std::vector<PermissionsData> revoked_permissions =
      manager()->GetRevokedPermissions();
  ASSERT_EQ(2U, revoked_permissions.size());
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(GURL(url1)),
            revoked_permissions[0].primary_pattern);
  EXPECT_EQ(PermissionsRevocationType::kUnusedPermissions,
            revoked_permissions[0].revocation_type);
  EXPECT_EQ(1U, revoked_permissions[0].permissions.size());
  EXPECT_TRUE(revoked_permissions[0].permissions.contains(geolocation_type));
  EXPECT_EQ(
      clock()->Now() +
          safety_check::GetUnusedSitePermissionsRevocationCleanUpThreshold(),
      revoked_permissions[0].constraints.expiration());

  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(GURL(url2)),
            revoked_permissions[1].primary_pattern);
  EXPECT_EQ(PermissionsRevocationType::kUnusedPermissions,
            revoked_permissions[1].revocation_type);
  EXPECT_EQ(1U, revoked_permissions[1].permissions.size());
  EXPECT_TRUE(revoked_permissions[1].permissions.contains(mediastream_type));
  EXPECT_EQ(
      clock()->Now() +
          safety_check::GetUnusedSitePermissionsRevocationCleanUpThreshold(),
      revoked_permissions[1].constraints.expiration());
}

TEST_F(UnusedSitePermissionsManagerTest, ClearRevokedPermissionsList) {
  SetupRevokedUnusedPermissionSite(url1);
  SetupRevokedUnusedPermissionSite(url2);
  ASSERT_EQ(2U, manager()->GetRevokedPermissions().size());

  manager()->ClearRevokedPermissionsList();
  EXPECT_TRUE(manager()->GetRevokedPermissions().empty());
  EXPECT_TRUE(GetRevokedUnusedPermissions(hcsm()).empty());
}

TEST_F(UnusedSitePermissionsManagerTest, RestoreDeletedRevokedPermission) {
  PermissionsData data;
  data.primary_pattern = ContentSettingsPattern::FromURLNoWildcard(GURL(url1));
  data.permissions[geolocation_type] = base::Value(CONTENT_SETTING_ALLOW);
  // Add notifications to simulate composite revocation data; manager should
  // filter it out since notifications are handled by other managers.
  data.permissions[ContentSettingsType::NOTIFICATIONS] = base::Value();
  data.constraints =
      content_settings::ContentSettingConstraints(clock()->Now());
  data.constraints.set_lifetime(base::Days(30));
  data.revocation_type =
      PermissionsRevocationType::kUnusedPermissionsAndDisruptiveNotifications;

  EXPECT_TRUE(manager()->GetRevokedPermissions().empty());

  manager()->RestoreDeletedRevokedPermission(data);

  std::vector<PermissionsData> restored = manager()->GetRevokedPermissions();
  ASSERT_EQ(1U, restored.size());
  EXPECT_EQ(data.primary_pattern, restored[0].primary_pattern);
  EXPECT_TRUE(restored[0].permissions.contains(geolocation_type));
  EXPECT_FALSE(
      restored[0].permissions.contains(ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(data.constraints.expiration(),
            restored[0].constraints.expiration());
}

TEST_F(UnusedSitePermissionsManagerTest, TrackOnlySingleOriginTest) {
  std::string example_url1 = "https://example1.com";
  std::string example_url2 = "https://[*.]example2.com";
  std::string example_url3 = "file:///foo/bar.txt";
  // Add one setting for all urls.
  SetTrackedContentSettingForType(example_url1, geolocation_type);
  SetTrackedContentSettingForType(example_url2, geolocation_type);
  // file:// URLs and wildcard patterns shouldn't be tracked for unused site
  // permissions.
  hcsm()->SetContentSettingDefaultScope(GURL(example_url3), GURL(example_url3),
                                        geolocation_type,
                                        ContentSetting::CONTENT_SETTING_ALLOW);

  UpdateManager();
  EXPECT_EQ(manager()->GetTrackedUnusedPermissionsForTesting().size(), 0u);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // Travel through time for 20 days.
  clock()->Advance(base::Days(20));

  // Only `example_url1` should be tracked because wildcard patterns and file://
  // URLs are not tracked for unused site permissions.
  UpdateManager();
  EXPECT_EQ(manager()->GetTrackedUnusedPermissionsForTesting().size(), 1u);
  auto tracked_origin = manager()->GetTrackedUnusedPermissionsForTesting()[0];
  EXPECT_EQ(GURL(tracked_origin.source.primary_pattern.ToString()),
            GURL(example_url1));
}

TEST_F(UnusedSitePermissionsManagerTest, FilePermissionsNotRevoked) {
  std::string file_url = "file:///foo/bar.txt";
  hcsm()->SetContentSettingDefaultScope(GURL(file_url), GURL(file_url),
                                        geolocation_type,
                                        ContentSetting::CONTENT_SETTING_ALLOW);

  UpdateManager();
  EXPECT_EQ(manager()->GetTrackedUnusedPermissionsForTesting().size(), 0u);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // Advance time past revocation threshold.
  clock()->Advance(base::Days(70));

  // Verify that file:// permissions are neither tracked nor auto-revoked.
  UpdateManager();
  EXPECT_EQ(manager()->GetTrackedUnusedPermissionsForTesting().size(), 0u);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            hcsm()->GetContentSetting(GURL(file_url), GURL(file_url),
                                      geolocation_type));
}

TEST_F(UnusedSitePermissionsManagerTest, TrackUnusedButDontRevoke) {
  SetTrackedContentSettingForType(url1, geolocation_type,
                                  ContentSetting::CONTENT_SETTING_BLOCK);

  // Travel through time for 20 days.
  clock()->Advance(base::Days(20));

  // GEOLOCATION permission should be on the tracked unused site permissions
  // list as it is denied 20 days before. The permission is not suitable for
  // revocation and this test verifies that RevokeUnusedPermissions() does not
  // enter infinite loop in such case.
  UpdateManager();
  auto unused_permissions = manager()->GetTrackedUnusedPermissionsForTesting();
  ASSERT_EQ(unused_permissions.size(), 1u);
  EXPECT_EQ(unused_permissions[0].type, geolocation_type);
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), GURL(url1)).size(), 0u);
}

TEST_F(UnusedSitePermissionsManagerTest, SecondaryPatternAlwaysWildcard) {
  const ContentSettingsType types[] = {geolocation_type,
                                       automatic_downloads_type};
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

  UpdateManager();
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // Travel through time for 70 days so that permissions are revoked.
  clock()->Advance(base::Days(70));
  UpdateManager();

  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 4u);
  for (auto unused_permission : GetRevokedUnusedPermissions(hcsm())) {
    EXPECT_EQ(unused_permission.secondary_pattern,
              ContentSettingsPattern::Wildcard());
  }
}

TEST_F(UnusedSitePermissionsManagerTest, MultipleRevocationsForSameOrigin) {
  // Grant GEOLOCATION permission for the url.
  SetTrackedContentSettingForType(url1, geolocation_type);
  EXPECT_EQ(manager()->GetTrackedUnusedPermissionsForTesting().size(), 0u);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // Travel through time for 20 days.
  clock()->Advance(base::Days(20));

  // Grant MEDIASTREAM_CAMERA permission for the url.
  SetTrackedContentSettingForType(url1, mediastream_type);

  // GEOLOCATION permission should be on the tracked unused site permissions
  // list as it is granted 20 days before. MEDIASTREAM_CAMERA permission should
  // not be tracked as it is just granted.
  UpdateManager();
  EXPECT_EQ(manager()->GetTrackedUnusedPermissionsForTesting().size(), 1u);
  EXPECT_EQ(manager()->GetTrackedUnusedPermissionsForTesting()[0].type,
            geolocation_type);

  // Travel through time for 50 days.
  clock()->Advance(base::Days(50));

  // GEOLOCATION permission should be on the revoked permissions list as it is
  // granted 70 days before. MEDIASTREAM_CAMERA permission should be on the
  // recently unused permissions list as it is granted 50 days before.
  UpdateManager();
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), GURL(url1)).size(), 1u);
  EXPECT_EQ(
      UnusedSitePermissionsManager::ConvertKeyToContentSettingsType(
          GetRevokedPermissionsForOneOrigin(hcsm(), GURL(url1))[0].GetString()),
      geolocation_type);
  EXPECT_EQ(manager()->GetTrackedUnusedPermissionsForTesting().size(), 1u);
  EXPECT_EQ(manager()->GetTrackedUnusedPermissionsForTesting()[0].type,
            mediastream_type);
}

TEST_F(UnusedSitePermissionsManagerTest, RegrantPreventsAutorevoke) {
  SetTrackedContentSettingForType(url1, geolocation_type);
  SetTrackedContentSettingForType(url2, geolocation_type);
  EXPECT_EQ(0U, GetRevokedUnusedPermissions(hcsm()).size());

  // Travel 70 days through time so that the granted permission is revoked.
  clock()->Advance(base::Days(70));
  UpdateManager();
  EXPECT_EQ(2U, GetRevokedUnusedPermissions(hcsm()).size());

  // After regranting permissions they are not revoked again even after >60 days
  // pass.
  manager()->RegrantPermissionsForOrigin(url::Origin::Create(GURL(url1)));
  manager()->RegrantPermissionsForOrigin(url::Origin::Create(GURL(url2)));

  EXPECT_TRUE(GetAutorevocationBypassedByUser(GURL(url1), geolocation_type));
  EXPECT_TRUE(GetAutorevocationBypassedByUser(GURL(url2), geolocation_type));
  EXPECT_EQ(0U, GetRevokedUnusedPermissions(hcsm()).size());

  clock()->Advance(base::Days(70));
  UpdateManager();
  EXPECT_EQ(0U, GetRevokedUnusedPermissions(hcsm()).size());
}

TEST_F(UnusedSitePermissionsManagerTest,
       UndoRegrantPermissionsForOrigin_GeolocationWithOptions) {
  struct TestCase {
    GeolocationSetting setting;
    const char* url;
  } test_cases[] = {
      {{PermissionOption::kAllowed, PermissionOption::kDenied},
       "https://example-approx-allow-precise-deny.com"},
      {{PermissionOption::kAllowed, PermissionOption::kAsk},
       "https://example-approx-allow-precise-ask.com"},
      {{PermissionOption::kAllowed, PermissionOption::kAllowed},
       "https://example-approx-allow-precise-allow.com"},
  };

  for (const auto& test_case : test_cases) {
    auto* info =
        content_settings::PermissionSettingsRegistry::GetInstance()->Get(
            ContentSettingsType::GEOLOCATION_WITH_OPTIONS);
    GURL test_url(test_case.url);
    base::Value initial_value = info->delegate().ToValue(test_case.setting);

    content_settings::ContentSettingConstraints constraint;
    constraint.set_track_last_visit_for_autoexpiration(true);
    hcsm()->SetPermissionSettingDefaultScope(
        test_url, test_url, ContentSettingsType::GEOLOCATION_WITH_OPTIONS,
        test_case.setting, constraint);

    EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

    clock()->Advance(base::Days(70));
    UpdateManager();

    EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 1u);
    std::vector<PermissionsData> result = manager()->GetRevokedPermissions();
    EXPECT_EQ(result.size(), 1u);

    manager()->RegrantPermissionsForOrigin(url::Origin::Create(test_url));

    EXPECT_TRUE(GetAutorevocationBypassedByUser(
        test_url, ContentSettingsType::GEOLOCATION_WITH_OPTIONS));

    manager()->UndoRegrantPermissionsForOrigin(result.front());

    EXPECT_FALSE(GetAutorevocationBypassedByUser(
        test_url, ContentSettingsType::GEOLOCATION_WITH_OPTIONS));
    EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 1u);

    hcsm()->SetWebsiteSettingDefaultScope(
        test_url, test_url, revoked_unused_site_type, base::Value());
  }
}

TEST_F(UnusedSitePermissionsManagerTest,
       RegrantGeolocationWithOptionsAfterRevokingGeolocation) {
  base::test::ScopedFeatureList scoped_feature{
      content_settings::features::kApproximateGeolocationPermission};

  GURL test_url(url1);
  url::Origin test_origin = url::Origin::Create(test_url);

  // Setup a revoked GEOLOCATION permission.
  SetupRevokedUnusedPermissionSite(url1);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 1u);

  manager()->RegrantPermissionsForOrigin(test_origin);
  EXPECT_EQ(
      hcsm()->GetPermissionSetting(
          test_url, test_url, ContentSettingsType::GEOLOCATION_WITH_OPTIONS),
      PermissionSetting(
          GeolocationSetting({.approximate = PermissionOption::kAllowed,
                              .precise = PermissionOption::kAllowed})));
}

TEST_F(UnusedSitePermissionsManagerTest, NotRevokeNotificationPermission) {
  // Grant GEOLOCATION and NOTIFICATION permission for the url.
  SetTrackedContentSettingForType(url1, geolocation_type);
  hcsm()->SetContentSettingDefaultScope(GURL(url1), GURL(url1),
                                        notifications_type,
                                        ContentSetting::CONTENT_SETTING_ALLOW);
  EXPECT_EQ(manager()->GetTrackedUnusedPermissionsForTesting().size(), 0u);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // Travel through time for 70 days.
  clock()->Advance(base::Days(70));

  // GEOLOCATION permission should be on the revoked permissions list, but
  // NOTIFICATION permissions should not be as notification permissions are
  // out of scope.
  UpdateManager();
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), GURL(url1)).size(), 1u);
  EXPECT_EQ(
      UnusedSitePermissionsManager::ConvertKeyToContentSettingsType(
          GetRevokedPermissionsForOneOrigin(hcsm(), GURL(url1))[0].GetString()),
      geolocation_type);

  // Clearing revoked permissions list should delete unused GEOLOCATION from
  // it but leave used NOTIFICATION permissions intact.
  manager()->ClearRevokedPermissionsList();
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), GURL(url1)).size(), 0u);
  EXPECT_EQ(hcsm()->GetContentSetting(GURL(url1), GURL(url1), geolocation_type),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_EQ(
      hcsm()->GetContentSetting(GURL(url1), GURL(url1), notifications_type),
      ContentSetting::CONTENT_SETTING_ALLOW);
}

// TODO(crbug.com/40267370): Clean-up after the backfill is done.
class UnusedSitePermissionsManagerBackfillTest
    : public ChromeRenderViewHostTestHarness {
 public:
  UnusedSitePermissionsManagerBackfillTest() = default;

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {// Needed for background UKM reporting.
            TestingProfile::TestingFactory{
                HistoryServiceFactory::GetInstance(),
                base::BindRepeating(&BuildTestHistoryService)}};
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    base::Time time;
    ASSERT_TRUE(base::Time::FromString("2025-09-07 13:00", &time));
    clock_.SetNow(time);

    prefs()->SetBoolean(
        safety_hub_prefs::kUnusedSitePermissionsRevocationEnabled, true);
    prefs()->SetBoolean(
        safety_hub_prefs::kUnusedSitePermissionsRevocationBackfillCompleted,
        false);

    manager_ =
        std::make_unique<UnusedSitePermissionsManager>(profile(), prefs());

    hcsm()->SetClockForTesting(&clock_);
    manager_->SetClockForTesting(&clock_);
  }

  void TearDown() override {
    manager_ = nullptr;
    hcsm()->SetClockForTesting(base::DefaultClock::GetInstance());
    ChromeRenderViewHostTestHarness::TearDown();
  }

  base::SimpleTestClock* clock() { return &clock_; }
  UnusedSitePermissionsManager* manager() { return manager_.get(); }
  HostContentSettingsMap* hcsm() {
    return HostContentSettingsMapFactory::GetForProfile(profile());
  }
  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile()->GetTestingPrefService();
  }
  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }

  ContentSettingsForOneType GetRevokedUnusedPermissions(
      HostContentSettingsMap* hcsm) {
    return hcsm->GetSettingsForOneType(revoked_unused_site_type);
  }

  void SetUntrackedContentSettingForType(
      std::string url,
      ContentSettingsType setting_type,
      ContentSetting setting_value = ContentSetting::CONTENT_SETTING_ALLOW) {
    content_settings::ContentSettingConstraints constraint;
    constraint.set_track_last_visit_for_autoexpiration(false);
    hcsm()->SetContentSettingDefaultScope(GURL(url), GURL(url), setting_type,
                                          setting_value, constraint);
  }

  void UpdateManager() {
    auto result = UnusedSitePermissionsManager::UpdateOnBackgroundThread(
        clock(), hcsm(),
        prefs()->GetBoolean(
            safety_hub_prefs::
                kUnusedSitePermissionsRevocationBackfillCompleted));
    manager()->RevokeUnusedPermissions(std::move(result));
  }

 private:
  base::SimpleTestClock clock_;
  std::unique_ptr<UnusedSitePermissionsManager> manager_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(UnusedSitePermissionsManagerBackfillTest,
       LastVisitedBackfill_NothingToBackfill) {
  feature_list()->InitAndEnableFeature(
      permissions::features::
          kSafetyHubUnusedPermissionRevocationForAllSurfaces);

  base::HistogramTester histogram_tester;
  const std::string completion_status_histogram_name =
      "Settings.SafetyHub.UnusedSitePermissionsModule."
      "Backfill.CompletionStatus";
  const std::string run_status_histogram_name =
      "Settings.SafetyHub.UnusedSitePermissionsModule."
      "Backfill.RunStatus";
  const std::string count_histogram_name =
      "Settings.SafetyHub.UnusedSitePermissionsModule."
      "Backfill.ListCountOnCompletion";

  // Check that backfill status is 'not completed' before triggering the
  // backfill.
  EXPECT_FALSE(prefs()->GetBoolean(
      safety_hub_prefs::kUnusedSitePermissionsRevocationBackfillCompleted));

  // Trigger the background task and check that it did not find any
  // untimestamped permissions and recorded the backfill completion status for
  // the user as 'not completed'.
  UpdateManager();
  EXPECT_EQ(manager()->GetUntimestampedPermissionsForTesting().size(), 0u);
  EXPECT_EQ(
      1U,
      histogram_tester.GetAllSamples(completion_status_histogram_name).size());
  histogram_tester.ExpectBucketCount(completion_status_histogram_name, false,
                                     1);

  // Check that UI thread that starts on background task completion marked the
  // backfill as completed.
  EXPECT_TRUE(prefs()->GetBoolean(
      safety_hub_prefs::kUnusedSitePermissionsRevocationBackfillCompleted));

  // Assert that one backfill attempt and one backfill completion are recorded
  // in UMA metrics.
  EXPECT_EQ(2U,
            histogram_tester.GetAllSamples(run_status_histogram_name).size());
  histogram_tester.ExpectBucketCount(run_status_histogram_name,
                                     false /*run started*/, 1);
  histogram_tester.ExpectBucketCount(run_status_histogram_name,
                                     true /*run completed*/, 1);

  // Assert that the number of timestamped permissions (zero) is recorded in UMA
  // metrics.
  histogram_tester.ExpectTotalCount(count_histogram_name, 1);
  histogram_tester.ExpectUniqueSample(count_histogram_name, 0, 1);
}

TEST_F(UnusedSitePermissionsManagerBackfillTest,
       LastVisitedBackfill_SuccessfullCompletion) {
  feature_list()->InitAndEnableFeature(
      permissions::features::
          kSafetyHubUnusedPermissionRevocationForAllSurfaces);

  base::HistogramTester histogram_tester;
  const std::string completion_status_histogram_name =
      "Settings.SafetyHub.UnusedSitePermissionsModule."
      "Backfill.CompletionStatus";
  const std::string run_status_histogram_name =
      "Settings.SafetyHub.UnusedSitePermissionsModule."
      "Backfill.RunStatus";
  const std::string count_histogram_name =
      "Settings.SafetyHub.UnusedSitePermissionsModule."
      "Backfill.ListCountOnCompletion";

  // Add two permissions without `last_visited` timestamp.
  SetUntrackedContentSettingForType(url5, geolocation_type);
  SetUntrackedContentSettingForType(url4, mediastream_type);

  // Trigger the background task and check that it added both permissions to
  // `untimestamped_permissions_` list and recorded the backfill completion
  // status for the user as 'not completed'.
  UpdateManager();
  EXPECT_EQ(manager()->GetUntimestampedPermissionsForTesting().size(), 2u);
  EXPECT_EQ(manager()->GetUntimestampedPermissionsForTesting()[0].type,
            geolocation_type);
  EXPECT_EQ(manager()->GetUntimestampedPermissionsForTesting()[1].type,
            mediastream_type);
  EXPECT_EQ(
      1U,
      histogram_tester.GetAllSamples(completion_status_histogram_name).size());
  histogram_tester.ExpectBucketCount(completion_status_histogram_name, false,
                                     1);

  // Check that UI thread that starts on background task completion performed
  // the backfill process on the `untimestamped_permissions_` leaving them
  // timestamped with `last_visited` that lies within the past 7 days.
  //
  // The `last_visited` is coarsed by `GetCoarseVisitedTime` [1] due to privacy.
  // It rounds given timestamp down to the nearest multiple of 7 in the past.
  // [1] components/content_settings/core/browser/content_settings_utils.cc
  const base::Time now = clock()->Now();
  content_settings::SettingInfo info;
  hcsm()->GetWebsiteSetting(GURL(url5), GURL(), geolocation_type, &info);
  EXPECT_GE(info.metadata.last_visited(), now - base::Days(7));
  EXPECT_LE(info.metadata.last_visited(), now);
  hcsm()->GetWebsiteSetting(GURL(url4), GURL(), mediastream_type, &info);
  EXPECT_GE(info.metadata.last_visited(), now - base::Days(7));
  EXPECT_LE(info.metadata.last_visited(), now);
  EXPECT_TRUE(prefs()->GetBoolean(
      safety_hub_prefs::kUnusedSitePermissionsRevocationBackfillCompleted));

  // Assert that one backfill attempt and one backfill completion are recorded
  // in UMA metrics.
  EXPECT_EQ(2U,
            histogram_tester.GetAllSamples(run_status_histogram_name).size());
  histogram_tester.ExpectBucketCount(run_status_histogram_name,
                                     false /*started*/, 1);
  histogram_tester.ExpectBucketCount(run_status_histogram_name,
                                     true /*completed*/, 1);

  // Assert that the number of timestamped permissions (two) is recorded in UMA
  // metrics.
  histogram_tester.ExpectTotalCount(count_histogram_name, 1);
  histogram_tester.ExpectUniqueSample(count_histogram_name, 2, 1);
}

TEST_F(UnusedSitePermissionsManagerBackfillTest,
       LastVisitedBackfill_RevokedAsUsual) {
  feature_list()->InitAndEnableFeature(
      permissions::features::
          kSafetyHubUnusedPermissionRevocationForAllSurfaces);

  base::HistogramTester histogram_tester;
  const std::string completion_status_histogram_name =
      "Settings.SafetyHub.UnusedSitePermissionsModule."
      "Backfill.CompletionStatus";

  // Add two permissions without `last_visited` timestamp.
  SetUntrackedContentSettingForType(url5, geolocation_type);
  SetUntrackedContentSettingForType(url4, mediastream_type);

  // Trigger the backfill and check both permissions were timestamped and the
  // backfill completion status was recorded for the user as 'not completed'.
  UpdateManager();
  EXPECT_EQ(manager()->GetUntimestampedPermissionsForTesting().size(), 2u);
  EXPECT_TRUE(prefs()->GetBoolean(
      safety_hub_prefs::kUnusedSitePermissionsRevocationBackfillCompleted));
  EXPECT_EQ(
      1U,
      histogram_tester.GetAllSamples(completion_status_histogram_name).size());
  histogram_tester.ExpectBucketCount(completion_status_histogram_name, false,
                                     1);

  // Move forward for 10 days and trigger the background task again.
  clock()->Advance(base::Days(10));
  UpdateManager();

  // Check that on consecutive runs the completion status for the user recorded
  // as 'completed'.
  EXPECT_EQ(
      2U,
      histogram_tester.GetAllSamples(completion_status_histogram_name).size());
  histogram_tester.ExpectBucketCount(completion_status_histogram_name, true, 1);

  // Check that backfilled permissions start being tracked as unused just like
  // permissions stamped on creation.
  EXPECT_EQ(manager()->GetTrackedUnusedPermissionsForTesting().size(), 2u);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // Check that backfilled permissions get auto-revoked just like
  // permissions stamped on creation.
  clock()->Advance(base::Days(60));
  UpdateManager();
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 2u);
  EXPECT_EQ(manager()->GetTrackedUnusedPermissionsForTesting().size(), 0u);
}

TEST_F(UnusedSitePermissionsManagerBackfillTest,
       LastVisitedBackfill_FlagIsOff) {
  feature_list()->InitAndDisableFeature(
      permissions::features::
          kSafetyHubUnusedPermissionRevocationForAllSurfaces);

  base::HistogramTester histogram_tester;

  // Trigger the UpdateManager() that would perform the backfill if the flag was
  // enabled.
  UpdateManager();

  // Check that backfill did not run.
  EXPECT_FALSE(prefs()->GetBoolean(
      safety_hub_prefs::kUnusedSitePermissionsRevocationBackfillCompleted));

  // Assert that no completion status for the user is recorded.
  EXPECT_EQ(0U,
            histogram_tester
                .GetAllSamples("Settings.SafetyHub.UnusedSitePermissionsModule."
                               "Backfill.CompletionStatus")
                .size());

  // Assert that no backfill attempts or completions are recorded in UMA
  // metrics.
  histogram_tester.ExpectTotalCount(
      "Settings.SafetyHub.UnusedSitePermissionsModule.Backfill.RunStatus", 0);

  // Assert that no counts of timestamped permissions are recorded in UMA
  // metrics.
  histogram_tester.ExpectTotalCount(
      "Settings.SafetyHub.UnusedSitePermissionsModule.Backfill."
      "ListCountOnCompletion",
      0);
}
