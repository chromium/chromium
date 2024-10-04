// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safety_hub_handler.h"

#include <ctime>
#include <memory>
#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/cws_info_service.h"
#include "chrome/browser/extensions/cws_info_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/safety_hub/mock_safe_browsing_database_manager.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service_factory.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/browser/ui/webui/version/version_ui.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_version.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/crx_file/id_util.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/permissions/constants.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using extensions::mojom::ManifestLocation;
using password_manager::TestPasswordStore;
using safety_hub::SafetyHubCardState;

enum SettingManager { USER, ADMIN, EXTENSION };
constexpr char kUnusedTestSite[] = "https://example1.com";
constexpr char kUsedTestSite[] = "https://example2.com";
constexpr char kAbusiveTestSite[] = "https://example3.com";
constexpr char kAbusiveAndUnusedTestSite[] = "https://example4.com";
constexpr char16_t kUsername[] = u"bob";
constexpr char16_t kCompromisedPassword[] = u"fnlsr4@cm^mdls@fkspnsg3d";
constexpr ContentSettingsType kUnusedRegularPermission =
    ContentSettingsType::GEOLOCATION;
constexpr ContentSettingsType kUnusedChooserPermission =
    ContentSettingsType::FILE_SYSTEM_ACCESS_CHOOSER_DATA;
const base::TimeDelta kLifetime = base::Days(30);

class SafetyHubHandlerTest : public testing::Test {
 public:
  SafetyHubHandlerTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {content_settings::features::kSafetyCheckUnusedSitePermissions,
         content_settings::features::
             kSafetyCheckUnusedSitePermissionsForSupportedChooserPermissions,
         features::kSafetyHubExtensionsUwSTrigger,
         features::kSafetyHubExtensionsOffStoreTrigger, features::kSafetyHub,
         safe_browsing::kSafetyHubAbusiveNotificationRevocation},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    // Set clock for HostContentSettingsMap.
    base::Time time;
    ASSERT_TRUE(base::Time::FromString("2022-09-07 13:00", &time));
    clock_.SetNow(time);
    hcsm_ = HostContentSettingsMapFactory::GetForProfile(profile());
    hcsm_->SetClockForTesting(&clock_);

    // Set up safe browsing service.
    SetUpSafeBrowsingService();

    handler_ = std::make_unique<SafetyHubHandler>(profile());
    handler()->set_web_ui(web_ui());
    handler()->AllowJavascript();

