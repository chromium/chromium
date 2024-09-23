// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"

#include <cstdint>
#include <ctime>
#include <list>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/json/values_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/safety_hub/abusive_notification_permissions_manager.h"
#include "chrome/browser/ui/safety_hub/mock_safe_browsing_database_manager.h"
#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "components/permissions/constants.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/pref_names.h"
#include "components/permissions/test/test_permissions_client.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char url1[] = "https://example1.com:443";
const char url2[] = "https://example2.com:443";
const char url3[] = "https://example3.com:443";
const char url4[] = "https://example4.com:443";
const ContentSettingsType automatic_downloads_type =
    ContentSettingsType::AUTOMATIC_DOWNLOADS;
const ContentSettingsType geolocation_type = ContentSettingsType::GEOLOCATION;
const ContentSettingsType mediastream_type =
    ContentSettingsType::MEDIASTREAM_CAMERA;
const ContentSettingsType notifications_type =
    ContentSettingsType::NOTIFICATIONS;
const ContentSettingsType chooser_type =
    ContentSettingsType::FILE_SYSTEM_ACCESS_CHOOSER_DATA;
const ContentSettingsType revoked_abusive_notification =
    ContentSettingsType::REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS;
const ContentSettingsType revoked_unused_site_type =
    ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS;
// An arbitrary large number that doesn't match any ContentSettingsType;
const int32_t unknown_type = 300000;

std::set<ContentSettingsType> abusive_permission_types({notifications_type});
std::set<ContentSettingsType> unused_permission_types({geolocation_type,
                                                       chooser_type});
std::set<ContentSettingsType> abusive_and_unused_permission_types(
    {notifications_type, geolocation_type, chooser_type});

std::unique_ptr<KeyedService> BuildUnusedSitePermissionsService(
    content::BrowserContext* context) {
  return std::make_unique<UnusedSitePermissionsService>(
      context, Profile::FromBrowserContext(context)->GetPrefs());
}

scoped_refptr<RefcountedKeyedService> BuildTestHostContentSettingsMap(
    content::BrowserContext* context) {
  return base::MakeRefCounted<HostContentSettingsMap>(
      Profile::FromBrowserContext(context)->GetPrefs(), false, true, false,
      false);
}

std::unique_ptr<KeyedService> BuildTestHistoryService(
    content::BrowserContext* context) {
  auto service = std::make_unique<history::HistoryService>();
  service->Init(history::TestHistoryDatabaseParamsForPath(context->GetPath()));
  return service;
}

PermissionsData CreatePermissionsData(
    ContentSettingsPattern& primary_pattern,
    std::set<ContentSettingsType>& permission_types) {
  PermissionsData permissions_data;
  permissions_data.primary_pattern = primary_pattern;
  permissions_data.permission_types = permission_types;
  return permissions_data;
}

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
          UnusedSitePermissionsService::ConvertContentSettingsTypeToKey(type));
    }
  }
}

void PopulateChooserWebsiteSettingsDicts(base::Value::Dict& integer_keyed,
                                         base::Value::Dict& string_keyed) {
  integer_keyed = base::Value::Dict().Set(
      base::NumberToString(static_cast<int32_t>(chooser_type)),
      base::Value::Dict().Set("foo", "bar"));
  string_keyed = base::Value::Dict().Set(
      UnusedSitePermissionsService::ConvertContentSettingsTypeToKey(
          chooser_type),
      base::Value::Dict().Set("foo", "bar"));
}

}  // namespace

