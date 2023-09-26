// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ctime>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "chrome/browser/ui/webui/settings/safety_hub_handler.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/browser/ui/webui/version/version_ui.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_version.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/constants.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using safety_hub::SafetyHubCardState;

enum SettingManager { USER, ADMIN, EXTENSION };
constexpr char kUnusedTestSite[] = "https://example1.com";
constexpr char kUsedTestSite[] = "https://example2.com";
constexpr ContentSettingsType kUnusedPermission =
    ContentSettingsType::GEOLOCATION;

class SafetyHubHandlerTest : public testing::Test {
 public:
  SafetyHubHandlerTest() {
    feature_list_.InitAndEnableFeature(
        content_settings::features::kSafetyCheckUnusedSitePermissions);
  }

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

    handler_ = std::make_unique<SafetyHubHandler>(profile());
    handler()->set_web_ui(web_ui());
    handler()->AllowJavascript();

    // Create a revoked permission.
    auto dict = base::Value::Dict().Set(
        permissions::kRevokedKey,
        base::Value::List().Append(
            static_cast<int32_t>(ContentSettingsType::GEOLOCATION)));

    hcsm()->SetWebsiteSettingDefaultScope(
        GURL(kUnusedTestSite), GURL(kUnusedTestSite),
        ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
        base::Value(dict.Clone()));

    // There should be only an unused URL in the revoked permissions list.
    const auto& revoked_permissions =
        handler()->PopulateUnusedSitePermissionsData();
    EXPECT_EQ(revoked_permissions.size(), 1UL);
    EXPECT_EQ(GURL(kUnusedTestSite),
              GURL(*revoked_permissions[0].GetDict().FindString(
                  site_settings::kOrigin)));
  }

  void TearDown() override {
    if (profile_) {
      auto* partition = profile_->GetDefaultStoragePartition();
      if (partition) {
        partition->WaitForDeletionTasksForTesting();
      }
    }
  }

  void ExpectRevokedPermission() {
    ContentSettingsForOneType revoked_permissions_list =
        hcsm()->GetSettingsForOneType(
            ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS);
    EXPECT_EQ(1U, revoked_permissions_list.size());
    EXPECT_EQ(
        ContentSetting::CONTENT_SETTING_ASK,
        hcsm()->GetContentSetting(GURL(kUnusedTestSite), GURL(kUnusedTestSite),
                                  kUnusedPermission));
  }

  void ValidateNotificationPermissionUpdate() {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());

    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ("notification-permission-review-list-maybe-changed",
              data.arg1()->GetString());

    ASSERT_TRUE(data.arg2()->is_list());
  }

  void SetPrefsForSafeBrowsing(bool is_enabled,
                               bool is_enhanced,
                               SettingManager managed_by) {
    auto* prefs = profile()->GetTestingPrefService();

    switch (managed_by) {
      case USER:
        prefs->SetUserPref(prefs::kSafeBrowsingEnabled,
                           std::make_unique<base::Value>(is_enabled));
        prefs->SetUserPref(prefs::kSafeBrowsingEnhanced,
                           std::make_unique<base::Value>(is_enhanced));
        break;
      case ADMIN:
        prefs->SetManagedPref(prefs::kSafeBrowsingEnabled,
                              std::make_unique<base::Value>(is_enabled));
        prefs->SetManagedPref(prefs::kSafeBrowsingEnhanced,
                              std::make_unique<base::Value>(is_enhanced));
        break;
      case EXTENSION:
        prefs->SetExtensionPref(prefs::kSafeBrowsingEnabled,
                                std::make_unique<base::Value>(is_enabled));
        prefs->SetExtensionPref(prefs::kSafeBrowsingEnhanced,
                                std::make_unique<base::Value>(is_enhanced));
        break;
      default:
        NOTREACHED() << "Unexpected value for managed_by argument. \n";
    }
  }

  void ValidateHandleSafeBrowsingCardData(std::string header,
                                          std::string subheader,
                                          SafetyHubCardState state) {
    base::Value::List args;
    args.Append("getSafeBrowsingState");

    handler()->HandleGetSafeBrowsingCardData(args);

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();

    EXPECT_EQ("cr.webUIResponse", data.function_name());
    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ("getSafeBrowsingState", data.arg1()->GetString());
    // arg2 is a boolean that is true if the callback is successful.
    ASSERT_TRUE(data.arg2()->is_bool());
    ASSERT_TRUE(data.arg2());
    ASSERT_TRUE(data.arg3()->is_dict());

    EXPECT_EQ(header, *data.arg3()->GetDict().FindString("header"));
    EXPECT_EQ(subheader, *data.arg3()->GetDict().FindString("subheader"));
    EXPECT_EQ(static_cast<int>(state),
              *data.arg3()->GetDict().FindInt("state"));
  }

  base::Value::List GetOriginList(int size) {
    base::Value::List origins;
    for (int i = 0; i < size; i++) {
      origins.Append("https://example" + base::NumberToString(i) + ".org:443");
    }
    return origins;
  }

  TestingProfile* profile() { return profile_.get(); }
  content::TestWebUI* web_ui() { return &web_ui_; }
  SafetyHubHandler* handler() { return handler_.get(); }
  HostContentSettingsMap* hcsm() { return hcsm_.get(); }
  base::SimpleTestClock* clock() { return &clock_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<SafetyHubHandler> handler_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebUI web_ui_;
  scoped_refptr<HostContentSettingsMap> hcsm_;
  base::SimpleTestClock clock_;
};