    // Run password check to fetch latest result from disk.
    safety_hub_test_util::UpdatePasswordCheckServiceAsync(
        PasswordStatusCheckServiceFactory::GetForProfile(profile()));
  }

  void TearDown() override {
    auto* partition = profile()->GetDefaultStoragePartition();
    if (partition) {
      partition->WaitForDeletionTasksForTesting();
    }
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
  }

  void AddNotificationPermissionsForReview() {
    // Set a host to have large number of notifications and keep engagement as
    // NONE.
    GURL url = GURL("https://example0.org:443");
    hcsm()->SetContentSettingDefaultScope(
        url, GURL(), ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);
    auto* notifications_engagement_service =
        NotificationsEngagementServiceFactory::GetForProfile(profile());
    notifications_engagement_service->RecordNotificationDisplayed(url, 35);

    // Trigger the update for changes to be seen.
    NotificationPermissionsReviewService* service =
        NotificationPermissionsReviewServiceFactory::GetForProfile(profile());
    CHECK(service);
    service->UpdateAsync();
    RunUntilIdle();
  }

  void AddRevokedPermission() {
    auto dict = base::Value::Dict()
                    .Set(permissions::kRevokedKey,
                         base::Value::List()
                             .Append(UnusedSitePermissionsService::
                                         ConvertContentSettingsTypeToKey(
                                             kUnusedRegularPermission))
                             .Append(UnusedSitePermissionsService::
                                         ConvertContentSettingsTypeToKey(
                                             kUnusedChooserPermission)))
                    .Set(permissions::kRevokedChooserPermissionsKey,
                         base::Value::Dict().Set(
                             UnusedSitePermissionsService::
                                 ConvertContentSettingsTypeToKey(
                                     kUnusedChooserPermission),
                             base::Value::Dict().Set("foo", "bar")));

    content_settings::ContentSettingConstraints constraint(clock()->Now());
    constraint.set_lifetime(kLifetime);

    hcsm()->SetWebsiteSettingDefaultScope(
        GURL(kUnusedTestSite), GURL(kUnusedTestSite),
        ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
        base::Value(dict.Clone()), constraint);
  }

  void AddAbusiveNotificationPermission() {
    mock_database_manager()->SetThreatTypeForUrl(
        GURL(kAbusiveTestSite),
        safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
    content_settings::ContentSettingConstraints constraint(clock()->Now());
    constraint.set_lifetime(kLifetime);
    hcsm()->SetWebsiteSettingDefaultScope(
        GURL(kAbusiveTestSite), GURL(kAbusiveTestSite),
        ContentSettingsType::REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS,
        base::Value(base::Value::Dict().Set(
            safety_hub::kRevokedStatusDictKeyStr, safety_hub::kRevokeStr)),
        constraint);
  }

  void AddAbusiveAndUnusedNotificationPermission() {
    content_settings::ContentSettingConstraints constraint(clock()->Now());
    constraint.set_lifetime(kLifetime);
    mock_database_manager()->SetThreatTypeForUrl(
        GURL(kAbusiveAndUnusedTestSite),
        safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
    // Setup abusive notification permissions.
    hcsm()->SetWebsiteSettingDefaultScope(
        GURL(kAbusiveAndUnusedTestSite), GURL(kAbusiveAndUnusedTestSite),
        ContentSettingsType::REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS,
        base::Value(base::Value::Dict().Set(
            safety_hub::kRevokedStatusDictKeyStr, safety_hub::kRevokeStr)),
        constraint);

    // Setup unused permissions.
    auto dict =
        base::Value::Dict()
            .Set(permissions::kRevokedKey,
                 base::Value::List()
                     .Append(UnusedSitePermissionsService::
                                 ConvertContentSettingsTypeToKey(
                                     kUnusedRegularPermission))
                     .Append(UnusedSitePermissionsService::
                                 ConvertContentSettingsTypeToKey(
                                     kUnusedChooserPermission)))
            .Set(permissions::kRevokedChooserPermissionsKey,
                 base::Value::Dict().Set(UnusedSitePermissionsService::
                                             ConvertContentSettingsTypeToKey(
                                                 kUnusedChooserPermission),
                                         base::Value::Dict().Set("foo", "bar")))
            .Set(safety_hub::kAbusiveRevocationExpirationKey,
                 base::TimeToValue(constraint.expiration()))
            .Set(safety_hub::kAbusiveRevocationLifetimeKey,
                 base::TimeDeltaToValue(constraint.lifetime()));
    hcsm()->SetWebsiteSettingDefaultScope(
        GURL(kAbusiveAndUnusedTestSite), GURL(kAbusiveAndUnusedTestSite),
        ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
        base::Value(dict.Clone()), constraint);
  }

  void CreateLeakedCredential() {
    profile_store().AddLogin(
        MakeForm(kUsername, kCompromisedPassword, kUsedTestSite, true));
    PasswordStatusCheckService* password_service =
        PasswordStatusCheckServiceFactory::GetForProfile(profile());
    safety_hub_test_util::UpdatePasswordCheckServiceAsync(password_service);
    EXPECT_EQ(password_service->compromised_credential_count(), 1UL);
  }

  void FixLeakedCredential() {
    profile_store().UpdateLogin(
        MakeForm(kUsername, u"new_fnlsr4@cm^mls@fkspnsg3d"));
    PasswordStatusCheckService* password_service =
        PasswordStatusCheckServiceFactory::GetForProfile(profile());
    safety_hub_test_util::UpdatePasswordCheckServiceAsync(password_service);
    EXPECT_EQ(password_service->compromised_credential_count(), 0UL);
  }

  void AddExtensionsForReview() {
    extensions::CWSInfoServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return safety_hub_test_util::GetMockCWSInfoService(
              Profile::FromBrowserContext(context));
        }));
    safety_hub_test_util::CreateMockExtensions(profile());
  }

  void CreatMockCWSInfoService() {
    extensions::CWSInfoServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return safety_hub_test_util::GetMockCWSInfoServiceNoTriggers(
              Profile::FromBrowserContext(context));
        }));
  }

  void AddTriggeringExtension() {
    handler_->SetTriggeringExtensionForTesting("Test");
  }

  void ExpectRevokedUnusedSitePermission(const std::string& url) {
    EXPECT_EQ(ContentSetting::CONTENT_SETTING_ASK,
              hcsm()->GetContentSetting(GURL(url), GURL(url),
                                        kUnusedRegularPermission));
    EXPECT_EQ(base::Value(),
              hcsm()->GetWebsiteSetting(GURL(url), GURL(url),
                                        kUnusedChooserPermission));
  }

  void ExpectRevokedAbusiveNotificationPermission(const std::string& url) {
    EXPECT_EQ(ContentSetting::CONTENT_SETTING_ASK,
              hcsm()->GetContentSetting(GURL(url), GURL(url),
                                        ContentSettingsType::NOTIFICATIONS));
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
        NOTREACHED_IN_MIGRATION()
            << "Unexpected value for managed_by argument. \n";
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

  void ValidateEntryPointHasRecommendationsAndHeader(bool hasRecommendations) {
    base::Value::List args;
    args.Append("getSafetyHubEntryPointData");

    handler()->HandleGetSafetyHubEntryPointData(args);

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();

    EXPECT_EQ("cr.webUIResponse", data.function_name());
    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ("getSafetyHubEntryPointData", data.arg1()->GetString());
    // arg2 is a boolean that is true if the callback is successful.
    ASSERT_TRUE(data.arg2()->is_bool());
    ASSERT_TRUE(data.arg2());

    // Validate the hasRecommendations value.
    ASSERT_TRUE(data.arg3()->is_dict());
    EXPECT_EQ(hasRecommendations,
              data.arg3()->GetDict().FindBool("hasRecommendations"));
    // Validate the header value.
    std::string header = hasRecommendations
                             ? l10n_util::GetStringUTF8(
                                   IDS_SETTINGS_SAFETY_HUB_ENTRY_POINT_HEADER)
                             : "";
    EXPECT_EQ(header, *data.arg3()->GetDict().FindString("header"));
  }

  // For a given Safety Hub module, configure the test environment so that tests
  // can run when there is a recommendation for the module and when there is
  // not.
  void SetupTestToShowOrHideRecommendationForModule(
      SafetyHubHandler::SafetyHubModule module,
      bool isModuleRecommended) {
    switch (module) {
      case SafetyHubHandler::SafetyHubModule::kPasswords:
        isModuleRecommended ? CreateLeakedCredential() : FixLeakedCredential();
        break;
      case SafetyHubHandler::SafetyHubModule::kVersion:
        isModuleRecommended
            ? g_browser_process->GetBuildState()->SetUpdate(
                  BuildState::UpdateType::kNormalUpdate,
                  base::Version({CHROME_VERSION_MAJOR, CHROME_VERSION_MINOR,
                                 CHROME_VERSION_BUILD,
                                 CHROME_VERSION_PATCH + 1}),
                  std::nullopt)
            : g_browser_process->GetBuildState()->SetUpdate(
                  BuildState::UpdateType::kNone, base::Version(), std::nullopt);
        break;
      case SafetyHubHandler::SafetyHubModule::kSafeBrowsing:
        isModuleRecommended
            ? SetPrefsForSafeBrowsing(false, false, SettingManager::USER)
            : SetPrefsForSafeBrowsing(true, true, SettingManager::USER);
        break;
      case SafetyHubHandler::SafetyHubModule::kExtensions:
        isModuleRecommended ? AddTriggeringExtension()
                            : ClearTriggeringExtensions();
        break;
      case SafetyHubHandler::SafetyHubModule::kNotifications:
        hcsm()->ClearSettingsForOneType(
            ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW);
        isModuleRecommended
            ? AddNotificationPermissionsForReview()
            : handler()->HandleIgnoreOriginsForNotificationPermissionReview(
                  base::Value::List().Append(GetOriginList(1)));
        break;
      case SafetyHubHandler::SafetyHubModule::kUnusedSitePermissions:
        isModuleRecommended
            ? AddRevokedPermission()
            : handler()->HandleAcknowledgeRevokedUnusedSitePermissionsList(
                  base::Value::List());
        break;
      default:
        NOTREACHED_IN_MIGRATION()
            << "Unexpected SafetyHubModule for test setup. A proper setup for "
               "the module can be done only for supported modules.\n";
    }
  }

  void AddUnusedPermission() {
    // Create a revoked permission.
    AddRevokedPermission();

    // There should be only an unused URL in the revoked permissions list.
    const auto& revoked_permissions =
        handler()->PopulateUnusedSitePermissionsData();
    EXPECT_EQ(revoked_permissions.size(), 1UL);
    EXPECT_EQ(GURL(kUnusedTestSite),
              GURL(*revoked_permissions[0].GetDict().FindString(
                  site_settings::kOrigin)));
  }

  void ClearTriggeringExtensions() {
    handler_->ClearExtensionResultsForTesting();
  }

  void ValidateEntryPointSubheader(
      std::string subheader,
      std::optional<SafetyHubHandler::SafetyHubModule> module = std::nullopt) {
    // If there is a module provided, expect the module to be in a subheader.
    // For that, setup the test environment so that the module has a
    // recommendation.
    if (module.has_value()) {
      SetupTestToShowOrHideRecommendationForModule(module.value(), true);
    }

    // Send a message to handler to get the current state of the subheader.
    base::Value::List args;
    args.Append("getSafetyHubEntryPointData");
    handler()->HandleGetSafetyHubEntryPointData(args);
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();

    // Check that response from the handler follows the right format.
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ("getSafetyHubEntryPointData", data.arg1()->GetString());
    // arg2 is a boolean that is true if the callback is successful.
    ASSERT_TRUE(data.arg2()->is_bool());
    ASSERT_TRUE(data.arg2());

    // Validate that the subheader we get is equal to the one we expect.
    ASSERT_TRUE(data.arg3()->is_dict());
    EXPECT_EQ(subheader, *data.arg3()->GetDict().FindString("subheader"));

    // If in the beginning of the method the test environment is set for a
    // module to have a recommendation, reset that back.
    if (module.has_value()) {
      SetupTestToShowOrHideRecommendationForModule(module.value(), false);
    }
  }

  base::Value::List GetOriginList(int size) {
    base::Value::List origins;
    for (int i = 0; i < size; i++) {
      origins.Append("https://example" + base::NumberToString(i) + ".org:443");
    }
    return origins;
  }

  // TODO(crbug.com/40267370): Consider moving common test util functions
  // between this file and password_status_check_service_unittest.cc to a util
  // class.
  password_manager::PasswordForm MakeForm(std::u16string_view username,
                                          std::u16string_view password,
                                          std::string origin = kUsedTestSite,
                                          bool is_leaked = false) {
    password_manager::PasswordForm form;
    form.username_value = username;
    form.password_value = password;
    form.signon_realm = origin;
    form.url = GURL(origin);

    if (is_leaked) {
      // Credential issues for weak and reused are detected automatically and
      // don't need to be specified explicitly.
      form.password_issues.insert_or_assign(
          password_manager::InsecureType::kLeaked,
          password_manager::InsecurityMetadata(
              base::Time::Now(), password_manager::IsMuted(false),
              password_manager::TriggerBackendNotification(false)));
    }
    return form;
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  TestingProfile* profile() { return &profile_; }
  content::TestWebUI* web_ui() { return &web_ui_; }
  SafetyHubHandler* handler() { return handler_.get(); }
  HostContentSettingsMap* hcsm() { return hcsm_.get(); }
  base::SimpleTestClock* clock() { return &clock_; }
  password_manager::TestPasswordStore& profile_store() {
    return *profile_store_;
  }
  password_manager::TestPasswordStore& account_store() {
    return *account_store_;
  }
  MockSafeBrowsingDatabaseManager* mock_database_manager() {
    return mock_database_manager_.get();
  }

 private:
  void SetUpSafeBrowsingService() {
    mock_database_manager_ =
        base::MakeRefCounted<MockSafeBrowsingDatabaseManager>();
    safe_browsing_factory_ =
        std::make_unique<safe_browsing::TestSafeBrowsingServiceFactory>();
    safe_browsing_factory_->SetTestDatabaseManager(
        mock_database_manager_.get());
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        safe_browsing_factory_->CreateSafeBrowsingService());
  }

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebUI web_ui_;
  scoped_refptr<HostContentSettingsMap> hcsm_;
  base::SimpleTestClock clock_;
  scoped_refptr<TestPasswordStore> profile_store_ =
      CreateAndUseTestPasswordStore(&profile_);
  scoped_refptr<password_manager::TestPasswordStore> account_store_ =
      CreateAndUseTestAccountPasswordStore(&profile_);
  std::unique_ptr<SafetyHubHandler> handler_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> mock_database_manager_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      safe_browsing_factory_;
};