class UnusedSitePermissionsServiceTest
    : public ChromeRenderViewHostTestHarness,
      public testing::WithParamInterface<
          std::tuple</*should_setup_abusive_notification_sites*/ bool,
                     /*should_setup_unused_sites*/ bool>> {
 public:
  UnusedSitePermissionsServiceTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    enabled_features.push_back(
        content_settings::features::kSafetyCheckUnusedSitePermissions);
    enabled_features.push_back(
        content_settings::features::
            kSafetyCheckUnusedSitePermissionsForSupportedChooserPermissions);
    enabled_features.push_back(features::kSafetyHub);
    if (ShouldSetupAbusiveNotificationSites()) {
      enabled_features.push_back(
          safe_browsing::kSafetyHubAbusiveNotificationRevocation);
    }
    feature_list_.InitWithFeatures(
        /*enabled_features=*/enabled_features,
        /*disabled_features=*/{});
  }
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    base::Time time;
    ASSERT_TRUE(base::Time::FromString("2022-09-07 13:00", &time));
    clock_.SetNow(time);
    if (ShouldSetupAbusiveNotificationSites()) {
      SetUpSafeBrowsingService();
    }
    prefs()->SetBoolean(
        safety_hub_prefs::kUnusedSitePermissionsRevocationEnabled, true);
    callback_count_ = 0;

    // The following lines also serve to first access and thus create the two
    // services.
    hcsm()->SetClockForTesting(&clock_);
    service()->SetClockForTesting(&clock_);
  }

  void TearDown() override {
    service()->SetClockForTesting(base::DefaultClock::GetInstance());
    hcsm()->SetClockForTesting(base::DefaultClock::GetInstance());
    if (ShouldSetupAbusiveNotificationSites()) {
      TearDownSafeBrowsingService();
    }

    // ~BrowserTaskEnvironment() will properly call Shutdown on the services.
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
                HostContentSettingsMapFactory::GetInstance(),
                base::BindRepeating(&BuildTestHostContentSettingsMap)},
            // Needed for background UKM reporting.
            TestingProfile::TestingFactory{
                HistoryServiceFactory::GetInstance(),
                base::BindRepeating(&BuildTestHistoryService)},
            TestingProfile::TestingFactory{
                UnusedSitePermissionsServiceFactory::GetInstance(),
                base::BindRepeating(&BuildUnusedSitePermissionsService)}};
  }

  bool ShouldSetupAbusiveNotificationSites() { return get<0>(GetParam()); }
  bool ShouldSetupUnusedSites() { return get<1>(GetParam()); }

  void ResetService() {
    // Setting the factory has the side effect of resetting the service
    // instance.
    UnusedSitePermissionsServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildUnusedSitePermissionsService));
  }

  base::SimpleTestClock* clock() { return &clock_; }

  UnusedSitePermissionsService* service() {
    return UnusedSitePermissionsServiceFactory::GetForProfile(profile());
  }

  HostContentSettingsMap* hcsm() {
    return HostContentSettingsMapFactory::GetForProfile(profile());
  }

  MockSafeBrowsingDatabaseManager* mock_database_manager() {
    return fake_database_manager_.get();
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile()->GetTestingPrefService();
  }

  uint8_t callback_count() { return callback_count_; }

  base::Time GetLastVisitedDate(GURL url, ContentSettingsType type) {
    content_settings::SettingInfo info;
    hcsm()->GetWebsiteSetting(url, url, type, &info);
    return info.metadata.last_visited();
  }

  ContentSettingsForOneType GetRevokedUnusedPermissions(
      HostContentSettingsMap* hcsm) {
    return hcsm->GetSettingsForOneType(revoked_unused_site_type);
  }

  base::Value::List GetRevokedPermissionsForOneOrigin(
      HostContentSettingsMap* hcsm,
      const GURL& url) {
    base::Value setting_value(
        hcsm->GetWebsiteSetting(url, url, revoked_unused_site_type, nullptr));

    base::Value::List permissions_list;
    if (!setting_value.is_dict() ||
        !setting_value.GetDict().FindList(permissions::kRevokedKey)) {
      return permissions_list;
    }

    permissions_list =
        std::move(*setting_value.GetDict().FindList(permissions::kRevokedKey));

    return permissions_list;
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

  void SetTrackedChooserType(std::string url) {
    content_settings::ContentSettingConstraints constraint;
    constraint.set_track_last_visit_for_autoexpiration(true);
    hcsm()->SetWebsiteSettingDefaultScope(
        GURL(url), GURL(url), chooser_type,
        base::Value(base::Value::Dict().Set("foo", "bar")), constraint);
  }

  void SetupAbusiveNotificationSite(std::string url, ContentSetting setting) {
    hcsm()->SetContentSettingDefaultScope(GURL(url), GURL(url),
                                          notifications_type, setting);
    mock_database_manager()->SetThreatTypeForUrl(
        GURL(url), safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
  }

  void SetupSafeNotificationSite(std::string url) {
    hcsm()->SetContentSettingDefaultScope(
        GURL(url), GURL(url), notifications_type,
        ContentSetting::CONTENT_SETTING_ALLOW);
    mock_database_manager()->SetThreatTypeForUrl(
        GURL(url), safe_browsing::SBThreatType::SB_THREAT_TYPE_SAFE);
  }

  void ExpectRevokedAbusiveNotificationPermissionSize(size_t expected_size) {
    ContentSettingsForOneType revoked_permissions_list =
        safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm());
    EXPECT_EQ(expected_size, revoked_permissions_list.size());
  }

  void SetupRevokedUnusedPermissionSite(std::string url) {
    content_settings::ContentSettingConstraints constraint(clock()->Now());
    constraint.set_lifetime(
        content_settings::features::
            kSafetyCheckUnusedSitePermissionsRevocationCleanUpThreshold.Get());

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
                         UnusedSitePermissionsService::
                             ConvertContentSettingsTypeToKey(geolocation_type))
                     .Append(UnusedSitePermissionsService::
                                 ConvertContentSettingsTypeToKey(chooser_type)))
            .Set(permissions::kRevokedChooserPermissionsKey,
                 base::Value::Dict().Set(
                     UnusedSitePermissionsService::
                         ConvertContentSettingsTypeToKey(chooser_type),
                     base::Value(base::Value::Dict().Set("foo", "bar"))));

    hcsm()->SetWebsiteSettingDefaultScope(
        GURL(url), GURL(url), revoked_unused_site_type,
        base::Value(dict.Clone()), constraint);
  }

  void SetupRevokedAbusiveNotificationSite(std::string url) {
    auto cleanUpThreshold =
        content_settings::features::
            kSafetyCheckUnusedSitePermissionsRevocationCleanUpThreshold.Get();
    content_settings::ContentSettingConstraints constraint(clock()->Now());
    constraint.set_lifetime(cleanUpThreshold);
    hcsm()->SetWebsiteSettingDefaultScope(
        GURL(url), GURL(url), revoked_abusive_notification,
        base::Value(base::Value::Dict().Set(
            safety_hub::kRevokedStatusDictKeyStr, safety_hub::kRevokeStr)),
        constraint);
  }

  void UndoRegrantPermissionsForUrl(
      std::string url,
      std::set<ContentSettingsType> permission_types,
      base::Time expiration = base::Time(),
      base::TimeDelta lifetime = base::Milliseconds(0)) {
    PermissionsData permissions_data;
    permissions_data.primary_pattern =
        ContentSettingsPattern::FromURLNoWildcard(GURL(url));
    permissions_data.permission_types = permission_types;
    permissions_data.chooser_permissions_data = base::Value::Dict().Set(
        UnusedSitePermissionsService::ConvertContentSettingsTypeToKey(
            chooser_type),
        base::Value::Dict().Set("foo", "bar"));
    permissions_data.constraints =
        content_settings::ContentSettingConstraints(expiration - lifetime);
    permissions_data.constraints.set_lifetime(lifetime);
    service()->UndoRegrantPermissionsForOrigin(permissions_data);
  }

  void AddRevokedPermissionToResult(
      UnusedSitePermissionsService::UnusedSitePermissionsResult* result,
      std::set<ContentSettingsType> permission_types,
      std::string url) {
    auto origin = ContentSettingsPattern::FromString(url);
    result->AddRevokedPermission(
        CreatePermissionsData(origin, permission_types));
  }

  void ExpectRevokedAbusiveNotificationSettingValues(std::string url) {
    EXPECT_TRUE(IsUrlInContentSettings(
        safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()),
        url));
    EXPECT_TRUE(
        safety_hub_util::IsUrlRevokedAbusiveNotification(hcsm(), GURL(url)));
    EXPECT_EQ(
        hcsm()->GetContentSetting(GURL(url), GURL(url), notifications_type),
        CONTENT_SETTING_ASK);
  }

  void ExpectCleanedUpAbusiveNotificationSettingValues(
      std::string url,
      bool is_regranted = false) {
    EXPECT_FALSE(IsUrlInContentSettings(
        safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()),
        url));
    EXPECT_FALSE(
        safety_hub_util::IsUrlRevokedAbusiveNotification(hcsm(), GURL(url)));
    EXPECT_EQ(
        hcsm()->GetContentSetting(GURL(url), GURL(url), notifications_type),
        is_regranted ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_ASK);
  }

  void ExpectSafeNotificationSettingValues(std::string url) {
    EXPECT_FALSE(IsUrlInContentSettings(
        safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm()),
        url));
    EXPECT_FALSE(
        safety_hub_util::IsUrlRevokedAbusiveNotification(hcsm(), GURL(url)));
    EXPECT_EQ(
        hcsm()->GetContentSetting(GURL(url), GURL(url), notifications_type),
        CONTENT_SETTING_ALLOW);
  }

  bool IsUrlInRevokedSettings(std::list<PermissionsData> permissions_data,
                              std::string url) {
    // TODO(crbug.com/40250875): Replace the below with a lambda method and
    // base::Contians.
    std::string url_pattern =
        ContentSettingsPattern::FromURLNoWildcard(GURL(url)).ToString();
    for (const auto& permission : permissions_data) {
      if (permission.primary_pattern.ToString() == url ||
          permission.primary_pattern.ToString() == url_pattern) {
        return true;
      }
    }
    return false;
  }

 private:
  void SetUpSafeBrowsingService() {
    prefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
    fake_database_manager_ =
        base::MakeRefCounted<MockSafeBrowsingDatabaseManager>();
    safe_browsing_factory_ =
        std::make_unique<safe_browsing::TestSafeBrowsingServiceFactory>();
    safe_browsing_factory_->SetTestDatabaseManager(
        fake_database_manager_.get());
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        safe_browsing_factory_->CreateSafeBrowsingService());
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Local state is needed to construct ProxyConfigService, which is a
    // dependency of PingManager on ChromeOS.
    TestingBrowserProcess::GetGlobal()->SetLocalState(profile()->GetPrefs());
#endif
  }

  void TearDownSafeBrowsingService() {
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
#endif
  }

  bool IsUrlInContentSettings(ContentSettingsForOneType content_settings,
                              std::string url) {
    // TODO(crbug.com/40250875): Replace the below with a lambda method and
    // base::Contians.
    std::string url_pattern =
        ContentSettingsPattern::FromURLNoWildcard(GURL(url)).ToString();
    for (const auto& setting : content_settings) {
      if (setting.primary_pattern.ToString() == url ||
          setting.primary_pattern.ToString() == url_pattern) {
        return true;
      }
    }
    return false;
  }

  base::SimpleTestClock clock_;
  uint8_t callback_count_;
  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> fake_database_manager_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      safe_browsing_factory_;
};