TEST_F(SafetyHubHandlerTest, PopulateUnusedSitePermissionsData) {
  // Add GEOLOCATION setting for url but do not add to revoked list.
  content_settings::ContentSettingConstraints constraint;
  constraint.set_track_last_visit_for_autoexpiration(true);
  hcsm()->SetContentSettingDefaultScope(
      GURL(kUsedTestSite), GURL(kUsedTestSite),
      ContentSettingsType::GEOLOCATION, ContentSetting::CONTENT_SETTING_ALLOW,
      constraint);

  // Revoked permissions list should still only contain the initial unused site.
  const auto& revoked_permissions =
      handler()->PopulateUnusedSitePermissionsData();
  EXPECT_EQ(revoked_permissions.size(), 1UL);
  EXPECT_EQ(GURL(kUnusedTestSite),
            GURL(*revoked_permissions[0].GetDict().FindString(
                site_settings::kOrigin)));
}

TEST_F(SafetyHubHandlerTest, HandleAllowPermissionsAgainForUnusedSite) {
  base::Value::List initial_unused_site_permissions =
      handler()->PopulateUnusedSitePermissionsData();
  ExpectRevokedPermission();

  // Allow the revoked permission for the unused site again.
  base::Value::List args;
  args.Append(base::Value(kUnusedTestSite));
  handler()->HandleAllowPermissionsAgainForUnusedSite(args);

  // Check there is no origin in revoked permissions list.
  ContentSettingsForOneType revoked_permissions_list =
      hcsm()->GetSettingsForOneType(
          ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS);
  EXPECT_EQ(0U, revoked_permissions_list.size());
  // Check if the permissions of url is regranted.
  EXPECT_EQ(
      ContentSetting::CONTENT_SETTING_ALLOW,
      hcsm()->GetContentSetting(GURL(kUnusedTestSite), GURL(kUnusedTestSite),
                                kUnusedPermission));

  // Undoing restores the initial state.
  handler()->HandleUndoAllowPermissionsAgainForUnusedSite(
      std::move(initial_unused_site_permissions));
  ExpectRevokedPermission();
}

TEST_F(SafetyHubHandlerTest,
       HandleAcknowledgeRevokedUnusedSitePermissionsList) {
  const auto& revoked_permissions_before =
      handler()->PopulateUnusedSitePermissionsData();
  EXPECT_GT(revoked_permissions_before.size(), 0U);
  // Acknowledging revoked permissions from unused sites clears the list.
  base::Value::List args;
  handler()->HandleAcknowledgeRevokedUnusedSitePermissionsList(args);
  const auto& revoked_permissions_after =
      handler()->PopulateUnusedSitePermissionsData();
  EXPECT_EQ(revoked_permissions_after.size(), 0U);

  // Undo reverts the list to its initial state.
  base::Value::List undo_args;
  undo_args.Append(revoked_permissions_before.Clone());
  handler()->HandleUndoAcknowledgeRevokedUnusedSitePermissionsList(undo_args);
  EXPECT_EQ(revoked_permissions_before,
            handler()->PopulateUnusedSitePermissionsData());
}