TEST_F(SafetyHubHandlerTest, PopulateUnusedSitePermissionsData) {
  AddUnusedPermission();
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
  const auto& revoked_permission_dict = revoked_permissions[0].GetDict();

  EXPECT_EQ(GURL(kUnusedTestSite),
            GURL(*revoked_permission_dict.FindString(site_settings::kOrigin)));

  const auto expiration = base::ValueToTime(
      *revoked_permission_dict.Find(safety_hub::kExpirationKey));
  EXPECT_EQ(expiration, clock()->Now() + kLifetime);

  const auto lifetime = base::ValueToTimeDelta(
      *revoked_permission_dict.Find(safety_hub::kLifetimeKey));
  EXPECT_EQ(lifetime, kLifetime);

  const auto* chooser_permissions_data = revoked_permission_dict.FindDict(
      safety_hub::kSafetyHubChooserPermissionsData);
  EXPECT_TRUE(chooser_permissions_data->contains(
      UnusedSitePermissionsService::ConvertContentSettingsTypeToKey(
          kUnusedChooserPermission)));
}

TEST_F(SafetyHubHandlerTest, HandleAllowPermissionsAgainForUnusedSite) {
  AddUnusedPermission();
  base::Value::List initial_unused_site_permissions =
      handler()->PopulateUnusedSitePermissionsData();
  ExpectRevokedUnusedSitePermission(kUnusedTestSite);
  EXPECT_EQ(hcsm()
                ->GetSettingsForOneType(
                    ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS)
                .size(),
            1U);

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
                                kUnusedRegularPermission));
  EXPECT_EQ(
      base::Value::Dict().Set("foo", "bar"),
      hcsm()->GetWebsiteSetting(GURL(kUnusedTestSite), GURL(kUnusedTestSite),
                                kUnusedChooserPermission));

  // Undoing restores the initial state.
  handler()->HandleUndoAllowPermissionsAgainForUnusedSite(
      std::move(initial_unused_site_permissions));
  ExpectRevokedUnusedSitePermission(kUnusedTestSite);
  EXPECT_EQ(hcsm()
                ->GetSettingsForOneType(
                    ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS)
                .size(),
            1U);
}