TEST_P(UnusedSitePermissionsServiceTest, UnusedSitePermissionsServiceTest) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      content_settings::features::kSafetyCheckUnusedSitePermissions);
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents());

  const base::Time now = clock()->Now();
  const base::TimeDelta precision =
      content_settings::GetCoarseVisitedTimePrecision();

  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile(), ServiceAccessType::EXPLICIT_ACCESS);
  history_service->AddPage(GURL(url1), clock()->Now(),
                           history::VisitSource::SOURCE_BROWSED);
  if (ShouldSetupUnusedSites()) {
    // Add one content setting for `url1` and two content settings +
    // one website setting for `url2`.
    SetTrackedContentSettingForType(url1, geolocation_type);
    SetTrackedContentSettingForType(url2, geolocation_type);
    SetTrackedContentSettingForType(url2, mediastream_type);
    SetTrackedChooserType(url2);
  }
  if (ShouldSetupAbusiveNotificationSites()) {
    // Add notifications setting for `url2` and `url3`, abusive notification
    // sites.
    SetupAbusiveNotificationSite(url2, ContentSetting::CONTENT_SETTING_ALLOW);
    SetupAbusiveNotificationSite(url3, ContentSetting::CONTENT_SETTING_ALLOW);
    SetupSafeNotificationSite(url4);
  }
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 0u);
  EXPECT_EQ(0U, GetRevokedUnusedPermissions(hcsm()).size());
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  if (ShouldSetupAbusiveNotificationSites()) {
    ExpectRevokedAbusiveNotificationPermissionSize(2U);
    ExpectRevokedAbusiveNotificationSettingValues(url2);
    ExpectRevokedAbusiveNotificationSettingValues(url3);
    ExpectSafeNotificationSettingValues(url4);
  } else {
    ExpectRevokedAbusiveNotificationPermissionSize(0U);
  }

  // Travel through time for 20 days.
  clock()->Advance(base::Days(20));
  base::Time future = clock()->Now();

  // The old settings should now be tracked as unused.
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 4u);
    EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);
    // Visit `url2` and check that the corresponding content setting got
    // updated.
    UnusedSitePermissionsService::TabHelper::CreateForWebContents(
        web_contents(), service());
  }
  NavigateAndCommit(GURL(url2));
  if (ShouldSetupUnusedSites()) {
    EXPECT_LE(GetLastVisitedDate(GURL(url1), geolocation_type), now);
    EXPECT_GE(GetLastVisitedDate(GURL(url1), geolocation_type),
              now - precision);
    EXPECT_LE(GetLastVisitedDate(GURL(url2), geolocation_type), future);
    EXPECT_GE(GetLastVisitedDate(GURL(url2), geolocation_type),
              future - precision);
    EXPECT_LE(GetLastVisitedDate(GURL(url2), mediastream_type), future);
    EXPECT_GE(GetLastVisitedDate(GURL(url2), mediastream_type),
              future - precision);
    EXPECT_LE(GetLastVisitedDate(GURL(url2), chooser_type), future);
    EXPECT_GE(GetLastVisitedDate(GURL(url2), chooser_type), future - precision);

    // Check that the service is only tracking one entry now.
    EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 1u);
  }

  // Travel through time for 50 days to make permissions be revoked.
  clock()->Advance(base::Days(50));

  // Unused permissions should be auto revoked.
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::RunLoop wait_for_ukm_loop;
  ukm_recorder.SetOnAddEntryCallback(ukm::builders::Permission::kEntryName,
                                     wait_for_ukm_loop.QuitClosure());

  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());

  if (ShouldSetupUnusedSites()) {
    // url2 should be on tracked permissions list.
    EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 3u);
    EXPECT_EQ(url2, service()
                        ->GetTrackedUnusedPermissionsForTesting()[0]
                        .source.primary_pattern.ToString());
    EXPECT_EQ(url2, service()
                        ->GetTrackedUnusedPermissionsForTesting()[1]
                        .source.primary_pattern.ToString());
    EXPECT_EQ(url2, service()
                        ->GetTrackedUnusedPermissionsForTesting()[2]
                        .source.primary_pattern.ToString());
    // `url1` should be on revoked permissions list.
    EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 1u);
    EXPECT_EQ(
        url1,
        GetRevokedUnusedPermissions(hcsm())[0].primary_pattern.ToString());

    // Revocation related histograms should be recorded for the revoked
    // geolocation grant, but nothing for other permission types.
    histogram_tester.ExpectUniqueSample(
        "Permissions.Action.Geolocation",
        static_cast<int>(permissions::PermissionAction::REVOKED), 1);
    histogram_tester.ExpectTotalCount(
        "Permissions.Revocation.ElapsedTimeSinceGrant.Geolocation", 1);
    EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("Permissions.Action."),
                testing::SizeIs(1));
    EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                    "Permissions.Revocation.ElapsedTimeSinceGrant."),
                testing::SizeIs(1));

    // Revocation UKM events should be emitted as well, and it takes a round
    // trip to the HistoryService, so wait for it.
    wait_for_ukm_loop.Run();

    auto entries = ukm_recorder.GetEntriesByName("Permission");
    ASSERT_EQ(1u, entries.size());
    ukm_recorder.ExpectEntrySourceHasUrl(entries[0], GURL(url1));
    ukm_recorder.ExpectEntryMetric(
        entries[0], "Source",
        static_cast<int64_t>(
            permissions::PermissionSourceUI::SAFETY_HUB_AUTO_REVOCATION));
  }
  if (ShouldSetupAbusiveNotificationSites()) {
    // Revoked abusive notification permissions should all be cleaned up.
    EXPECT_EQ(safety_hub_util::GetRevokedAbusiveNotificationPermissions(hcsm())
                  .size(),
              0u);
    ExpectCleanedUpAbusiveNotificationSettingValues(url2);
    ExpectCleanedUpAbusiveNotificationSettingValues(url3);
    ExpectSafeNotificationSettingValues(url4);
  }
}

TEST_P(UnusedSitePermissionsServiceTest, TrackOnlySingleOriginTest) {
  std::string example_url1 = "https://example1.com";
  std::string example_url2 = "https://[*.]example2.com";
  std::string example_url3 = "file:///foo/bar.txt";
  // Add one setting for all urls.
  SetTrackedContentSettingForType(example_url1, geolocation_type);
  SetTrackedContentSettingForType(example_url2, geolocation_type);
  // TODO(crbug.com/40267370): The first parameter should be `example_url3`,
  // but the test crashes.
  hcsm()->SetContentSettingDefaultScope(GURL(example_url2), GURL(example_url3),
                                        geolocation_type,
                                        ContentSetting::CONTENT_SETTING_ALLOW);

  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 0u);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // Travel through time for 20 days.
  clock()->Advance(base::Days(20));

  // Only `url1` should be tracked because it is the only single origin url.
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 1u);
  auto tracked_origin = service()->GetTrackedUnusedPermissionsForTesting()[0];
  EXPECT_EQ(GURL(tracked_origin.source.primary_pattern.ToString()),
            GURL(example_url1));
}

TEST_P(UnusedSitePermissionsServiceTest, TrackUnusedButDontRevoke) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      content_settings::features::kSafetyCheckUnusedSitePermissions);
  SetTrackedContentSettingForType(url1, geolocation_type,
                                  ContentSetting::CONTENT_SETTING_BLOCK);

  // Travel through time for 20 days.
  clock()->Advance(base::Days(20));

  // GEOLOCATION permission should be on the tracked unused site permissions
  // list as it is denied 20 days before. The permission is not suitable for
  // revocation and this test verifies that RevokeUnusedPermissions() does not
  // enter infinite loop in such case.
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  auto unused_permissions = service()->GetTrackedUnusedPermissionsForTesting();
  ASSERT_EQ(unused_permissions.size(), 1u);
  EXPECT_EQ(unused_permissions[0].type, geolocation_type);
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), GURL(url1)).size(), 0u);
}

TEST_P(UnusedSitePermissionsServiceTest, SecondaryPatternAlwaysWildcard) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      content_settings::features::kSafetyCheckUnusedSitePermissions);

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

  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // Travel through time for 70 days so that permissions are revoked.
  clock()->Advance(base::Days(70));
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());

  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 4u);
  for (auto unused_permission : GetRevokedUnusedPermissions(hcsm())) {
    EXPECT_EQ(unused_permission.secondary_pattern,
              ContentSettingsPattern::Wildcard());
  }
}

TEST_P(UnusedSitePermissionsServiceTest, MultipleRevocationsForSameOrigin) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      content_settings::features::kSafetyCheckUnusedSitePermissions);

  // Grant GEOLOCATION permission for the url.
  SetTrackedContentSettingForType(url1, geolocation_type);
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 0u);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // Travel through time for 20 days.
  clock()->Advance(base::Days(20));

  // Grant MEDIASTREAM_CAMERA permission for the url.
  SetTrackedContentSettingForType(url1, mediastream_type);

  // GEOLOCATION permission should be on the tracked unused site permissions
  // list as it is granted 20 days before. MEDIASTREAM_CAMERA permission should
  // not be tracked as it is just granted.
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 1u);
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting()[0].type,
            geolocation_type);

  // Travel through time for 50 days.
  clock()->Advance(base::Days(50));

  // GEOLOCATION permission should be on the revoked permissions list as it is
  // granted 70 days before. MEDIASTREAM_CAMERA permission should be on the
  // recently unused permissions list as it is granted 50 days before.
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), GURL(url1)).size(), 1u);
  EXPECT_EQ(
      UnusedSitePermissionsService::ConvertKeyToContentSettingsType(
          GetRevokedPermissionsForOneOrigin(hcsm(), GURL(url1))[0].GetString()),
      geolocation_type);
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 1u);
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting()[0].type,
            mediastream_type);
}