TEST_F(SafetyHubHandlerTest,
       HandleIgnoreOriginsForNotificationPermissionReview) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      ::features::kSafetyCheckNotificationPermissions);

  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile());
  ContentSettingsForOneType ignored_patterns =
      content_settings->GetSettingsForOneType(
          ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW);
  ASSERT_EQ(0U, ignored_patterns.size());

  base::Value::List args;
  args.Append(GetOriginList(1));
  handler()->HandleIgnoreOriginsForNotificationPermissionReview(args);

  // Check there is 1 origin in ignore list.
  ignored_patterns = content_settings->GetSettingsForOneType(
      ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW);
  ASSERT_EQ(1U, ignored_patterns.size());

  ValidateNotificationPermissionUpdate();
}

TEST_F(SafetyHubHandlerTest,
       HandleUndoIgnoreOriginsForNotificationPermissionReview) {
  base::Value::List args;
  args.Append(GetOriginList(1));
  handler()->HandleIgnoreOriginsForNotificationPermissionReview(args);

  // Check there is 1 origin in ignore list.
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile());
  ContentSettingsForOneType ignored_patterns =
      content_settings->GetSettingsForOneType(
          ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW);
  ASSERT_EQ(1U, ignored_patterns.size());

  // Check there are no origins in ignore list.
  handler()->HandleUndoIgnoreOriginsForNotificationPermissionReview(args);
  ignored_patterns = content_settings->GetSettingsForOneType(
      ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW);
  ASSERT_EQ(0U, ignored_patterns.size());
}

TEST_F(SafetyHubHandlerTest, HandleAllowNotificationPermissionForOrigins) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      ::features::kSafetyCheckNotificationPermissions);

  base::Value::List args;
  base::Value::List origins = GetOriginList(2);
  args.Append(origins.Clone());
  handler()->HandleAllowNotificationPermissionForOrigins(args);

  // Check the permission for the two origins is allow.
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile());
  ContentSettingsForOneType notification_permissions =
      content_settings->GetSettingsForOneType(
          ContentSettingsType::NOTIFICATIONS);
  auto type = content_settings->GetContentSetting(
      GURL(origins[0].GetString()), GURL(), ContentSettingsType::NOTIFICATIONS);
  ASSERT_EQ(CONTENT_SETTING_ALLOW, type);

  type = content_settings->GetContentSetting(
      GURL(origins[1].GetString()), GURL(), ContentSettingsType::NOTIFICATIONS);
  ASSERT_EQ(CONTENT_SETTING_ALLOW, type);

  ValidateNotificationPermissionUpdate();
}

TEST_F(SafetyHubHandlerTest, HandleBlockNotificationPermissionForOrigins) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      ::features::kSafetyCheckNotificationPermissions);

  base::Value::List args;
  base::Value::List origins = GetOriginList(2);
  args.Append(origins.Clone());

  handler()->HandleBlockNotificationPermissionForOrigins(args);

  // Check the permission for the two origins is block.
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile());
  ContentSettingsForOneType notification_permissions =
      content_settings->GetSettingsForOneType(
          ContentSettingsType::NOTIFICATIONS);
  auto type = content_settings->GetContentSetting(
      GURL(origins[0].GetString()), GURL(), ContentSettingsType::NOTIFICATIONS);
  ASSERT_EQ(CONTENT_SETTING_BLOCK, type);

  type = content_settings->GetContentSetting(
      GURL(origins[1].GetString()), GURL(), ContentSettingsType::NOTIFICATIONS);
  ASSERT_EQ(CONTENT_SETTING_BLOCK, type);

  ValidateNotificationPermissionUpdate();
}

TEST_F(SafetyHubHandlerTest, HandleResetNotificationPermissionForOrigins) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      ::features::kSafetyCheckNotificationPermissions);

  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile());
  base::Value::List args;
  base::Value::List origins = GetOriginList(1);
  args.Append(origins.Clone());

  content_settings->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString(origins[0].GetString()),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
      CONTENT_SETTING_ALLOW);

  handler()->HandleResetNotificationPermissionForOrigins(args);

  // Check the permission for the origin is reset.
  auto type = content_settings->GetContentSetting(
      GURL(origins[0].GetString()), GURL(), ContentSettingsType::NOTIFICATIONS);
  ASSERT_EQ(CONTENT_SETTING_ASK, type);

  ValidateNotificationPermissionUpdate();
}