TEST_F(SafetyHubHandlerTest,
       HandleAcknowledgeRevokedUnusedSitePermissionsList) {
  AddUnusedPermission();
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

TEST_F(SafetyHubHandlerTest, PopulateAbusiveAndUnusedSitePermissionsData) {
  AddAbusiveNotificationPermission();
  AddRevokedPermission();
  AddAbusiveAndUnusedNotificationPermission();

  // Revoked permissions list should contain all 3 urls.
  const auto& revoked_permissions =
      handler()->PopulateUnusedSitePermissionsData();
  EXPECT_EQ(revoked_permissions.size(), 3UL);
  EXPECT_EQ(GURL(kUnusedTestSite),
            GURL(*revoked_permissions[0].GetDict().FindString(
                site_settings::kOrigin)));
  EXPECT_EQ(GURL(kAbusiveAndUnusedTestSite),
            GURL(*revoked_permissions[1].GetDict().FindString(
                site_settings::kOrigin)));
  EXPECT_EQ(GURL(kAbusiveTestSite),
            GURL(*revoked_permissions[2].GetDict().FindString(
                site_settings::kOrigin)));

  // Unused site url should have unused permissions in permission list.
  auto* revoked_permission_list_unused =
      revoked_permissions[0].GetDict().FindList(site_settings::kPermissions);
  EXPECT_EQ((*revoked_permission_list_unused)[0], "location");
  EXPECT_EQ((*revoked_permission_list_unused)[1],
            "file-system-access-handles-data");

  // Abusive and unused site url should have both notifications and unused
  // permissions in permission list.
  auto* revoked_permission_list_abusive_and_unused =
      revoked_permissions[1].GetDict().FindList(site_settings::kPermissions);
  EXPECT_EQ((*revoked_permission_list_abusive_and_unused)[0], "location");
  EXPECT_EQ((*revoked_permission_list_abusive_and_unused)[1], "notifications");
  EXPECT_EQ((*revoked_permission_list_abusive_and_unused)[2],
            "file-system-access-handles-data");

  // Abusive notification url should have notifications in permission list.
  auto* revoked_permission_list_abusive =
      revoked_permissions[2].GetDict().FindList(site_settings::kPermissions);
  EXPECT_EQ((*revoked_permission_list_abusive)[0], "notifications");

  // Notifications should not be allowed.
  ExpectRevokedAbusiveNotificationPermission(kAbusiveAndUnusedTestSite);
  ExpectRevokedAbusiveNotificationPermission(kAbusiveTestSite);
}

TEST_F(SafetyHubHandlerTest, HandleAllowPermissionsAgainForAbusiveSite) {
  AddAbusiveNotificationPermission();
  base::Value::List initial_abusive_site_permissions =
      handler()->PopulateUnusedSitePermissionsData();
  ExpectRevokedAbusiveNotificationPermission(kAbusiveTestSite);

  // Allow the revoked permission for the unused site again.
  base::Value::List args;
  args.Append(base::Value(kAbusiveTestSite));
  handler()->HandleAllowPermissionsAgainForUnusedSite(args);

  // Check there is no origin in revoked permissions list.
  EXPECT_TRUE(safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm())
                  .empty());
  // Check if the permissions of url is regranted.
  EXPECT_EQ(
      ContentSetting::CONTENT_SETTING_ALLOW,
      hcsm()->GetContentSetting(GURL(kAbusiveTestSite), GURL(kAbusiveTestSite),
                                ContentSettingsType::NOTIFICATIONS));

  // Undoing restores the initial state.
  handler()->HandleUndoAllowPermissionsAgainForUnusedSite(
      std::move(initial_abusive_site_permissions));
  ExpectRevokedAbusiveNotificationPermission(kAbusiveTestSite);
}

TEST_F(SafetyHubHandlerTest,
       HandleAllowPermissionsAgainForAbusiveAndUnusedSite) {
  AddAbusiveAndUnusedNotificationPermission();
  base::Value::List initial_abusive_and_unused_site_permissions =
      handler()->PopulateUnusedSitePermissionsData();

  // Allow the revoked permission for the unused site again.
  base::Value::List args;
  args.Append(base::Value(kAbusiveAndUnusedTestSite));
  handler()->HandleAllowPermissionsAgainForUnusedSite(args);

  // Check there is no origin in revoked permissions list.
  EXPECT_TRUE(safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm())
                  .empty());
  EXPECT_EQ(0U, hcsm()
                    ->GetSettingsForOneType(
                        ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS)
                    .size());
  // Check if the permissions of url is regranted.
  EXPECT_EQ(ContentSetting::CONTENT_SETTING_ALLOW,
            hcsm()->GetContentSetting(GURL(kAbusiveAndUnusedTestSite),
                                      GURL(kAbusiveAndUnusedTestSite),
                                      ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(ContentSetting::CONTENT_SETTING_ALLOW,
            hcsm()->GetContentSetting(GURL(kAbusiveAndUnusedTestSite),
                                      GURL(kAbusiveAndUnusedTestSite),
                                      ContentSettingsType::GEOLOCATION));

  // Undoing restores the initial state.
  handler()->HandleUndoAllowPermissionsAgainForUnusedSite(
      std::move(initial_abusive_and_unused_site_permissions));
  ExpectRevokedAbusiveNotificationPermission(kAbusiveAndUnusedTestSite);
  EXPECT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()).size(),
      1U);
  ExpectRevokedUnusedSitePermission(kAbusiveAndUnusedTestSite);
  EXPECT_EQ(hcsm()
                ->GetSettingsForOneType(
                    ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS)
                .size(),
            1U);
}