// TODO(crbug.com/40928115): Flaky on all platforms.
TEST_P(UnusedSitePermissionsServiceTest,
       DISABLED_ClearRevokedPermissionsListAfter30d) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      content_settings::features::kSafetyCheckUnusedSitePermissions);

  SetTrackedContentSettingForType(url1, geolocation_type);
  SetTrackedContentSettingForType(url1, mediastream_type);
  SetTrackedChooserType(url1);

  // Travel through time for 70 days.
  clock()->Advance(base::Days(70));

  // Both GEOLOCATION and MEDIASTREAM_CAMERA permissions should be on the
  // revoked permissions list as they are granted more than 60 days before.
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), GURL(url1)).size(), 3u);
  EXPECT_EQ(
      UnusedSitePermissionsService::ConvertKeyToContentSettingsType(
          GetRevokedPermissionsForOneOrigin(hcsm(), GURL(url1))[0].GetString()),
      geolocation_type);
  EXPECT_EQ(
      UnusedSitePermissionsService::ConvertKeyToContentSettingsType(
          GetRevokedPermissionsForOneOrigin(hcsm(), GURL(url1))[1].GetString()),
      mediastream_type);
  EXPECT_EQ(
      UnusedSitePermissionsService::ConvertKeyToContentSettingsType(
          GetRevokedPermissionsForOneOrigin(hcsm(), GURL(url1))[2].GetString()),
      chooser_type);

  // Travel through time for 30 days.
  clock()->Advance(base::Days(30));

  // No permission should be on the revoked permissions list as they are revoked
  // more than 30 days before.
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), GURL(url1)).size(), 0u);
}

TEST_P(UnusedSitePermissionsServiceTest, RegrantPermissionsForOrigin) {
  if (ShouldSetupAbusiveNotificationSites()) {
    SetupAbusiveNotificationSite(url2, ContentSetting::CONTENT_SETTING_ASK);
    SetupAbusiveNotificationSite(url3, ContentSetting::CONTENT_SETTING_ASK);
    SetupRevokedAbusiveNotificationSite(url2);
    SetupRevokedAbusiveNotificationSite(url3);
    ExpectRevokedAbusiveNotificationPermissionSize(2U);
  }
  if (ShouldSetupUnusedSites()) {
    SetupRevokedUnusedPermissionSite(url1);
    SetupRevokedUnusedPermissionSite(url2);
    EXPECT_EQ(2U, GetRevokedUnusedPermissions(hcsm()).size());
  }

  // Allow the permission for `url1` again, which is unused.
  service()->RegrantPermissionsForOrigin(url::Origin::Create(GURL(url1)));
  if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(1U, GetRevokedUnusedPermissions(hcsm()).size());
    // Check if the permissions of `url1` is regranted.
    EXPECT_EQ(
        ContentSetting::CONTENT_SETTING_ALLOW,
        hcsm()->GetContentSetting(GURL(url1), GURL(url1), geolocation_type));
    EXPECT_EQ(base::Value::Dict().Set("foo", "bar"),
              hcsm()->GetWebsiteSetting(GURL(url1), GURL(url1), chooser_type));
  }
  if (ShouldSetupAbusiveNotificationSites()) {
    ExpectRevokedAbusiveNotificationPermissionSize(2U);
    ExpectRevokedAbusiveNotificationSettingValues(url2);
    ExpectRevokedAbusiveNotificationSettingValues(url3);
  }

  // Allow the permission for `url2`, which is both abusive and unused.
  service()->RegrantPermissionsForOrigin(url::Origin::Create(GURL(url2)));
  if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(0U, GetRevokedUnusedPermissions(hcsm()).size());
    // Check if the permissions of `url2` is regranted.
    EXPECT_EQ(
        ContentSetting::CONTENT_SETTING_ALLOW,
        hcsm()->GetContentSetting(GURL(url2), GURL(url2), geolocation_type));
    EXPECT_EQ(base::Value::Dict().Set("foo", "bar"),
              hcsm()->GetWebsiteSetting(GURL(url2), GURL(url2), chooser_type));
  }
  if (ShouldSetupAbusiveNotificationSites()) {
    ExpectRevokedAbusiveNotificationPermissionSize(1U);
    ExpectCleanedUpAbusiveNotificationSettingValues(
        url2, /*ignore_future_revocation=*/true);
    ExpectRevokedAbusiveNotificationSettingValues(url3);
  }

  // Allow the permission for `url3`, which is abusive.
  service()->RegrantPermissionsForOrigin(url::Origin::Create(GURL(url3)));
  if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(0U, GetRevokedUnusedPermissions(hcsm()).size());
  }
  if (ShouldSetupAbusiveNotificationSites()) {
    ExpectRevokedAbusiveNotificationPermissionSize(0U);
    ExpectCleanedUpAbusiveNotificationSettingValues(
        url2, /*ignore_future_revocation=*/true);
    ExpectCleanedUpAbusiveNotificationSettingValues(
        url3, /*ignore_future_revocation=*/true);
  }

  // Undoing the changes should add `url1` back to the list of revoked
  // permissions and reset its permissions.
  UndoRegrantPermissionsForUrl(url1, unused_permission_types);
  if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(1U, GetRevokedUnusedPermissions(hcsm()).size());
    EXPECT_EQ(
        ContentSetting::CONTENT_SETTING_ASK,
        hcsm()->GetContentSetting(GURL(url1), GURL(url1), geolocation_type));
    EXPECT_EQ(base::Value(),
              hcsm()->GetWebsiteSetting(GURL(url1), GURL(url1), chooser_type));
  }
  if (ShouldSetupAbusiveNotificationSites()) {
    ExpectRevokedAbusiveNotificationPermissionSize(0U);
    ExpectCleanedUpAbusiveNotificationSettingValues(
        url2, /*ignore_future_revocation=*/true);
    ExpectCleanedUpAbusiveNotificationSettingValues(
        url3, /*ignore_future_revocation=*/true);
  }

  // Undoing `url2` adds it back to the revoked permissions lists.
  UndoRegrantPermissionsForUrl(url2, abusive_and_unused_permission_types);
  if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(2U, GetRevokedUnusedPermissions(hcsm()).size());
    EXPECT_EQ(
        ContentSetting::CONTENT_SETTING_ASK,
        hcsm()->GetContentSetting(GURL(url2), GURL(url2), geolocation_type));
    EXPECT_EQ(base::Value(),
              hcsm()->GetWebsiteSetting(GURL(url2), GURL(url2), chooser_type));
  }
  if (ShouldSetupAbusiveNotificationSites()) {
    ExpectRevokedAbusiveNotificationPermissionSize(1U);
    ExpectRevokedAbusiveNotificationSettingValues(url2);
    ExpectCleanedUpAbusiveNotificationSettingValues(
        url3, /*ignore_future_revocation=*/true);
  }

  // Undoing `url3` adds it back to the revoked abusive notification permissions
  // list.
  UndoRegrantPermissionsForUrl(url3, abusive_permission_types);
  if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(2U, GetRevokedUnusedPermissions(hcsm()).size());
    EXPECT_EQ(
        ContentSetting::CONTENT_SETTING_ASK,
        hcsm()->GetContentSetting(GURL(url3), GURL(url3), geolocation_type));
    EXPECT_EQ(base::Value(),
              hcsm()->GetWebsiteSetting(GURL(url3), GURL(url3), chooser_type));
  }
  if (ShouldSetupAbusiveNotificationSites()) {
    ExpectRevokedAbusiveNotificationPermissionSize(2U);
    ExpectRevokedAbusiveNotificationSettingValues(url2);
    ExpectRevokedAbusiveNotificationSettingValues(url3);
  }
}

