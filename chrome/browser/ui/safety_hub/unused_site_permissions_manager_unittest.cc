// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/unused_site_permissions_manager.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/constants.h"
#include "components/safety_check/safety_check.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const char url1[] = "https://example1.com:443";
const char url2[] = "https://example2.com:443";
const ContentSettingsType geolocation_type = ContentSettingsType::GEOLOCATION;
const ContentSettingsType mediastream_type =
    ContentSettingsType::MEDIASTREAM_CAMERA;
const ContentSettingsType chooser_type =
    ContentSettingsType::FILE_SYSTEM_ACCESS_CHOOSER_DATA;
const ContentSettingsType revoked_unused_site_type =
    ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS;
// An arbitrary large number that doesn't match any ContentSettingsType;
const int32_t unknown_type = 300000;

void PopulateWebsiteSettingsLists(base::Value::List& integer_keyed,
                                  base::Value::List& string_keyed) {
  auto* website_settings_registry =
      content_settings::WebsiteSettingsRegistry::GetInstance();
  for (const auto* info : *website_settings_registry) {
    ContentSettingsType type = info->type();
    if (content_settings::CanTrackLastVisit(type)) {
      // TODO(crbug.com/41495119): Find a way to iterate over all chooser based
      // settings and populate the revoked-chooser dictionary accordingly.
      if (content_settings::IsChooserPermissionEligibleForAutoRevocation(
              type)) {
        // Currently there's only one chooser content settings type.
        // Ensure all chooser types are covered.
        EXPECT_EQ(ContentSettingsType::FILE_SYSTEM_ACCESS_CHOOSER_DATA, type);
      }

      integer_keyed.Append(static_cast<int32_t>(type));
      string_keyed.Append(
          UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(type));
    }
  }
}

void PopulateChooserWebsiteSettingsDicts(base::Value::Dict& integer_keyed,
                                         base::Value::Dict& string_keyed) {
  integer_keyed = base::Value::Dict().Set(
      base::NumberToString(static_cast<int32_t>(chooser_type)),
      base::Value::Dict().Set("foo", "bar"));
  string_keyed = base::Value::Dict().Set(
      UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(
          chooser_type),
      base::Value::Dict().Set("foo", "bar"));
}

}  // namespace

class UnusedSitePermissionsManagerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  UnusedSitePermissionsManagerTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    enabled_features.push_back(
        content_settings::features::kSafetyCheckUnusedSitePermissions);
    enabled_features.push_back(
        content_settings::features::
            kSafetyCheckUnusedSitePermissionsForSupportedChooserPermissions);
    feature_list_.InitWithFeatures(
        /*enabled_features=*/enabled_features,
        /*disabled_features=*/{});
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
      base::TimeDelta lifetime =
          safety_check::GetUnusedSitePermissionsRevocationCleanUpThreshold()) {
    content_settings::ContentSettingConstraints constraint(clock()->Now());
    constraint.set_lifetime(lifetime);

    // `REVOKED_UNUSED_SITE_PERMISSIONS` stores base::Value::Dict with two keys:
    // (1) key for a string list of revoked permission types
    // (2) key for a dictionary, which key is a string permission type, mapped
    // to its revoked permission data in base::Value (i.e. {"foo": "bar"})
    // {
    //  "revoked": [geolocation, file-system-access-chooser-data, ... ],
    //  "revoked-chooser-permissions": {"file-system-access-chooser-data":
    //  {"foo": "bar"}}
    // }
    auto dict =
        base::Value::Dict()
            .Set(permissions::kRevokedKey,
                 base::Value::List()
                     .Append(
                         UnusedSitePermissionsManager::
                             ConvertContentSettingsTypeToKey(geolocation_type))
                     .Append(UnusedSitePermissionsManager::
                                 ConvertContentSettingsTypeToKey(chooser_type)))
            .Set(permissions::kRevokedChooserPermissionsKey,
                 base::Value::Dict().Set(
                     UnusedSitePermissionsManager::
                         ConvertContentSettingsTypeToKey(chooser_type),
                     base::Value(base::Value::Dict().Set("foo", "bar"))));

    hcsm()->SetWebsiteSettingDefaultScope(
        GURL(url), GURL(url), revoked_unused_site_type,
        base::Value(dict.Clone()), constraint);
  }

 private:
  base::SimpleTestClock clock_;
  std::unique_ptr<UnusedSitePermissionsManager> manager_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(UnusedSitePermissionsManagerTest,
       UpdateIntegerValuesToGroupName_AllContentSettings) {
  base::Value::List permissions_list_int;
  base::Value::List permissions_list_string;
  base::Value::Dict chooser_permission_dict_int;
  base::Value::Dict chooser_permission_dict_string;
  PopulateWebsiteSettingsLists(permissions_list_int, permissions_list_string);
  PopulateChooserWebsiteSettingsDicts(chooser_permission_dict_int,
                                      chooser_permission_dict_string);

  auto dict = base::Value::Dict()
                  .Set(permissions::kRevokedKey, permissions_list_int.Clone())
                  .Set(permissions::kRevokedChooserPermissionsKey,
                       chooser_permission_dict_int.Clone());

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
  EXPECT_EQ(chooser_permission_dict_int,
            GetRevokedUnusedPermissions(hcsm())[0]
                .setting_value.GetDict()
                .Find(permissions::kRevokedChooserPermissionsKey)
                ->GetDict());

  // Update disk stored content settings values from integers to strings.
  manager()->UpdateIntegerValuesToGroupName();

  // Validate content settings are stored in group name strings.
  revoked_permissions_content_settings =
      hcsm()->GetSettingsForOneType(revoked_unused_site_type);
  EXPECT_EQ(permissions_list_string, GetRevokedUnusedPermissions(hcsm())[0]
                                         .setting_value.GetDict()
                                         .Find(permissions::kRevokedKey)
                                         ->GetList());
  EXPECT_EQ(chooser_permission_dict_string,
            GetRevokedUnusedPermissions(hcsm())[0]
                .setting_value.GetDict()
                .Find(permissions::kRevokedChooserPermissionsKey)
                ->GetDict());
}