TEST_F(SafetyHubHandlerTest,
       HandleAcknowledgeRevokedAbusiveAndUnusedSitePermissionsList) {
  AddUnusedPermission();
  AddAbusiveNotificationPermission();
  AddAbusiveAndUnusedNotificationPermission();
  const auto& revoked_permissions_before =
      handler()->PopulateUnusedSitePermissionsData();
  EXPECT_EQ(revoked_permissions_before.size(), 3U);
  ExpectRevokedUnusedSitePermission(kUnusedTestSite);
  ExpectRevokedAbusiveNotificationPermission(kAbusiveTestSite);
  ExpectRevokedUnusedSitePermission(kAbusiveAndUnusedTestSite);
  ExpectRevokedAbusiveNotificationPermission(kAbusiveAndUnusedTestSite);
  EXPECT_EQ(hcsm()
                ->GetSettingsForOneType(
                    ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS)
                .size(),
            2U);
  EXPECT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()).size(),
      2U);

  // Acknowledging revoked permissions clears the list.
  base::Value::List args;
  handler()->HandleAcknowledgeRevokedUnusedSitePermissionsList(args);
  const auto& revoked_permissions_after =
      handler()->PopulateUnusedSitePermissionsData();
  EXPECT_EQ(revoked_permissions_after.size(), 0U);
  EXPECT_EQ(hcsm()
                ->GetSettingsForOneType(
                    ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS)
                .size(),
            0U);
  EXPECT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()).size(),
      0U);

  // Undo reverts the list to its initial state.
  base::Value::List undo_args;
  undo_args.Append(revoked_permissions_before.Clone());
  handler()->HandleUndoAcknowledgeRevokedUnusedSitePermissionsList(undo_args);
  EXPECT_EQ(revoked_permissions_before,
            handler()->PopulateUnusedSitePermissionsData());
  EXPECT_EQ(hcsm()
                ->GetSettingsForOneType(
                    ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS)
                .size(),
            2U);
  EXPECT_EQ(
      safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()).size(),
      2U);
}

TEST_F(SafetyHubHandlerTest,
       HandleIgnoreOriginsForNotificationPermissionReview) {
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
  // TODO(crbug.com/40066645): Remove this after adding names for those
  // types.
  static constexpr auto kNoNameTypes =
      base::MakeFixedFlatSet<ContentSettingsType>({
          // clang-format off
          ContentSettingsType::MIDI,
          ContentSettingsType::DURABLE_STORAGE,
          ContentSettingsType::NFC,
          ContentSettingsType::FILE_SYSTEM_READ_GUARD,
          ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
          ContentSettingsType::FILE_SYSTEM_ACCESS_EXTENDED_PERMISSION,
          // clang-format on
      });

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
        base::Value::List().Append(
            UnusedSitePermissionsService::ConvertContentSettingsTypeToKey(
                type)));
    hcsm()->SetWebsiteSettingDefaultScope(
        GURL(kUnusedTestSite), GURL(kUnusedTestSite),
        ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
        base::Value(dict.Clone()));

    // Unless the permission in kNoNameTypes, it should be shown on the UI.
    const auto& revoked_permissions =
        handler()->PopulateUnusedSitePermissionsData();
    if (base::Contains(kNoNameTypes, type)) {
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

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_SETTINGS_SAFETY_HUB_VERSION_CARD_HEADER_UPDATED),
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
      std::nullopt);

  base::Value::List args;
  args.Append("getVersionCardData");
  handler()->HandleGetVersionCardData(args);

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  ASSERT_TRUE(data.arg3()->is_dict());

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_SETTINGS_SAFETY_HUB_VERSION_CARD_HEADER_RESTART),
            base::UTF8ToUTF16(*data.arg3()->GetDict().FindString("header")));
  EXPECT_EQ(l10n_util ::GetStringUTF16(
                IDS_SETTINGS_SAFETY_HUB_VERSION_CARD_SUBHEADER_RESTART),
            base::UTF8ToUTF16(*data.arg3()->GetDict().FindString("subheader")));
  EXPECT_EQ(static_cast<int>(SafetyHubCardState::kWarning),
            *data.arg3()->GetDict().FindInt("state"));
}

TEST_F(SafetyHubHandlerTest,
       HandleGetSafetyHubEntryPointData_HasRecommendationsAndHeader) {
  std::vector<SafetyHubHandler::SafetyHubModule> modules;
  modules.push_back(SafetyHubHandler::SafetyHubModule::kPasswords);
  modules.push_back(SafetyHubHandler::SafetyHubModule::kVersion);
  modules.push_back(SafetyHubHandler::SafetyHubModule::kSafeBrowsing);
  modules.push_back(SafetyHubHandler::SafetyHubModule::kExtensions);
  modules.push_back(SafetyHubHandler::SafetyHubModule::kNotifications);
  modules.push_back(SafetyHubHandler::SafetyHubModule::kUnusedSitePermissions);

  std::vector<int> masks;
  for (int i = 0; i < (int)modules.size(); i++) {
    masks.push_back(pow(2, i));
  }

  // Each module can either have a recommendation or not. To test all possible
  // combinations of modules, a binary vector and bit masking is used. i-th
  // element of the vector represents whether the i-th module in modules array
  // has a recommendation or not.
  for (int testCase = pow(2, (int)modules.size()) - 1; testCase >= 0;
       testCase--) {
    SCOPED_TRACE("testCase: " + std::bitset<8>(testCase).to_string());
    std::set<SafetyHubHandler::SafetyHubModule> recommendedModules;

    for (int i = 0; i < (int)modules.size(); i++) {
      bool isModuleRecommended = (testCase & masks[i]);
      SetupTestToShowOrHideRecommendationForModule(modules[i],
                                                   isModuleRecommended);
      if (isModuleRecommended) {
        recommendedModules.insert(modules[i]);
      }
    }

    ValidateEntryPointHasRecommendationsAndHeader(!recommendedModules.empty());
  }
}