TEST_P(UnusedSitePermissionsServiceTest, RegrantPreventsAutorevoke) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      content_settings::features::kSafetyCheckUnusedSitePermissions);

  if (ShouldSetupUnusedSites()) {
    SetTrackedContentSettingForType(url1, geolocation_type);
    SetTrackedContentSettingForType(url2, geolocation_type);
  }
  EXPECT_EQ(0U, GetRevokedUnusedPermissions(hcsm()).size());

  // Travel 70 days through time so that the granted permission is revoked.
  clock()->Advance(base::Days(70));
  if (ShouldSetupAbusiveNotificationSites()) {
    SetupAbusiveNotificationSite(url2, ContentSetting::CONTENT_SETTING_ALLOW);
    SetupAbusiveNotificationSite(url3, ContentSetting::CONTENT_SETTING_ALLOW);
  }
  ExpectRevokedAbusiveNotificationPermissionSize(0U);

  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(2U, GetRevokedUnusedPermissions(hcsm()).size());
  }
  if (ShouldSetupAbusiveNotificationSites()) {
    ExpectRevokedAbusiveNotificationPermissionSize(2U);
    ExpectRevokedAbusiveNotificationSettingValues(url2);
    ExpectRevokedAbusiveNotificationSettingValues(url3);
  }

  // After regranting permissions they are not revoked again even after >60 days
  // pass.
  service()->RegrantPermissionsForOrigin(url::Origin::Create(GURL(url1)));
  service()->RegrantPermissionsForOrigin(url::Origin::Create(GURL(url2)));
  service()->RegrantPermissionsForOrigin(url::Origin::Create(GURL(url3)));
  if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(0U, GetRevokedUnusedPermissions(hcsm()).size());
  }
  if (ShouldSetupAbusiveNotificationSites()) {
    ExpectRevokedAbusiveNotificationPermissionSize(0U);
    ExpectCleanedUpAbusiveNotificationSettingValues(
        url2, /*ignore_future_revocation=*/true);
    ExpectCleanedUpAbusiveNotificationSettingValues(
        url3, /*ignore_future_revocation=*/true);
  }

  clock()->Advance(base::Days(70));
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(0U, GetRevokedUnusedPermissions(hcsm()).size());
  }
  if (ShouldSetupAbusiveNotificationSites()) {
    ExpectRevokedAbusiveNotificationPermissionSize(0U);
  }
}

TEST_P(UnusedSitePermissionsServiceTest, UndoRegrantPermissionsForOrigin) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      content_settings::features::kSafetyCheckUnusedSitePermissions);

  SetTrackedContentSettingForType(url1, geolocation_type);
  SetTrackedChooserType(url1);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // Travel 70 days through time so that the granted permission is revoked.
  clock()->Advance(base::Days(70));
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 1u);
  const ContentSettingPatternSource revoked_permission =
      GetRevokedUnusedPermissions(hcsm())[0];

  // Permission remains revoked after regrant and undo.
  service()->RegrantPermissionsForOrigin(url::Origin::Create(GURL(url1)));
  UndoRegrantPermissionsForUrl(url1, unused_permission_types,
                               revoked_permission.metadata.expiration(),
                               revoked_permission.metadata.lifetime());
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 1u);

  // Revoked permission is cleaned up after >30 days.
  clock()->Advance(base::Days(40));
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // If that permission is granted again, it will still be autorevoked.
  SetTrackedContentSettingForType(url1, geolocation_type);
  SetTrackedChooserType(url1);
  clock()->Advance(base::Days(70));
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 1u);
}

TEST_P(UnusedSitePermissionsServiceTest, NotRevokeNotificationPermission) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      content_settings::features::kSafetyCheckUnusedSitePermissions);

  // Grant GEOLOCATION and NOTIFICATION permission for the url.
  SetTrackedContentSettingForType(url1, geolocation_type);
  hcsm()->SetContentSettingDefaultScope(GURL(url1), GURL(url1),
                                        notifications_type,
                                        ContentSetting::CONTENT_SETTING_ALLOW);
  EXPECT_EQ(service()->GetTrackedUnusedPermissionsForTesting().size(), 0u);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm()).size(), 0u);

  // Travel through time for 70 days.
  clock()->Advance(base::Days(70));

  // GEOLOCATION permission should be on the revoked permissions list, but
  // NOTIFICATION permissions should not be as notification permissions are out
  // of scope.
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), GURL(url1)).size(), 1u);
  EXPECT_EQ(
      UnusedSitePermissionsService::ConvertKeyToContentSettingsType(
          GetRevokedPermissionsForOneOrigin(hcsm(), GURL(url1))[0].GetString()),
      geolocation_type);

  // Clearing revoked permissions list should delete unused GEOLOCATION from it
  // but leave used NOTIFICATION permissions intact.
  service()->ClearRevokedPermissionsList();
  EXPECT_EQ(GetRevokedPermissionsForOneOrigin(hcsm(), GURL(url1)).size(), 0u);
  EXPECT_EQ(hcsm()->GetContentSetting(GURL(url1), GURL(url1), geolocation_type),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_EQ(
      hcsm()->GetContentSetting(GURL(url1), GURL(url1), notifications_type),
      ContentSetting::CONTENT_SETTING_ALLOW);
}

TEST_P(UnusedSitePermissionsServiceTest, ClearRevokedPermissionsList) {
  if (ShouldSetupAbusiveNotificationSites()) {
    SetupAbusiveNotificationSite(url2, ContentSetting::CONTENT_SETTING_ASK);
    SetupAbusiveNotificationSite(url3, ContentSetting::CONTENT_SETTING_ASK);
    SetupRevokedAbusiveNotificationSite(url2);
    SetupRevokedAbusiveNotificationSite(url3);
    ExpectRevokedAbusiveNotificationPermissionSize(2U);
  }
  if (ShouldSetupUnusedSites()) {
    SetupRevokedUnusedPermissionSite(url1);
    SetupRevokedUnusedPermissionSite(url2);
    EXPECT_EQ(2U, GetRevokedUnusedPermissions(hcsm()).size());
  }

  // Revoked permissions list should be empty after clearing the revoked
  // permissions list.
  service()->ClearRevokedPermissionsList();
  EXPECT_EQ(0U, GetRevokedUnusedPermissions(hcsm()).size());
  ExpectRevokedAbusiveNotificationPermissionSize(0U);
}

TEST_P(UnusedSitePermissionsServiceTest, RecordRegrantMetricForAllowAgain) {
  SetupRevokedUnusedPermissionSite(url1);
  SetupRevokedUnusedPermissionSite(url2);
  EXPECT_EQ(2U, GetRevokedUnusedPermissions(hcsm()).size());

  // Advance 14 days; this will be the expected histogram sample.
  clock()->Advance(base::Days(14));
  base::HistogramTester histogram_tester;

  // Allow the permission for `url` again
  service()->RegrantPermissionsForOrigin(url::Origin::Create(GURL(url1)));

  // Only a single entry should be recorded in the histogram.
  const std::vector<base::Bucket> buckets = histogram_tester.GetAllSamples(
      "Settings.SafetyCheck.UnusedSitePermissionsAllowAgainDays");
  EXPECT_EQ(1U, buckets.size());
  // The recorded metric should be the elapsed days since the revocation.
  histogram_tester.ExpectUniqueSample(
      "Settings.SafetyCheck.UnusedSitePermissionsAllowAgainDays", 14, 1);
}