TEST_F(UnusedSitePermissionsManagerTest,
       UpdateIntegerValuesToGroupName_SubsetOfContentSettings) {
  base::Value::List permissions_list_int;
  permissions_list_int.Append(static_cast<int32_t>(geolocation_type));
  permissions_list_int.Append(static_cast<int32_t>(mediastream_type));

  auto dict = base::Value::Dict().Set(permissions::kRevokedKey,
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
      base::Value::List()
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
  base::Value::List permissions_list_int;
  permissions_list_int.Append(static_cast<int32_t>(geolocation_type));
  // Append a large number that does not match to any content settings type.
  permissions_list_int.Append(unknown_type);

  auto dict = base::Value::Dict().Set(permissions::kRevokedKey,
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
      base::Value::List()
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
  base::Value::List permissions_list_int;
  base::Value::List permissions_list_string;
  base::Value::Dict chooser_permission_dict_int;
  base::Value::Dict chooser_permission_dict_string;
  PopulateWebsiteSettingsLists(permissions_list_int, permissions_list_string);
  PopulateChooserWebsiteSettingsDicts(chooser_permission_dict_int,
                                      chooser_permission_dict_string);
  auto dict = base::Value::Dict()
                  .Set(permissions::kRevokedKey, permissions_list_int.Clone())
                  .Set(permissions::kRevokedChooserPermissionsKey,
                       chooser_permission_dict_int.Clone());

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
  EXPECT_EQ(chooser_permission_dict_string,
            GetRevokedUnusedPermissions(hcsm())[0]
                .setting_value.GetDict()
                .Find(permissions::kRevokedChooserPermissionsKey)
                ->GetDict());
}

TEST_F(UnusedSitePermissionsManagerNameMigrationTest,
       UpdateIntegerValuesToGroupName_MixedKeys) {
  // Setting up two entries one with integers and one with strings to simulate
  // partial migration in case of a crash.
  auto dict_int = base::Value::Dict().Set(
      permissions::kRevokedKey,
      base::Value::List().Append(static_cast<int32_t>(mediastream_type)));
  auto dict_string = base::Value::Dict().Set(
      permissions::kRevokedKey,
      base::Value::List().Append(
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
  auto expected_permissions_list_url1 = base::Value::List().Append(
      UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(
          mediastream_type));
  auto expected_permissions_list_url2 = base::Value::List().Append(
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
  auto dict_int = base::Value::Dict().Set(
      permissions::kRevokedKey,
      base::Value::List()
          .Append(static_cast<int32_t>(mediastream_type))
          // Append a large number that does not match to any content settings
          // type.
          .Append(unknown_type));
  auto dict_string = base::Value::Dict().Set(
      permissions::kRevokedKey,
      base::Value::List().Append(
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
      base::Value::List()
          .Append(UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(
              mediastream_type))
          .Append(unknown_type);
  auto expected_permissions_list_url2 = base::Value::List().Append(
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