TEST_F(SafetyHubHandlerTest,
       HandleGetSafetyHubEntryPointData_Subheader_NothingToDo) {
  // Reset unused site permissions data that is set in the test suite setup.
  SetupTestToShowOrHideRecommendationForModule(
      SafetyHubHandler::SafetyHubModule::kUnusedSitePermissions, false);
  ValidateEntryPointSubheader(l10n_util::GetStringUTF8(
      IDS_SETTINGS_SAFETY_HUB_ENTRY_POINT_NOTHING_TO_DO));
}

TEST_F(SafetyHubHandlerTest,
       HandleGetSafetyHubEntryPointData_Subheader_OneModule) {
  SetupTestToShowOrHideRecommendationForModule(
      SafetyHubHandler::SafetyHubModule::kUnusedSitePermissions, false);

  ValidateEntryPointSubheader(
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_PASSWORDS_MODULE_NAME),
      SafetyHubHandler::SafetyHubModule::kPasswords);

  ValidateEntryPointSubheader(
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_VERSION_MODULE_UPPERCASE_NAME),
      SafetyHubHandler::SafetyHubModule::kVersion);

  ValidateEntryPointSubheader(
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_SAFE_BROWSING_MODULE_NAME),
      SafetyHubHandler::SafetyHubModule::kSafeBrowsing);

  ValidateEntryPointSubheader(
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_EXTENSIONS_MODULE_UPPERCASE_NAME),
      SafetyHubHandler::SafetyHubModule::kExtensions);

  ValidateEntryPointSubheader(
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_NOTIFICATIONS_MODULE_UPPERCASE_NAME),
      SafetyHubHandler::SafetyHubModule::kNotifications);

  ValidateEntryPointSubheader(
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_PERMISSIONS_MODULE_UPPERCASE_NAME),
      SafetyHubHandler::SafetyHubModule::kUnusedSitePermissions);
}

TEST_F(SafetyHubHandlerTest,
       HandleGetSafetyHubEntryPointData_Subheader_TwoModulesWithPassword) {
  SetupTestToShowOrHideRecommendationForModule(
      SafetyHubHandler::SafetyHubModule::kUnusedSitePermissions, false);
  // Passwords module will always be in a subheader string.
  SetupTestToShowOrHideRecommendationForModule(
      SafetyHubHandler::SafetyHubModule::kPasswords, true);
  std::string expected_subheader = "";

  // The expected subheader should be "Passwords, Chrome update"
  expected_subheader =
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_PASSWORDS_MODULE_NAME) +
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_MODULE_NAME_SEPARATOR) +
      " " +
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_VERSION_MODULE_LOWERCASE_NAME);
  ValidateEntryPointSubheader(expected_subheader,
                              SafetyHubHandler::SafetyHubModule::kVersion);

  // The expected subheader should be "Passwords, Safe Browsing"
  expected_subheader =
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_PASSWORDS_MODULE_NAME) +
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_MODULE_NAME_SEPARATOR) +
      " " +
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_SAFE_BROWSING_MODULE_NAME);
  ValidateEntryPointSubheader(expected_subheader,
                              SafetyHubHandler::SafetyHubModule::kSafeBrowsing);

  // The expected subheader should be "Passwords, extensions"
  expected_subheader =
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_PASSWORDS_MODULE_NAME) +
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_MODULE_NAME_SEPARATOR) +
      " " +
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_EXTENSIONS_MODULE_LOWERCASE_NAME);
  ValidateEntryPointSubheader(expected_subheader,
                              SafetyHubHandler::SafetyHubModule::kExtensions);

  // The expected subheader should be "Passwords, notifications"
  expected_subheader =
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_PASSWORDS_MODULE_NAME) +
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_MODULE_NAME_SEPARATOR) +
      " " +
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_NOTIFICATIONS_MODULE_LOWERCASE_NAME);
  ValidateEntryPointSubheader(
      expected_subheader, SafetyHubHandler::SafetyHubModule::kNotifications);

  // The expected subheader should be "Passwords, permissions"
  expected_subheader =
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_PASSWORDS_MODULE_NAME) +
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_MODULE_NAME_SEPARATOR) +
      " " +
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_PERMISSIONS_MODULE_LOWERCASE_NAME);
  ValidateEntryPointSubheader(
      expected_subheader,
      SafetyHubHandler::SafetyHubModule::kUnusedSitePermissions);
}

TEST_F(SafetyHubHandlerTest,
       HandleGetSafetyHubEntryPointData_Subheader_AllModules) {
  AddUnusedPermission();
  SetupTestToShowOrHideRecommendationForModule(
      SafetyHubHandler::SafetyHubModule::kPasswords, true);
  SetupTestToShowOrHideRecommendationForModule(
      SafetyHubHandler::SafetyHubModule::kVersion, true);
  SetupTestToShowOrHideRecommendationForModule(
      SafetyHubHandler::SafetyHubModule::kSafeBrowsing, true);
  SetupTestToShowOrHideRecommendationForModule(
      SafetyHubHandler::SafetyHubModule::kExtensions, true);
  SetupTestToShowOrHideRecommendationForModule(
      SafetyHubHandler::SafetyHubModule::kNotifications, true);

  // The expected subheader should be "Passwords, Chrome update, Safe Browsing,
  // extensions, notifications, permissions"
  std::string expected_subheader =
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_PASSWORDS_MODULE_NAME) +
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_MODULE_NAME_SEPARATOR) +
      " " +
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_VERSION_MODULE_LOWERCASE_NAME) +
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_MODULE_NAME_SEPARATOR) +
      " " +
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_SAFE_BROWSING_MODULE_NAME) +
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_MODULE_NAME_SEPARATOR) +
      " " +
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_EXTENSIONS_MODULE_LOWERCASE_NAME) +
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_MODULE_NAME_SEPARATOR) +
      " " +
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_NOTIFICATIONS_MODULE_LOWERCASE_NAME) +
      l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_MODULE_NAME_SEPARATOR) +
      " " +
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_SAFETY_HUB_PERMISSIONS_MODULE_LOWERCASE_NAME);
  ValidateEntryPointSubheader(expected_subheader);
}