TEST_P(UnusedSitePermissionsServiceTest,
       RemoveSiteFromRevokedPermissionsListOnPermissionChange) {
  if (ShouldSetupAbusiveNotificationSites()) {
    SetupAbusiveNotificationSite(url2, ContentSetting::CONTENT_SETTING_ASK);
    SetupAbusiveNotificationSite(url3, ContentSetting::CONTENT_SETTING_ASK);
    SetupRevokedAbusiveNotificationSite(url2);
    SetupRevokedAbusiveNotificationSite(url3);
    ExpectRevokedAbusiveNotificationPermissionSize(2U);
  }
  if (ShouldSetupUnusedSites()) {
    SetupRevokedUnusedPermissionSite(url1);
    SetupRevokedUnusedPermissionSite(url3);
    SetupRevokedUnusedPermissionSite(url4);
    EXPECT_EQ(3U, GetRevokedUnusedPermissions(hcsm()).size());
  }

  // For a site where permissions have been revoked, granting a revoked
  // permission again for `url1` will remove the site from the list of revoked
  // unused sites.
  hcsm()->SetContentSettingDefaultScope(GURL(url1), GURL(), geolocation_type,
                                        CONTENT_SETTING_ALLOW);
  if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(2U, GetRevokedUnusedPermissions(hcsm()).size());
  }
  if (ShouldSetupAbusiveNotificationSites()) {
    ExpectRevokedAbusiveNotificationPermissionSize(2U);
  }

  // If we grant a revoked permission again for `url2`, it will be removed the
  // list of revoked abusive and unused sites.
  hcsm()->SetContentSettingDefaultScope(
      GURL(url2), GURL(url2), notifications_type, CONTENT_SETTING_ALLOW);
  if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(2U, GetRevokedUnusedPermissions(hcsm()).size());
  }
  if (ShouldSetupAbusiveNotificationSites()) {
    ExpectRevokedAbusiveNotificationPermissionSize(1U);
    ExpectCleanedUpAbusiveNotificationSettingValues(
        url2, /*ignore_future_revocation=*/true);
    ExpectRevokedAbusiveNotificationSettingValues(url3);
  }

  // If we grant revoked unused permission again for `url3`, it will be removed
  // the list of revoked abusive and unused sites.
  hcsm()->SetContentSettingDefaultScope(
      GURL(url3), GURL(url3), geolocation_type, CONTENT_SETTING_ALLOW);
  if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(1U, GetRevokedUnusedPermissions(hcsm()).size());
  }
  if (ShouldSetupAbusiveNotificationSites()) {
    ExpectRevokedAbusiveNotificationPermissionSize(0U);
    ExpectCleanedUpAbusiveNotificationSettingValues(
        url3, /*ignore_future_revocation=*/false);
  }

  // Grant the revoked chooser permissions again from url5, and check that
  // the revoked permission list is empty.
  if (ShouldSetupUnusedSites()) {
    hcsm()->SetWebsiteSettingDefaultScope(
        GURL(url4), GURL(), chooser_type,
        base::Value(base::Value::Dict().Set("foo", "baz")));
    EXPECT_EQ(0U, GetRevokedUnusedPermissions(hcsm()).size());
  }
}

TEST_P(UnusedSitePermissionsServiceTest, InitializeLatestResult) {
  if (ShouldSetupAbusiveNotificationSites()) {
    SetupAbusiveNotificationSite(url2, ContentSetting::CONTENT_SETTING_ASK);
    SetupAbusiveNotificationSite(url3, ContentSetting::CONTENT_SETTING_ASK);
    SetupRevokedAbusiveNotificationSite(url2);
    SetupRevokedAbusiveNotificationSite(url3);
  }
  if (ShouldSetupUnusedSites()) {
    SetupRevokedUnusedPermissionSite(url1);
    SetupRevokedUnusedPermissionSite(url2);
  }

  // When we start up a new service instance, the latest result (i.e. the list
  // of revoked permissions) should be immediately available.
  auto new_service = std::make_unique<UnusedSitePermissionsService>(
      profile(), profile()->GetPrefs());
  std::optional<std::unique_ptr<SafetyHubService::Result>> opt_result =
      new_service->GetCachedResult();
  EXPECT_TRUE(opt_result.has_value());
  auto* result =
      static_cast<UnusedSitePermissionsService::UnusedSitePermissionsResult*>(
          opt_result.value().get());
  if (ShouldSetupUnusedSites() && ShouldSetupAbusiveNotificationSites()) {
    EXPECT_EQ(3U, result->GetRevokedPermissions().size());
    EXPECT_TRUE(IsUrlInRevokedSettings(result->GetRevokedPermissions(), url1));
    EXPECT_TRUE(IsUrlInRevokedSettings(result->GetRevokedPermissions(), url2));
    EXPECT_TRUE(IsUrlInRevokedSettings(result->GetRevokedPermissions(), url3));
  } else if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(2U, result->GetRevokedPermissions().size());
    EXPECT_TRUE(IsUrlInRevokedSettings(result->GetRevokedPermissions(), url1));
    EXPECT_TRUE(IsUrlInRevokedSettings(result->GetRevokedPermissions(), url2));
  } else if (ShouldSetupAbusiveNotificationSites()) {
    EXPECT_EQ(2U, result->GetRevokedPermissions().size());
    EXPECT_TRUE(IsUrlInRevokedSettings(result->GetRevokedPermissions(), url2));
    EXPECT_TRUE(IsUrlInRevokedSettings(result->GetRevokedPermissions(), url3));
  }
}

TEST_P(UnusedSitePermissionsServiceTest, ResultToFromDict) {
  auto result = std::make_unique<
      UnusedSitePermissionsService::UnusedSitePermissionsResult>();
  // This is necessary for revoked abusive notification permissions, since
  // checking URLs is asynchronous.
  base::RunLoop().RunUntilIdle();
  if (ShouldSetupUnusedSites()) {
    AddRevokedPermissionToResult(result.get(), unused_permission_types, url1);
    if (ShouldSetupAbusiveNotificationSites()) {
      AddRevokedPermissionToResult(result.get(),
                                   abusive_and_unused_permission_types, url2);
    } else {
      AddRevokedPermissionToResult(result.get(), unused_permission_types, url2);
    }
  }
  if (ShouldSetupAbusiveNotificationSites()) {
    if (!ShouldSetupUnusedSites()) {
      AddRevokedPermissionToResult(result.get(), abusive_permission_types,
                                   url2);
    }
    AddRevokedPermissionToResult(result.get(), abusive_permission_types, url3);
  }

  if (ShouldSetupUnusedSites() && ShouldSetupAbusiveNotificationSites()) {
    EXPECT_EQ(3U, result->GetRevokedPermissions().size());
    EXPECT_EQ(ContentSettingsPattern::FromString(url1),
              result->GetRevokedPermissions().front().primary_pattern);
    EXPECT_TRUE(IsUrlInRevokedSettings(result->GetRevokedPermissions(), url1));
    EXPECT_TRUE(IsUrlInRevokedSettings(result->GetRevokedPermissions(), url2));
    EXPECT_TRUE(IsUrlInRevokedSettings(result->GetRevokedPermissions(), url3));
  } else if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(2U, result->GetRevokedPermissions().size());
    EXPECT_TRUE(IsUrlInRevokedSettings(result->GetRevokedPermissions(), url1));
    EXPECT_TRUE(IsUrlInRevokedSettings(result->GetRevokedPermissions(), url2));
  } else if (ShouldSetupAbusiveNotificationSites()) {
    EXPECT_EQ(2U, result->GetRevokedPermissions().size());
    EXPECT_TRUE(IsUrlInRevokedSettings(result->GetRevokedPermissions(), url2));
    EXPECT_TRUE(IsUrlInRevokedSettings(result->GetRevokedPermissions(), url3));
  }

  // When converting to dict, the values of the revoked permissions should be
  // correctly converted to base::Value.
  base::Value::Dict dict = result->ToDictValue();
  auto* revoked_origins_list = dict.FindList(kUnusedSitePermissionsResultKey);
  if (ShouldSetupUnusedSites() && ShouldSetupAbusiveNotificationSites()) {
    EXPECT_EQ(3U, revoked_origins_list->size());
    EXPECT_EQ(url1, revoked_origins_list->front().GetString());
    EXPECT_TRUE(base::Contains(*revoked_origins_list, url1));
    EXPECT_TRUE(base::Contains(*revoked_origins_list, url2));
    EXPECT_TRUE(base::Contains(*revoked_origins_list, url3));
  } else if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(2U, revoked_origins_list->size());
    EXPECT_TRUE(base::Contains(*revoked_origins_list, url1));
    EXPECT_TRUE(base::Contains(*revoked_origins_list, url2));
  } else if (ShouldSetupAbusiveNotificationSites()) {
    EXPECT_EQ(2U, revoked_origins_list->size());
    EXPECT_TRUE(base::Contains(*revoked_origins_list, url2));
    EXPECT_TRUE(base::Contains(*revoked_origins_list, url3));
  }
}