TEST_F(SafetyHubHandlerTest, HandleGetSafeBrowsingCardData_EnabledEnhanced) {
  SetPrefsForSafeBrowsing(true, true, SettingManager::USER);
  ValidateHandleSafeBrowsingCardData(
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_SB_ON_ENHANCED_HEADER),
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_SB_ON_ENHANCED_SUBHEADER),
      SafetyHubCardState::kSafe);

  SetPrefsForSafeBrowsing(true, true, SettingManager::EXTENSION);
  ValidateHandleSafeBrowsingCardData(
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_SB_ON_ENHANCED_HEADER),
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_SB_ON_ENHANCED_SUBHEADER),
      SafetyHubCardState::kSafe);

  SetPrefsForSafeBrowsing(true, true, SettingManager::ADMIN);
  ValidateHandleSafeBrowsingCardData(
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_SB_ON_ENHANCED_HEADER),
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_SB_ON_ENHANCED_SUBHEADER),
      SafetyHubCardState::kSafe);
}

TEST_F(SafetyHubHandlerTest, HandleGetSafeBrowsingCardData_EnabledStandard) {
  SetPrefsForSafeBrowsing(true, false, SettingManager::USER);
  ValidateHandleSafeBrowsingCardData(
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_SB_ON_STANDARD_HEADER),
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_SB_ON_STANDARD_SUBHEADER),
      SafetyHubCardState::kSafe);

  SetPrefsForSafeBrowsing(true, false, SettingManager::EXTENSION);
  ValidateHandleSafeBrowsingCardData(
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_SB_ON_STANDARD_HEADER),
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_SB_ON_STANDARD_SUBHEADER),
      SafetyHubCardState::kSafe);

  SetPrefsForSafeBrowsing(true, false, SettingManager::ADMIN);
  ValidateHandleSafeBrowsingCardData(
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_SB_ON_STANDARD_HEADER),
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_SB_ON_STANDARD_SUBHEADER),
      SafetyHubCardState::kSafe);
}

TEST_F(SafetyHubHandlerTest, HandleGetSafeBrowsingCardData_DisabledByAdmin) {
  SetPrefsForSafeBrowsing(false, false, SettingManager::ADMIN);
  ValidateHandleSafeBrowsingCardData(
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_SB_OFF_HEADER),
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_SB_OFF_MANAGED_SUBHEADER),
      SafetyHubCardState::kInfo);

  SetPrefsForSafeBrowsing(false, true, SettingManager::ADMIN);
  ValidateHandleSafeBrowsingCardData(
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_SB_OFF_HEADER),
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_SB_OFF_MANAGED_SUBHEADER),
      SafetyHubCardState::kInfo);
}

TEST_F(SafetyHubHandlerTest,
       HandleGetSafeBrowsingCardData_DisabledByExtension) {
  SetPrefsForSafeBrowsing(false, false, SettingManager::EXTENSION);
  ValidateHandleSafeBrowsingCardData(
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_SB_OFF_HEADER),
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_SB_OFF_EXTENSION_SUBHEADER),
      SafetyHubCardState::kInfo);

  SetPrefsForSafeBrowsing(false, true, SettingManager::EXTENSION);
  ValidateHandleSafeBrowsingCardData(
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_SB_OFF_HEADER),
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_SB_OFF_EXTENSION_SUBHEADER),
      SafetyHubCardState::kInfo);
}

TEST_F(SafetyHubHandlerTest, HandleGetSafeBrowsingCardData_DisabledByUser) {
  SetPrefsForSafeBrowsing(false, false, SettingManager::USER);
  ValidateHandleSafeBrowsingCardData(
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_SB_OFF_HEADER),
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_SB_OFF_USER_SUBHEADER),
      SafetyHubCardState::kWarning);

  SetPrefsForSafeBrowsing(false, true, SettingManager::USER);
  ValidateHandleSafeBrowsingCardData(
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_SB_OFF_HEADER),
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_SB_OFF_USER_SUBHEADER),
      SafetyHubCardState::kWarning);
}