TEST_F(SafetyHubHandlerTest, ExtensionPrefAndInitialization) {
  // Create fake extensions for our pref service to load and test that
  // the `web_ui()` has recorded no events
  AddExtensionsForReview();
  EXPECT_EQ(5, handler()->GetNumberOfExtensionsThatNeedReview());
  CreatMockCWSInfoService();
  EXPECT_EQ(0u, web_ui()->call_data().size());
  // After `AcknowledgeSafetyCheckExtensions` one event should have been fired.
  safety_hub_test_util::AcknowledgeSafetyCheckExtensions(
      crx_file::id_util::GenerateId("TestExtension5"), profile());
  EXPECT_EQ(1u, web_ui()->call_data().size());
  // After `RemoveExtension` one additional event should have been fired
  // making two total events.
  safety_hub_test_util::AcknowledgeSafetyCheckExtensions(
      crx_file::id_util::GenerateId("TestExtension4"), profile());
  EXPECT_EQ(2u, web_ui()->call_data().size());
  // When the same actions are preformed on good extensions, no more
  // events are fired.
  safety_hub_test_util::AcknowledgeSafetyCheckExtensions(
      crx_file::id_util::GenerateId("lgcbbihjdcmnlndohpfhppljiilnkoab"),
      profile());
  EXPECT_EQ(2u, web_ui()->call_data().size());
  safety_hub_test_util::RemoveExtension("jadkojfancihcakelhdnpkcidencgdjg",
                                        ManifestLocation::kInternal, profile());
  EXPECT_EQ(2u, web_ui()->call_data().size());
}

class SafetyHubHandlerEitherAbusiveOrUnusedPermissionRevocationDisabledTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  SafetyHubHandlerEitherAbusiveOrUnusedPermissionRevocationDisabledTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    enabled_features.push_back(features::kSafetyHub);
    disabled_features.push_back(
        content_settings::features::
            kSafetyCheckUnusedSitePermissionsForSupportedChooserPermissions);

    if (IsUnusedPermissionRevocationDisabled()) {
      enabled_features.push_back(
          safe_browsing::kSafetyHubAbusiveNotificationRevocation);
      disabled_features.push_back(
          content_settings::features::kSafetyCheckUnusedSitePermissions);
    } else {
      enabled_features.push_back(
          content_settings::features::kSafetyCheckUnusedSitePermissions);
      disabled_features.push_back(
          safe_browsing::kSafetyHubAbusiveNotificationRevocation);
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  // If the test parameter is true, enable abusive notification revocation and
  // disable unused site permission revocation. Otherwise, do the opposite
  // (enable unused site permission revocation and disable abusive notification
  // revocation). This allows cleaner and easier testing of the same scenarios
  // for the two different feature treatments, since SafetyHubHandler already
  // tests when both features are enabled.
  bool IsUnusedPermissionRevocationDisabled() { return GetParam(); }

  void SetUp() override {
    // Set clock for HostContentSettingsMap.
    base::Time time;
    ASSERT_TRUE(base::Time::FromString("2022-09-07 13:00", &time));
    clock_.SetNow(time);
    hcsm_ = HostContentSettingsMapFactory::GetForProfile(profile());
    hcsm_->SetClockForTesting(&clock_);

    if (IsUnusedPermissionRevocationDisabled()) {
      SetUpSafeBrowsingService();
    }
    handler_ = std::make_unique<SafetyHubHandler>(profile());
    handler()->set_web_ui(web_ui());
    handler()->AllowJavascript();

    if (IsUnusedPermissionRevocationDisabled()) {
      AddAbusiveNotificationPermission();
    } else {
      AddRevokedUnusedPermission();
    }
  }

  void TearDown() override {
    if (IsUnusedPermissionRevocationDisabled()) {
      TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
    } else {
      auto* partition = profile()->GetDefaultStoragePartition();
      if (partition) {
        partition->WaitForDeletionTasksForTesting();
      }
    }
  }

  TestingProfile* profile() { return &profile_; }
  content::TestWebUI* web_ui() { return &web_ui_; }
  SafetyHubHandler* handler() { return handler_.get(); }
  HostContentSettingsMap* hcsm() { return hcsm_.get(); }
  base::SimpleTestClock* clock() { return &clock_; }
  MockSafeBrowsingDatabaseManager* mock_database_manager() {
    return mock_database_manager_.get();
  }

  void AddRevokedUnusedPermission() {
    auto dict = base::Value::Dict().Set(
        permissions::kRevokedKey,
        base::Value::List().Append(
            UnusedSitePermissionsService::ConvertContentSettingsTypeToKey(
                kUnusedRegularPermission)));

    content_settings::ContentSettingConstraints constraint(clock()->Now());
    constraint.set_lifetime(kLifetime);

    hcsm()->SetWebsiteSettingDefaultScope(
        GURL(kUnusedTestSite), GURL(kUnusedTestSite),
        ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
        base::Value(dict.Clone()), constraint);
  }

  void AddAbusiveNotificationPermission() {
    mock_database_manager()->SetThreatTypeForUrl(
        GURL(kAbusiveTestSite),
        safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
    hcsm()->SetWebsiteSettingDefaultScope(
        GURL(kAbusiveTestSite), GURL(kAbusiveTestSite),
        ContentSettingsType::REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS,
        base::Value(base::Value::Dict().Set(
            safety_hub::kRevokedStatusDictKeyStr, safety_hub::kRevokeStr)));
  }

  void ExpectRevokedUnusedSitePermission(const std::string& url) {
    EXPECT_EQ(ContentSetting::CONTENT_SETTING_ASK,
              hcsm()->GetContentSetting(GURL(url), GURL(url),
                                        kUnusedRegularPermission));
  }

  void ExpectRevokedAbusiveNotificationPermission(const std::string& url) {
    EXPECT_EQ(ContentSetting::CONTENT_SETTING_ASK,
              hcsm()->GetContentSetting(GURL(url), GURL(url),
                                        ContentSettingsType::NOTIFICATIONS));
  }

 private:
  void SetUpSafeBrowsingService() {
    mock_database_manager_ =
        base::MakeRefCounted<MockSafeBrowsingDatabaseManager>();
    safe_browsing_factory_ =
        std::make_unique<safe_browsing::TestSafeBrowsingServiceFactory>();
    safe_browsing_factory_->SetTestDatabaseManager(
        mock_database_manager_.get());
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        safe_browsing_factory_->CreateSafeBrowsingService());
  }

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebUI web_ui_;
  scoped_refptr<HostContentSettingsMap> hcsm_;
  base::SimpleTestClock clock_;
  std::unique_ptr<SafetyHubHandler> handler_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> mock_database_manager_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      safe_browsing_factory_;
};