TEST_P(UnusedSitePermissionsServiceTest, ResultGetRevokedOrigins) {
  auto result = std::make_unique<
      UnusedSitePermissionsService::UnusedSitePermissionsResult>();
  EXPECT_EQ(0U, result->GetRevokedOrigins().size());
  AddRevokedPermissionToResult(result.get(), unused_permission_types, url1);
  EXPECT_EQ(1U, result->GetRevokedOrigins().size());
  EXPECT_EQ(ContentSettingsPattern::FromString(url1),
            *result->GetRevokedOrigins().begin());
  AddRevokedPermissionToResult(result.get(), unused_permission_types, url2);
  EXPECT_EQ(2U, result->GetRevokedOrigins().size());
  EXPECT_TRUE(result->GetRevokedOrigins().contains(
      ContentSettingsPattern::FromString(url1)));
  EXPECT_TRUE(result->GetRevokedOrigins().contains(
      ContentSettingsPattern::FromString(url2)));

  // Adding another permission type to `url2` does not change the size of the
  // revoked origin list.
  std::set<ContentSettingsType> permission_types({mediastream_type});
  auto origin = ContentSettingsPattern::FromString(url2);
  result->AddRevokedPermission(CreatePermissionsData(origin, permission_types));
  EXPECT_EQ(2U, result->GetRevokedOrigins().size());
}

TEST_P(UnusedSitePermissionsServiceTest, ResultIsTriggerForMenuNotification) {
  auto result = std::make_unique<
      UnusedSitePermissionsService::UnusedSitePermissionsResult>();
  EXPECT_FALSE(result->IsTriggerForMenuNotification());
  AddRevokedPermissionToResult(result.get(), unused_permission_types, url1);
  EXPECT_TRUE(result->IsTriggerForMenuNotification());
}

TEST_P(UnusedSitePermissionsServiceTest, ResultWarrantsNewMenuNotification) {
  auto old_result = std::make_unique<
      UnusedSitePermissionsService::UnusedSitePermissionsResult>();
  auto new_result = std::make_unique<
      UnusedSitePermissionsService::UnusedSitePermissionsResult>();
  EXPECT_FALSE(
      new_result->WarrantsNewMenuNotification(old_result->ToDictValue()));
  // origin1 revoked in new, but not in old -> warrants notification
  AddRevokedPermissionToResult(new_result.get(), unused_permission_types, url1);
  EXPECT_TRUE(
      new_result->WarrantsNewMenuNotification(old_result->ToDictValue()));
  // origin1 in both new and old -> no notification
  AddRevokedPermissionToResult(old_result.get(), unused_permission_types, url1);
  EXPECT_FALSE(
      new_result->WarrantsNewMenuNotification(old_result->ToDictValue()));
  // origin1 in both, origin2 in new -> warrants notification
  AddRevokedPermissionToResult(new_result.get(), unused_permission_types, url2);
  EXPECT_TRUE(
      new_result->WarrantsNewMenuNotification(old_result->ToDictValue()));
  // origin1 and origin2 in both new and old -> no notification
  AddRevokedPermissionToResult(old_result.get(), unused_permission_types, url2);
  EXPECT_FALSE(
      new_result->WarrantsNewMenuNotification(old_result->ToDictValue()));
}

TEST_P(UnusedSitePermissionsServiceTest, AutoRevocationSetting) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitWithFeatureStates(
      {{content_settings::features::kSafetyCheckUnusedSitePermissions, false}});

  // When auto-revocation is on, the timer is started by
  // StartRepeatedUpdates() on start-up.
  ResetService();
  EXPECT_TRUE(service()->IsTimerRunningForTesting());

  // Disable auto-revocation by setting kUnusedSitePermissionsRevocationEnabled
  // pref to false and turning off safe browsing. This should stop the repeated
  // timer.
  prefs()->SetBoolean(safety_hub_prefs::kUnusedSitePermissionsRevocationEnabled,
                      false);
  prefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  EXPECT_FALSE(service()->IsTimerRunningForTesting());

  // Reset the service so auto-revocation is off on the service creation. The
  // repeated timer is not started on service creation in this case.
  ResetService();
  EXPECT_FALSE(service()->IsTimerRunningForTesting());

  // Enable auto-revocation by setting kUnusedSitePermissionsRevocationEnabled
  // pref to true. This should restart the repeated timer.
  if (ShouldSetupAbusiveNotificationSites()) {
    prefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  }
  if (ShouldSetupUnusedSites()) {
    prefs()->SetBoolean(
        safety_hub_prefs::kUnusedSitePermissionsRevocationEnabled, true);
  }
  if (ShouldSetupAbusiveNotificationSites() || ShouldSetupUnusedSites()) {
    EXPECT_TRUE(service()->IsTimerRunningForTesting());
  } else {
    EXPECT_FALSE(service()->IsTimerRunningForTesting());
  }
}

TEST_P(UnusedSitePermissionsServiceTest, AutoCleanupRevokedPermissions) {
  if (ShouldSetupUnusedSites()) {
    // Add one content setting for `url1` and two content settings +
    // one website setting for `url2`.
    SetTrackedContentSettingForType(url1, geolocation_type);
    SetTrackedContentSettingForType(url2, geolocation_type);
    SetTrackedChooserType(url2);
  }

  // Fast forward 50 days then maybe setup abusive notifications.
  clock()->Advance(base::Days(50));
  if (ShouldSetupAbusiveNotificationSites()) {
    // Add notifications setting for `url2` and `url3`, abusive notification
    // sites.
    SetupAbusiveNotificationSite(url2, ContentSetting::CONTENT_SETTING_ALLOW);
    SetupAbusiveNotificationSite(url3, ContentSetting::CONTENT_SETTING_ALLOW);
  }

  // Abusive notifications should be revoked, but not unused sites yet.
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  if (ShouldSetupAbusiveNotificationSites()) {
    ExpectRevokedAbusiveNotificationPermissionSize(2U);
  }
  if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(0U, GetRevokedUnusedPermissions(hcsm()).size());
  }

  // Fast forwarding 20 days then performing check should revoked unused site
  // permissions.
  clock()->Advance(base::Days(20));
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  if (ShouldSetupAbusiveNotificationSites()) {
    ExpectRevokedAbusiveNotificationPermissionSize(2U);
  }
  if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(2U, GetRevokedUnusedPermissions(hcsm()).size());
  }

  // Fast forwarding 20 days should cleanup abusive sites, but not yet unused
  // sites.
  clock()->Advance(base::Days(20));
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  if (ShouldSetupAbusiveNotificationSites()) {
    ExpectRevokedAbusiveNotificationPermissionSize(0U);
  }
  if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(2U, GetRevokedUnusedPermissions(hcsm()).size());
  }

  // Fast forwarding 20 days should cleanup unused sites.
  clock()->Advance(base::Days(20));
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  if (ShouldSetupAbusiveNotificationSites()) {
    ExpectRevokedAbusiveNotificationPermissionSize(0U);
  }
  if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(0U, GetRevokedUnusedPermissions(hcsm()).size());
  }
}