// Test that revocation is happen correctly for all content setting types.
TEST_F(SafetyHubHandlerTest, RevokeAllContentSettingTypes) {
  // TODO(crbug.com/1459305): Remove this after adding names for those
  // types.
  std::list<ContentSettingsType> no_name_types = {
      ContentSettingsType::DURABLE_STORAGE,
      ContentSettingsType::ACCESSIBILITY_EVENTS,
      ContentSettingsType::NFC,
      ContentSettingsType::FILE_SYSTEM_READ_GUARD,
      ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
      ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
      ContentSettingsType::FILE_SYSTEM_ACCESS_EXTENDED_PERMISSION};

  // Add all content settings in the content setting registry to revoked
  // permissions list.
  auto* content_settings_registry =
      content_settings::ContentSettingsRegistry::GetInstance();
  for (const content_settings::ContentSettingsInfo* info :
       *content_settings_registry) {
    ContentSettingsType type = info->website_settings_info()->type();

    // If the permission can not be tracked, then also can not be revoked.
    if (!content_settings::CanTrackLastVisit(type)) {
      continue;
    }

    // If the permission can not set to ALLOW, then also can not be revoked.
    if (!content_settings_registry->Get(type)->IsSettingValid(
            ContentSetting::CONTENT_SETTING_ALLOW)) {
      continue;
    }

    // Add the permission to revoked permission list.
    auto dict = base::Value::Dict().Set(
        permissions::kRevokedKey,
        base::Value::List().Append(static_cast<int32_t>(type)));
    hcsm()->SetWebsiteSettingDefaultScope(
        GURL(kUnusedTestSite), GURL(kUnusedTestSite),
        ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
        base::Value(dict.Clone()));

    // Unless the permission in no_name_types, it should be shown on the UI.
    const auto& revoked_permissions =
        handler()->PopulateUnusedSitePermissionsData();
    bool is_no_name_type =
        (std::find(no_name_types.begin(), no_name_types.end(), type) !=
         no_name_types.end());
    if (is_no_name_type) {
      EXPECT_EQ(revoked_permissions.size(), 0U);
    } else {
      EXPECT_EQ(revoked_permissions.size(), 1U);
    }
  }
}

TEST_F(SafetyHubHandlerTest, VersionCardUpToDate) {
  base::Value::List args;
  args.Append("getVersionCardData");
  handler()->HandleGetVersionCardData(args);

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  ASSERT_TRUE(data.arg3()->is_dict());

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SETTINGS_UPGRADE_UP_TO_DATE),
            base::UTF8ToUTF16(*data.arg3()->GetDict().FindString("header")));
  EXPECT_EQ(VersionUI::GetAnnotatedVersionStringForUi(),
            base::UTF8ToUTF16(*data.arg3()->GetDict().FindString("subheader")));
  EXPECT_EQ(static_cast<int>(SafetyHubCardState::kSafe),
            *data.arg3()->GetDict().FindInt("state"));
}

TEST_F(SafetyHubHandlerTest, VersionCardOutOfDate) {
  // An update is available, the version card should let the user know.
  g_browser_process->GetBuildState()->SetUpdate(
      BuildState::UpdateType::kNormalUpdate,
      base::Version({CHROME_VERSION_MAJOR, CHROME_VERSION_MINOR,
                     CHROME_VERSION_BUILD, CHROME_VERSION_PATCH + 1}),
      absl::nullopt);

  base::Value::List args;
  args.Append("getVersionCardData");
  handler()->HandleGetVersionCardData(args);

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  ASSERT_TRUE(data.arg3()->is_dict());

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_RECOVERY_BUBBLE_TITLE),
            base::UTF8ToUTF16(*data.arg3()->GetDict().FindString("header")));
  EXPECT_EQ(l10n_util ::GetStringUTF16(
                IDS_SETTINGS_SAFETY_HUB_VERSION_CARD_SUBHEADER_RESTART),
            base::UTF8ToUTF16(*data.arg3()->GetDict().FindString("subheader")));
  EXPECT_EQ(static_cast<int>(SafetyHubCardState::kWarning),
            *data.arg3()->GetDict().FindInt("state"));
}