TEST_P(SafetyHubHandlerEitherAbusiveOrUnusedPermissionRevocationDisabledTest,
       PopulateSitePermissionsData) {
  // Revoked permissions list should contain the url.
  const auto& revoked_permissions =
      handler()->PopulateUnusedSitePermissionsData();
  EXPECT_EQ(revoked_permissions.size(), 1UL);
  EXPECT_EQ(GURL(IsUnusedPermissionRevocationDisabled() ? kAbusiveTestSite
                                                        : kUnusedTestSite),
            GURL(*revoked_permissions[0].GetDict().FindString(
                site_settings::kOrigin)));

  auto* revoked_permission_list =
      revoked_permissions[0].GetDict().FindList(site_settings::kPermissions);
  if (IsUnusedPermissionRevocationDisabled()) {
    EXPECT_EQ((*revoked_permission_list)[0], "notifications");
    // Notifications should not be allowed.
    ExpectRevokedAbusiveNotificationPermission(kAbusiveTestSite);
  } else {
    EXPECT_EQ((*revoked_permission_list)[0], "location");
  }
}

TEST_P(SafetyHubHandlerEitherAbusiveOrUnusedPermissionRevocationDisabledTest,
       HandleAllowPermissionsAgainForSite) {
  base::Value::List initial_permissions =
      handler()->PopulateUnusedSitePermissionsData();
  if (IsUnusedPermissionRevocationDisabled()) {
    ExpectRevokedAbusiveNotificationPermission(kAbusiveTestSite);
  } else {
    ExpectRevokedUnusedSitePermission(kUnusedTestSite);
  }

  // Allow the revoked permission for the unused site again.
  base::Value::List args;
  args.Append(base::Value(IsUnusedPermissionRevocationDisabled()
                              ? kAbusiveTestSite
                              : kUnusedTestSite));
  handler()->HandleAllowPermissionsAgainForUnusedSite(args);

  if (IsUnusedPermissionRevocationDisabled()) {
    // Check there is no origin in revoked permissions list.
    EXPECT_TRUE(
        safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm())
            .empty());
    // Check if the permissions of url is regranted.
    EXPECT_EQ(ContentSetting::CONTENT_SETTING_ALLOW,
              hcsm()->GetContentSetting(GURL(kAbusiveTestSite),
                                        GURL(kAbusiveTestSite),
                                        ContentSettingsType::NOTIFICATIONS));
  } else {
    // Check there is no origin in revoked permissions list.
    ContentSettingsForOneType revoked_permissions_list =
        hcsm()->GetSettingsForOneType(
            ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS);
    EXPECT_TRUE(revoked_permissions_list.empty());
    // Check if the permissions of url is regranted.
    EXPECT_EQ(
        ContentSetting::CONTENT_SETTING_ALLOW,
        hcsm()->GetContentSetting(GURL(kUnusedTestSite), GURL(kUnusedTestSite),
                                  kUnusedRegularPermission));
  }

  // Undoing restores the initial state.
  handler()->HandleUndoAllowPermissionsAgainForUnusedSite(
      std::move(initial_permissions));
  if (IsUnusedPermissionRevocationDisabled()) {
    ExpectRevokedAbusiveNotificationPermission(kAbusiveTestSite);
  } else {
    ExpectRevokedUnusedSitePermission(kUnusedTestSite);
  }
}

TEST_P(SafetyHubHandlerEitherAbusiveOrUnusedPermissionRevocationDisabledTest,
       HandleAcknowledgeRevokedSitePermissionsList) {
  const auto& revoked_permissions_before =
      handler()->PopulateUnusedSitePermissionsData();
  EXPECT_EQ(revoked_permissions_before.size(), 1U);
  if (IsUnusedPermissionRevocationDisabled()) {
    ExpectRevokedAbusiveNotificationPermission(kAbusiveTestSite);
    EXPECT_EQ(safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm())
                  .size(),
              1U);
  } else {
    ContentSettingsForOneType revoked_permissions_list =
        hcsm()->GetSettingsForOneType(
            ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS);
    EXPECT_EQ(1U, revoked_permissions_list.size());
  }

  // Acknowledging revoked permissions clears the list.
  base::Value::List args;
  handler()->HandleAcknowledgeRevokedUnusedSitePermissionsList(args);
  const auto& revoked_permissions_after =
      handler()->PopulateUnusedSitePermissionsData();
  EXPECT_TRUE(revoked_permissions_after.empty());
  if (IsUnusedPermissionRevocationDisabled()) {
    EXPECT_TRUE(
        safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm())
            .empty());
  } else {
    ContentSettingsForOneType revoked_permissions_list =
        hcsm()->GetSettingsForOneType(
            ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS);
    EXPECT_TRUE(revoked_permissions_list.empty());
  }

  // Undo reverts the list to its initial state.
  base::Value::List undo_args;
  undo_args.Append(revoked_permissions_before.Clone());
  handler()->HandleUndoAcknowledgeRevokedUnusedSitePermissionsList(undo_args);
  EXPECT_EQ(revoked_permissions_before,
            handler()->PopulateUnusedSitePermissionsData());
  if (IsUnusedPermissionRevocationDisabled()) {
    EXPECT_EQ(safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm())
                  .size(),
              1U);
  } else {
    ContentSettingsForOneType revoked_permissions_list =
        hcsm()->GetSettingsForOneType(
            ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS);
    EXPECT_EQ(1U, revoked_permissions_list.size());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SafetyHubHandlerEitherAbusiveOrUnusedPermissionRevocationDisabledTest,
    testing::Bool());