TEST_P(UnusedSitePermissionsServiceTest,
       ChangingSettingOnRevokedSettingClearsIt) {
  if (ShouldSetupUnusedSites()) {
    // Add one content setting for `url1` and two content settings +
    // one website setting for `url2`.
    SetTrackedContentSettingForType(url1, geolocation_type);
    SetTrackedContentSettingForType(url2, geolocation_type);
    SetTrackedChooserType(url2);
  }

  // Fast forward 70 days will revoke any unused site permissions.
  clock()->Advance(base::Days(70));
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(2U, GetRevokedUnusedPermissions(hcsm()).size());
  }

  // Fast forward 20 days. Setting up abusive site permissions will cause
  // auto-revocation and revoked unused sites will still be in the list.
  clock()->Advance(base::Days(20));
  if (ShouldSetupAbusiveNotificationSites()) {
    // Add notifications setting for `url2` and `url3`, abusive notification
    // sites.
    SetupAbusiveNotificationSite(url2, ContentSetting::CONTENT_SETTING_ALLOW);
    SetupAbusiveNotificationSite(url3, ContentSetting::CONTENT_SETTING_ALLOW);
  }
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  if (ShouldSetupUnusedSites()) {
    // If notifications were set up for `url2`, then remove it from the list of
    // revoked unused permissions.
    if (ShouldSetupAbusiveNotificationSites()) {
      EXPECT_EQ(1U, GetRevokedUnusedPermissions(hcsm()).size());

    } else {
      EXPECT_EQ(2U, GetRevokedUnusedPermissions(hcsm()).size());
    }
  }
  // Whether `url2` was removed from revoked unused permissions or not, it
  // should be in the list of revoked abusive notifications.
  if (ShouldSetupAbusiveNotificationSites()) {
    ExpectRevokedAbusiveNotificationPermissionSize(2U);
  }

  // Fast forward 20 more days will cause auto-cleanup of unused sites, but not
  // abusive sites.
  clock()->Advance(base::Days(20));
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(0U, GetRevokedUnusedPermissions(hcsm()).size());
  }
  if (ShouldSetupAbusiveNotificationSites()) {
    ExpectRevokedAbusiveNotificationPermissionSize(2U);
  }

  // Fast forward 20 more days will cause auto-cleanup of abusive sites.
  clock()->Advance(base::Days(20));
  safety_hub_test_util::UpdateUnusedSitePermissionsServiceAsync(service());
  if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(0U, GetRevokedUnusedPermissions(hcsm()).size());
  }
  if (ShouldSetupAbusiveNotificationSites()) {
    ExpectRevokedAbusiveNotificationPermissionSize(0U);
  }
}

TEST_P(UnusedSitePermissionsServiceTest,
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
  service()->UpdateIntegerValuesToGroupName();

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

TEST_P(UnusedSitePermissionsServiceTest,
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
  service()->UpdateIntegerValuesToGroupName();

  // Validate content settings are stored in group name strings.
  auto permissions_list_string =
      base::Value::List()
          .Append(UnusedSitePermissionsService::ConvertContentSettingsTypeToKey(
              geolocation_type))
          .Append(UnusedSitePermissionsService::ConvertContentSettingsTypeToKey(
              mediastream_type));
  revoked_permissions_content_settings =
      hcsm()->GetSettingsForOneType(revoked_unused_site_type);
  EXPECT_EQ(permissions_list_string, GetRevokedUnusedPermissions(hcsm())[0]
                                         .setting_value.GetDict()
                                         .Find(permissions::kRevokedKey)
                                         ->GetList());
}

TEST_P(UnusedSitePermissionsServiceTest,
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
  service()->UpdateIntegerValuesToGroupName();

  // Validate content settings are stored in group name strings.
  auto permissions_list_string =
      base::Value::List()
          .Append(UnusedSitePermissionsService::ConvertContentSettingsTypeToKey(
              geolocation_type))
          .Append(unknown_type);
  revoked_permissions_content_settings =
      hcsm()->GetSettingsForOneType(revoked_unused_site_type);
  EXPECT_EQ(permissions_list_string, GetRevokedUnusedPermissions(hcsm())[0]
                                         .setting_value.GetDict()
                                         .Find(permissions::kRevokedKey)
                                         ->GetList());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    UnusedSitePermissionsServiceTest,
    testing::Combine(
        /*should_setup_abusive_notification_sites=*/testing::Bool(),
        /*should_setup_unused_sites=*/testing::Bool()));

class UnusedSitePermissionsServiceStartUpTest
    : public ChromeRenderViewHostTestHarness {
 public:
  UnusedSitePermissionsServiceStartUpTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {content_settings::features::kSafetyCheckUnusedSitePermissions,
         content_settings::features::
             kSafetyCheckUnusedSitePermissionsForSupportedChooserPermissions,
         features::kSafetyHub},
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

TEST_F(UnusedSitePermissionsServiceStartUpTest,
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

  // When we start up a new service instance, the latest result (i.e. the list
  // of revoked permissions) should be be updated to strings.
  auto new_service = std::make_unique<UnusedSitePermissionsService>(
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

TEST_F(UnusedSitePermissionsServiceStartUpTest,
       UpdateIntegerValuesToGroupName_MixedKeys) {
  // Setting up two entries one with integers and one with strings to simulate
  // partial migration in case of a crash.
  auto dict_int = base::Value::Dict().Set(
      permissions::kRevokedKey,
      base::Value::List().Append(static_cast<int32_t>(mediastream_type)));
  auto dict_string = base::Value::Dict().Set(
      permissions::kRevokedKey,
      base::Value::List().Append(
          UnusedSitePermissionsService::ConvertContentSettingsTypeToKey(
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

  // When we start up a new service instance, the latest result (i.e. the list
  // of revoked permissions) should be be updated to strings.
  auto new_service = std::make_unique<UnusedSitePermissionsService>(
      profile(), profile()->GetPrefs());

  // Verify the migration is completed on after the service has started and pref
  // is set accordingly.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      safety_hub_prefs::kUnusedSitePermissionsRevocationMigrationCompleted));
  auto expected_permissions_list_url1 = base::Value::List().Append(
      UnusedSitePermissionsService::ConvertContentSettingsTypeToKey(
          mediastream_type));
  auto expected_permissions_list_url2 = base::Value::List().Append(
      UnusedSitePermissionsService::ConvertContentSettingsTypeToKey(
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

TEST_F(UnusedSitePermissionsServiceStartUpTest,
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
          UnusedSitePermissionsService::ConvertContentSettingsTypeToKey(
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

  // When we start up a new service instance, the latest result (i.e. the list
  // of revoked permissions) should be be updated to strings.
  auto new_service = std::make_unique<UnusedSitePermissionsService>(
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
          .Append(UnusedSitePermissionsService::ConvertContentSettingsTypeToKey(
              mediastream_type))
          .Append(unknown_type);
  auto expected_permissions_list_url2 = base::Value::List().Append(
      UnusedSitePermissionsService::ConvertContentSettingsTypeToKey(
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

class UnusedSitePermissionsServiceSafetyHubDisabledTest
    : public ChromeRenderViewHostTestHarness {
 public:
  UnusedSitePermissionsServiceSafetyHubDisabledTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/
        {features::kSafetyHub});
  }
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    hcsm_ = base::MakeRefCounted<HostContentSettingsMap>(
        profile()->GetPrefs(), false, true, false, false);
    service_ = std::make_unique<UnusedSitePermissionsService>(
        profile(), profile()->GetPrefs());
    callback_count_ = 0;
  }

  void TearDown() override {
    service_->Shutdown();
    service_.reset();
    hcsm_->ShutdownOnUIThread();
    base::RunLoop().RunUntilIdle();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void ResetService() {
    service_ = std::make_unique<UnusedSitePermissionsService>(
        profile(), profile()->GetPrefs());
  }

  UnusedSitePermissionsService* service() { return service_.get(); }

  uint8_t callback_count() { return callback_count_; }

 private:
  std::unique_ptr<UnusedSitePermissionsService> service_;
  scoped_refptr<HostContentSettingsMap> hcsm_;
  uint8_t callback_count_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(UnusedSitePermissionsServiceSafetyHubDisabledTest,
       UnusedSitePermissionsRevocationEnabled) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitWithFeatureStates(
      {{content_settings::features::kSafetyCheckUnusedSitePermissions, true}});
  // If Safety Hub is disabled but kSafetyCheckUnusedSitePermissions is on,
  // auto-revocation still happens (i.e. the timer is started on start-up).
  ResetService();
  EXPECT_TRUE(service()->IsTimerRunningForTesting());
}

TEST_F(UnusedSitePermissionsServiceSafetyHubDisabledTest,
       UnusedSitePermissionsRevocationDisabled) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitWithFeatureStates(
      {{content_settings::features::kSafetyCheckUnusedSitePermissions, false},
       {safe_browsing::kSafetyHubAbusiveNotificationRevocation, false}});

  // If both kSafetyHub and kSafetyCheckUnusedSitePermissions are disabled, then
  // no auto-revocation should happen (i.e. no repeated timers should start).
  ResetService();
  EXPECT_FALSE(service()->IsTimerRunningForTesting());
}
