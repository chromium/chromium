// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/cookie_settings.h"

#include <cstddef>
#include <memory>
#include <string>
#include <tuple>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/tpcd/metadata/browser/manager.h"
#include "components/tpcd/metadata/browser/parser.h"
#include "extensions/buildflags/buildflags.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_IOS)
#include "components/content_settings/core/common/features.h"
#else
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#endif

namespace {
using ProviderType = content_settings::ProviderType;

const bool kSupports3pcBlocking = {
#if BUILDFLAG(IS_IOS)
    false
#else
    true
#endif
};

#if !BUILDFLAG(IS_IOS)
constexpr char kAllowedRequestsHistogram[] =
    "API.StorageAccess.AllowedRequests2";
#endif

// To avoid an explosion of test cases, please don't just add a boolean to
// the test features. Consider whether features can interact with each other and
// whether you really need all combinations.

// Controls features that can unblock 3p cookies.
enum GrantSource {
  // Not eligible for additional grants.
  kNoneGranted,
  // Eligible for StorageAccess grants.
  kStorageAccessGrantsEligibleViaAPI,
  // Eligible for TopLevelStorageAccess grants.
  kTopLevelStorageAccessGrantsEligible,
  // Whether `net::features::kTpcdTrialSettings` is enabled.
  k3pcdTrialEligible,
  // Whether `net::features::kTpcdMetadataGrants` is enabled.
  kTpcdMetadataGrantsEligible,
  // Can use Storage Access permission grants via an HTTP response header.
  kStorageAccessGrantsEligibleViaHeader,

  kGrantSourceCount
};

class TestTpcdManagerDelegate : public tpcd::metadata::Manager::Delegate {
 public:
  void SetTpcdMetadataGrants(const ContentSettingsForOneType& grants) override {
  }

  TestingPrefServiceSimple& GetLocalState() override { return local_state_; }

 private:
  TestingPrefServiceSimple local_state_;
};

}  // namespace

namespace content_settings {
class CookieSettingsObserver : public CookieSettings::Observer {
 public:
  explicit CookieSettingsObserver(CookieSettings* settings)
      : settings_(settings) {
    scoped_observation_.Observe(settings);
  }

  CookieSettingsObserver(const CookieSettingsObserver&) = delete;
  CookieSettingsObserver& operator=(const CookieSettingsObserver&) = delete;

  void OnThirdPartyCookieBlockingChanged(
      bool block_third_party_cookies) override {
    ASSERT_EQ(block_third_party_cookies,
              settings_->ShouldBlockThirdPartyCookies());
    last_value_ = block_third_party_cookies;
  }

  bool last_value() { return last_value_; }

 private:
  raw_ptr<CookieSettings> settings_;
  bool last_value_ = false;
  base::ScopedObservation<CookieSettings, CookieSettings::Observer>
      scoped_observation_{this};
};

class CookieSettingsTestBase : public testing::Test {
 public:
  CookieSettingsTestBase()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        kBlockedSite("http://ads.thirdparty.com"),
        kAllowedSite("http://good.allays.com"),
        kFirstPartySite("http://cool.things.com"),
        kSameSiteSite("http://other.things.com"),
        kChromeURL("chrome://foo"),
        kExtensionURL("chrome-extension://deadbeef"),
        kDevToolsURL(GURL("devtools://devtools")),
        kDomain("example.com"),
        kDotDomain(".example.com"),
        kSubDomain("www.example.com"),
        kOtherDomain("www.not-example.com"),
        kDomainWildcardPattern("[*.]example.com"),
        kHttpSite("http://example.com"),
        kHttpsSite("https://example.com"),
        kHttpsSubdomainSite("https://www.example.com"),
        kHttpsSite8080("https://example.com:8080"),
        kBlockedSiteForCookies(net::SiteForCookies::FromUrl(kBlockedSite)),
        kAllowedSiteForCookies(net::SiteForCookies::FromUrl(kAllowedSite)),
        kFirstPartySiteForCookies(
            net::SiteForCookies::FromUrl(kFirstPartySite)),
        kChromeSiteForCookies(net::SiteForCookies::FromUrl(kChromeURL)),
        kExtensionSiteForCookies(net::SiteForCookies::FromUrl(kExtensionURL)),
        kHttpSiteForCookies(net::SiteForCookies::FromUrl(kHttpSite)),
        kHttpsSiteForCookies(net::SiteForCookies::FromUrl(kHttpsSite)),
        kDevToolsSiteForCookies(net::SiteForCookies::FromUrl(kDevToolsURL)),
        kAllHttpsSitesPattern(ContentSettingsPattern::FromString("https://*")) {
  }

  ~CookieSettingsTestBase() override {
    cookie_settings_->ShutdownOnUIThread();
    cookie_settings_incognito_->ShutdownOnUIThread();
    settings_map_->ShutdownOnUIThread();
    tracking_protection_settings_->Shutdown();
  }

  void SetUp() override {
    ContentSettingsRegistry::GetInstance()->ResetForTest();
    CookieSettings::RegisterProfilePrefs(prefs_.registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    privacy_sandbox::RegisterProfilePrefs(prefs_.registry());
    settings_map_ = new HostContentSettingsMap(
        &prefs_, false /* is_off_the_record */, false /* store_last_modified */,
        false /* restore_session */, false /* should_record_metrics */);
    tracking_protection_settings_ =
        std::make_unique<privacy_sandbox::TrackingProtectionSettings>(
            &prefs_, settings_map_.get(),
            /*is_incognito=*/false);

    auto has_fedcm_sharing_permission =
        CookieSettings::NoFedCmSharingPermissionsCallback();

    tpcd_metadata_manager_ = std::make_unique<tpcd::metadata::Manager>(
        tpcd::metadata::Parser::GetInstance(),
        test_tpcd_metadata_manager_delegate_);

    cookie_settings_ = new CookieSettings(
        settings_map_.get(), &prefs_, tracking_protection_settings_.get(),
        false, has_fedcm_sharing_permission, tpcd_metadata_manager_.get(),
        "chrome-extension");
    cookie_settings_incognito_ = new CookieSettings(
        settings_map_.get(), &prefs_, tracking_protection_settings_.get(), true,
        has_fedcm_sharing_permission, /*tpcd_metadata_manager=*/nullptr,
        "chrome-extension");
  }

  void FastForwardTime(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  bool ShouldDeleteCookieOnExit(const std::string& domain, bool is_https) {
    return cookie_settings_->ShouldDeleteCookieOnExit(
        cookie_settings_->GetCookieSettings(), domain,
        is_https ? net::CookieSourceScheme::kSecure
                 : net::CookieSourceScheme::kNonSecure);
  }

 protected:
  // There must be a valid SingleThreadTaskRunner::CurrentDefaultHandle in
  // HostContentSettingsMap's scope.
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;

  sync_preferences::TestingPrefServiceSyncable prefs_;
  TestTpcdManagerDelegate test_tpcd_metadata_manager_delegate_;
  std::unique_ptr<tpcd::metadata::Manager> tpcd_metadata_manager_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
  scoped_refptr<CookieSettings> cookie_settings_;
  scoped_refptr<CookieSettings> cookie_settings_incognito_;
  std::unique_ptr<privacy_sandbox::TrackingProtectionSettings>
      tracking_protection_settings_;

  const GURL kBlockedSite;
  const GURL kAllowedSite;
  const GURL kFirstPartySite;
  const GURL kSameSiteSite;
  const GURL kChromeURL;
  const GURL kExtensionURL;
  const GURL kDevToolsURL;
  const std::string kDomain;
  const std::string kDotDomain;
  const std::string kSubDomain;
  const std::string kOtherDomain;
  const std::string kDomainWildcardPattern;
  const GURL kHttpSite;
  const GURL kHttpsSite;
  const GURL kHttpsSubdomainSite;
  const GURL kHttpsSite8080;
  const net::SiteForCookies kBlockedSiteForCookies;
  const net::SiteForCookies kAllowedSiteForCookies;
  const net::SiteForCookies kFirstPartySiteForCookies;
  const net::SiteForCookies kChromeSiteForCookies;
  const net::SiteForCookies kExtensionSiteForCookies;
  const net::SiteForCookies kHttpSiteForCookies;
  const net::SiteForCookies kHttpsSiteForCookies;
  const net::SiteForCookies kDevToolsSiteForCookies;
  ContentSettingsPattern kAllHttpsSitesPattern;
};

// Default test class to be used by most tests. If you want to add a new
// parameter, consider whether all test cases actually require this parameter
// or whether it is sufficient to add a new subclass of CookieSettingsTestBase.
class CookieSettingsTest : public CookieSettingsTestBase {
 public:
  CookieSettingsTest() {
    feature_list_.InitAndEnableFeature(
        privacy_sandbox::kTrackingProtectionContentSettingFor3pcb);
  }

 private:
};

// Parameterized class that tests combinations of StorageAccess grants and 3pcd
// grants. Tests that don't need the whole range of combinations should create
// their own parameterized subclasses.
class CookieSettingsTestP : public CookieSettingsTestBase,
                            public testing::WithParamInterface<GrantSource> {
 public:
  CookieSettingsTestP() {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (Is3pcdTrialEligible()) {
      enabled_features.push_back({net::features::kTpcdTrialSettings, {}});
    } else {
      disabled_features.push_back(net::features::kTpcdTrialSettings);
    }

    if (Is3pcdMetadataGrantEligible()) {
      enabled_features.push_back({net::features::kTpcdMetadataGrants, {}});
    } else {
      disabled_features.push_back(net::features::kTpcdMetadataGrants);
    }

    enabled_features.push_back({features::kTpcdHeuristicsGrants,
                                {{"TpcdReadHeuristicsGrants", "true"}}});

    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

  bool IsStorageAccessGrantEligibleViaAPI() const {
    return grant_source() == GrantSource::kStorageAccessGrantsEligibleViaAPI;
  }

  bool IsStorageAccessGrantEligibleViaHeader() const {
    return grant_source() == GrantSource::kStorageAccessGrantsEligibleViaHeader;
  }

  bool IsTopLevelStorageAccessGrantEligible() const {
    return grant_source() == GrantSource::kTopLevelStorageAccessGrantsEligible;
  }

  bool Is3pcdTrialEligible() const {
    return grant_source() == GrantSource::k3pcdTrialEligible;
  }

  bool Is3pcdMetadataGrantEligible() const {
    return grant_source() == GrantSource::kTpcdMetadataGrantsEligible;
  }

  net::CookieSettingOverrides GetCookieSettingOverrides() const {
    net::CookieSettingOverrides overrides;
    if (IsStorageAccessGrantEligibleViaAPI()) {
      overrides.Put(net::CookieSettingOverride::kStorageAccessGrantEligible);
    }
    if (IsStorageAccessGrantEligibleViaHeader()) {
      overrides.Put(
          net::CookieSettingOverride::kStorageAccessGrantEligibleViaHeader);
    }
    if (IsTopLevelStorageAccessGrantEligible()) {
      overrides.Put(
          net::CookieSettingOverride::kTopLevelStorageAccessGrantEligible);
    }
    return overrides;
  }

  // Assumes that cookie access would be blocked if not for a Storage Access API
  // grant.
  ContentSetting SettingWithSaaOverride() const {
    return IsStorageAccessGrantEligibleViaAPI() ||
                   IsStorageAccessGrantEligibleViaHeader()
               ? CONTENT_SETTING_ALLOW
               : CONTENT_SETTING_BLOCK;
  }

  // Assumes that cookie access would be blocked if not for some usage of the
  // Storage Access API. Note that this is not the same thing as having a SAA
  // permission grant.
  ContentSetting SettingWithSaaViaAPI() const {
    return IsStorageAccessGrantEligibleViaAPI() ? CONTENT_SETTING_ALLOW
                                                : CONTENT_SETTING_BLOCK;
  }

  // A version of above that considers Top-Level Storage Access API grant
  // instead of Storage Access API grant.
  ContentSetting SettingWithTopLevelSaaOverride() const {
    return IsTopLevelStorageAccessGrantEligible() ? CONTENT_SETTING_ALLOW
                                                  : CONTENT_SETTING_BLOCK;
  }

  // Assumes that cookie access would be blocked if not for a
  // `ContentSettingsType::TPCD_TRIAL` setting.
  ContentSetting SettingWith3pcdTrialSetting() const {
    return Is3pcdTrialEligible() ? CONTENT_SETTING_ALLOW
                                 : CONTENT_SETTING_BLOCK;
  }

  // Assumes that cookie access would be blocked if not for a
  // `net::features::kThirdPartyStoragePartitioning` enablement.
  ContentSetting SettingWith3pcdMetadataGrantEligibleOverride() const {
    return Is3pcdMetadataGrantEligible() ? CONTENT_SETTING_ALLOW
                                         : CONTENT_SETTING_BLOCK;
  }

  // The cookie access result would be blocked if not for a Storage Access API
  // grant.
  net::cookie_util::StorageAccessResult
  BlockedStorageAccessResultWithSaaOverride() const {
    if (IsStorageAccessGrantEligibleViaAPI() ||
        IsStorageAccessGrantEligibleViaHeader()) {
      return net::cookie_util::StorageAccessResult::
          ACCESS_ALLOWED_STORAGE_ACCESS_GRANT;
    }
    return net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
  }

  // The cookie access result would be blocked if not for some Storage Access
  // API usage. Note that this is not the same thing as presence of a permission
  // grant.
  net::cookie_util::StorageAccessResult
  BlockedStorageAccessResultWithSaaViaAPI() const {
    if (IsStorageAccessGrantEligibleViaAPI()) {
      return net::cookie_util::StorageAccessResult::
          ACCESS_ALLOWED_STORAGE_ACCESS_GRANT;
    }
    return net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
  }

  // A version of above that considers Top-Level Storage Access API grant
  // instead of Storage Access API grant.
  net::cookie_util::StorageAccessResult
  BlockedStorageAccessResultWithTopLevelSaaOverride() const {
    if (IsTopLevelStorageAccessGrantEligible()) {
      return net::cookie_util::StorageAccessResult::
          ACCESS_ALLOWED_TOP_LEVEL_STORAGE_ACCESS_GRANT;
    }
    return net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
  }

  // The cookie access result would be blocked if not for a
  // `ContentSettingsType::TPCD_TRIAL` setting.
  net::cookie_util::StorageAccessResult
  BlockedStorageAccessResultWith3pcdTrialSetting() const {
    if (Is3pcdTrialEligible()) {
      return net::cookie_util::StorageAccessResult::ACCESS_ALLOWED_3PCD_TRIAL;
    }
    return net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
  }

  // The storage access result would be blocked if not for a
  // `net::features::kTpcdMetadataGrants` enablement.
  net::cookie_util::StorageAccessResult
  BlockedStorageAccessResultWith3pcdMetadataGrantOverride() const {
    if (Is3pcdMetadataGrantEligible()) {
      return net::cookie_util::StorageAccessResult::
          ACCESS_ALLOWED_3PCD_METADATA_GRANT;
    }
    return net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
  }

 private:
  GrantSource grant_source() const { return GetParam(); }
};

TEST_F(CookieSettingsTest, CustomExceptionsNoWildcardLessSpecificDomain) {
  GURL first_party_url = GURL("https://cool.things.com");

  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(first_party_url, nullptr));

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_NE(kSupports3pcBlocking, cookie_settings_->IsThirdPartyAccessAllowed(
                                      first_party_url, nullptr));

  // No wildcard, matching top-level domain:
  auto less_specific_domain_pattern =
      ContentSettingsPattern::FromString("things.com");
  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(), less_specific_domain_pattern,
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  EXPECT_NE(kSupports3pcBlocking, cookie_settings_->IsThirdPartyAccessAllowed(
                                      first_party_url, nullptr));
}

TEST_F(CookieSettingsTest, CustomExceptionsNoWildcardMatchingDomain) {
  GURL first_party_url = GURL("https://cool.things.com");

  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(first_party_url, nullptr));

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_NE(kSupports3pcBlocking, cookie_settings_->IsThirdPartyAccessAllowed(
                                      first_party_url, nullptr));

  auto top_level_domain_pattern =
      ContentSettingsPattern::FromString("cool.things.com");
  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(), top_level_domain_pattern,
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(first_party_url, nullptr));

  SettingInfo info;
  cookie_settings_->ResetThirdPartyCookieSetting(first_party_url);
  EXPECT_NE(kSupports3pcBlocking, cookie_settings_->IsThirdPartyAccessAllowed(
                                      first_party_url, nullptr));
  // Verify that the exception was removed.
  EXPECT_EQ(settings_map_->GetContentSetting(
                GURL(), first_party_url, ContentSettingsType::COOKIES, &info),
            CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(info.primary_pattern.MatchesAllHosts());
  EXPECT_TRUE(info.secondary_pattern.MatchesAllHosts());
}

TEST_F(CookieSettingsTest, CustomExceptionsWildcardMatchingDomain) {
  GURL first_party_url = GURL("https://cool.things.com");

  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(first_party_url, nullptr));

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_NE(kSupports3pcBlocking, cookie_settings_->IsThirdPartyAccessAllowed(
                                      first_party_url, nullptr));

  auto top_level_domain_pattern =
      ContentSettingsPattern::FromString("[*.]cool.things.com");
  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(), top_level_domain_pattern,
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(first_party_url, nullptr));

  SettingInfo info;
  cookie_settings_->ResetThirdPartyCookieSetting(first_party_url);
  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(first_party_url, nullptr));
  // Verify that the exception was not removed (since it doesn't match any of
  // the expected patterns).
  EXPECT_EQ(settings_map_->GetContentSetting(
                GURL(), first_party_url, ContentSettingsType::COOKIES, &info),
            CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(info.primary_pattern.MatchesAllHosts());
  EXPECT_EQ(info.secondary_pattern, top_level_domain_pattern);
  // Manually reset the exception.
  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(), top_level_domain_pattern,
      ContentSettingsType::COOKIES, CONTENT_SETTING_DEFAULT);
  EXPECT_NE(kSupports3pcBlocking, cookie_settings_->IsThirdPartyAccessAllowed(
                                      first_party_url, nullptr));
}

TEST_F(CookieSettingsTest, CustomExceptionsWildcardLessSpecificDomain) {
  GURL first_party_url = GURL("https://cool.things.com");

  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(first_party_url, nullptr));

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_NE(kSupports3pcBlocking, cookie_settings_->IsThirdPartyAccessAllowed(
                                      first_party_url, nullptr));

  auto top_level_domain_wildcard_pattern =
      ContentSettingsPattern::FromString("[*.]things.com");
  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(), top_level_domain_wildcard_pattern,
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(first_party_url, nullptr));

  SettingInfo info;
  cookie_settings_->ResetThirdPartyCookieSetting(first_party_url);
  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(first_party_url, nullptr));
  // Verify that the exception was not removed (since it doesn't match any of
  // the expected patterns).
  EXPECT_EQ(settings_map_->GetContentSetting(
                GURL(), first_party_url, ContentSettingsType::COOKIES, &info),
            CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(info.primary_pattern.MatchesAllHosts());
  EXPECT_EQ(info.secondary_pattern, top_level_domain_wildcard_pattern);
  // Manually reset the exception.
  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(), top_level_domain_wildcard_pattern,
      ContentSettingsType::COOKIES, CONTENT_SETTING_DEFAULT);
  EXPECT_NE(kSupports3pcBlocking, cookie_settings_->IsThirdPartyAccessAllowed(
                                      first_party_url, nullptr));
}

TEST_F(CookieSettingsTest, CustomExceptionsDotComWildcard) {
  GURL first_party_url = GURL("https://cool.things.com");

  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(first_party_url, nullptr));

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_NE(kSupports3pcBlocking, cookie_settings_->IsThirdPartyAccessAllowed(
                                      first_party_url, nullptr));

  auto dot_com_pattern = ContentSettingsPattern::FromString("[*.]com");
  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(), dot_com_pattern,
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(first_party_url, nullptr));

  SettingInfo info;
  cookie_settings_->ResetThirdPartyCookieSetting(first_party_url);
  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(first_party_url, nullptr));
  // Verify that the exception was not removed (since it doesn't match any of
  // the expected patterns).
  EXPECT_EQ(settings_map_->GetContentSetting(
                GURL(), first_party_url, ContentSettingsType::COOKIES, &info),
            CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(info.primary_pattern.MatchesAllHosts());
  EXPECT_EQ(info.secondary_pattern, dot_com_pattern);
  // Manually reset the exception.
  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(), dot_com_pattern,
      ContentSettingsType::COOKIES, CONTENT_SETTING_DEFAULT);
  EXPECT_NE(kSupports3pcBlocking, cookie_settings_->IsThirdPartyAccessAllowed(
                                      first_party_url, nullptr));
}

TEST_F(CookieSettingsTest, TestAllowlistedScheme) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kHttpSite, kChromeSiteForCookies, /*top_frame_origin=*/std::nullopt,
      net::CookieSettingOverrides()));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kHttpsSite, kChromeSiteForCookies, /*top_frame_origin=*/std::nullopt,
      net::CookieSettingOverrides()));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kChromeURL, kHttpSiteForCookies, /*top_frame_origin=*/std::nullopt,
      net::CookieSettingOverrides()));
#if BUILDFLAG(ENABLE_EXTENSIONS)
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kExtensionURL, kExtensionSiteForCookies,
      /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
#else
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kExtensionURL, kExtensionSiteForCookies,
      /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
#endif
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kExtensionURL, kHttpSiteForCookies, /*top_frame_origin=*/std::nullopt,
      net::CookieSettingOverrides()));
}

TEST_F(CookieSettingsTest, CookiesBlockSingle) {
  cookie_settings_->SetCookieSetting(kBlockedSite, CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kBlockedSiteForCookies, /*top_frame_origin=*/std::nullopt,
      net::CookieSettingOverrides()));
}

TEST_P(CookieSettingsTestP, CookiesBlockThirdParty) {
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  // Cookie is allowed only when block is overridden.

  // A(B) context. Inner frame is cross-origin from top-level frame.
  EXPECT_NE(kSupports3pcBlocking, cookie_settings_->IsFullCookieAccessAllowed(
                                      kBlockedSite, kFirstPartySiteForCookies,
                                      /*top_frame_origin=*/std::nullopt,
                                      GetCookieSettingOverrides()));
  EXPECT_NE(kSupports3pcBlocking,
            cookie_settings_->IsFullCookieAccessAllowed(
                kBlockedSite, net::SiteForCookies(),
                /*top_frame_origin=*/url::Origin::Create(kFirstPartySite),
                GetCookieSettingOverrides()));
  EXPECT_NE(kSupports3pcBlocking, cookie_settings_->IsFullCookieAccessAllowed(
                                      kBlockedSite, net::SiteForCookies(),
                                      /*top_frame_origin=*/std::nullopt,
                                      GetCookieSettingOverrides()));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));

  // A(B(subA)) context. The inner frame is same-site with the top-level frame,
  // but there's an intermediate cross-site frame.
  EXPECT_EQ(IsStorageAccessGrantEligibleViaAPI() ||
                IsStorageAccessGrantEligibleViaHeader() ||
                !kSupports3pcBlocking,
            cookie_settings_->IsFullCookieAccessAllowed(
                kHttpsSubdomainSite, net::SiteForCookies(),
                /*top_frame_origin=*/url::Origin::Create(kHttpsSite),
                GetCookieSettingOverrides()));
}

TEST_F(CookieSettingsTest, CookiesControlsDefault) {
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
  EXPECT_NE(
      kSupports3pcBlocking,
      cookie_settings_incognito_->IsFullCookieAccessAllowed(
          kBlockedSite, kFirstPartySiteForCookies,
          /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
}

TEST_F(CookieSettingsTest, CookiesControlsDisabled) {
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kOff));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
  EXPECT_TRUE(cookie_settings_incognito_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
}

TEST_F(CookieSettingsTest, CookiesControlsEnabledForIncognito) {
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kIncognitoOnly));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
  EXPECT_NE(
      kSupports3pcBlocking,
      cookie_settings_incognito_->IsFullCookieAccessAllowed(
          kBlockedSite, kFirstPartySiteForCookies,
          /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
}

TEST_F(CookieSettingsTest, TestThirdPartyCookiePhaseout) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {
          net::features::kForceThirdPartyCookieBlocking,
          net::features::kThirdPartyStoragePartitioning,
      },
      {});
  ASSERT_TRUE(net::cookie_util::IsForceThirdPartyCookieBlockingEnabled());

  // Build new CookieSettings since `cookie_settings_` was created before
  // ForceThirdPartyCookieBlocking was enabled.
  scoped_refptr<CookieSettings> cookie_settings = new CookieSettings(
      settings_map_.get(), &prefs_, tracking_protection_settings_.get(), false,
      CookieSettings::NoFedCmSharingPermissionsCallback(),
      /*tpcd_metadata_manager=*/nullptr, "chrome-extension");

  EXPECT_EQ(kSupports3pcBlocking,
            cookie_settings->ShouldBlockThirdPartyCookies());

  EXPECT_NE(kSupports3pcBlocking, cookie_settings->IsFullCookieAccessAllowed(
                                      kBlockedSite, kFirstPartySiteForCookies,
                                      /*top_frame_origin=*/std::nullopt,
                                      net::CookieSettingOverrides()));

  // Test that ForceThirdPartyCookieBlocking overrides preference changes.
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kOff));
  EXPECT_NE(kSupports3pcBlocking, cookie_settings->IsFullCookieAccessAllowed(
                                      kBlockedSite, kFirstPartySiteForCookies,
                                      /*top_frame_origin=*/std::nullopt,
                                      net::CookieSettingOverrides()));

  // Test that ForceThirdPartyCookieBlocking can be overridden by site-specific
  // content settings.
  cookie_settings->SetCookieSetting(kBlockedSite, CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(cookie_settings->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));

  // Requests from DevTools panels added by extensions should get cookies.
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kHttpsSite, kDevToolsSiteForCookies,
      /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
}

TEST_F(CookieSettingsTest, CookiesAllowThirdParty) {
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));
}

TEST_F(CookieSettingsTest, CookiesExplicitBlockSingleThirdParty) {
  cookie_settings_->SetCookieSetting(kBlockedSite, CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kAllowedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
}

TEST_F(CookieSettingsTest, CookiesExplicitSessionOnly) {
  cookie_settings_->SetCookieSetting(kBlockedSite,
                                     CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
  EXPECT_TRUE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
  EXPECT_TRUE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));
}

TEST_F(CookieSettingsTest, ThirdPartyExceptionSessionOnly) {
  cookie_settings_->SetThirdPartyCookieSetting(kBlockedSite,
                                               CONTENT_SETTING_SESSION_ONLY);
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));
}

using AreThirdPartyCookiesLimited = CookieSettingsTestP;

TEST_P(AreThirdPartyCookiesLimited, TrueWhen3pcsNotBlockedInModeB) {
  prefs_.SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);
  prefs_.SetBoolean(prefs::kBlockAll3pcToggleEnabled, false);
  EXPECT_TRUE(cookie_settings_->AreThirdPartyCookiesLimited());
}

TEST_P(AreThirdPartyCookiesLimited, FalseWhenAll3pcsBlockedInModeB) {
  prefs_.SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);
  prefs_.SetBoolean(prefs::kBlockAll3pcToggleEnabled, true);
  EXPECT_FALSE(cookie_settings_->AreThirdPartyCookiesLimited());
}

TEST_P(AreThirdPartyCookiesLimited,
       TrueWhenCookieControlsModePrefSetToLimited) {
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kLimited));
  EXPECT_TRUE(cookie_settings_->AreThirdPartyCookiesLimited());
}

TEST_P(AreThirdPartyCookiesLimited,
       FalseWhenCookieControlsModePrefSetToBlockThirdParty) {
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_FALSE(cookie_settings_->AreThirdPartyCookiesLimited());
}

class CookieSettingsTestUserBypass : public CookieSettingsTest {
 public:
  CookieSettingsTestUserBypass() {
    // Verify that cookie settings works correct with temporary user bypass
    // exceptions.
    feature_list_.InitAndEnableFeatureWithParameters(
        content_settings::features::kUserBypassUI, {{"expiration", "90d"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// UserBypass is a desktop and android-only feature
#if !BUILDFLAG(IS_IOS)
TEST_F(CookieSettingsTestUserBypass, UserBypassTemporaryExceptions) {
  // Bypass shouldn't be enabled.
  EXPECT_FALSE(
      cookie_settings_->IsStoragePartitioningBypassEnabled(kFirstPartySite));
  EXPECT_FALSE(
      cookie_settings_->IsStoragePartitioningBypassEnabled(kBlockedSite));

  cookie_settings_->SetCookieSettingForUserBypass(kFirstPartySite);

  // Bypass should only be enabled for |kFirstPartySite| with non-bypassed
  // site(s) unaffected.
  EXPECT_TRUE(
      cookie_settings_->IsStoragePartitioningBypassEnabled(kFirstPartySite));
  EXPECT_FALSE(
      cookie_settings_->IsStoragePartitioningBypassEnabled(kBlockedSite));

  base::TimeDelta expiration =
      content_settings::features::kUserBypassUIExceptionExpiration.Get();
  ASSERT_FALSE(expiration.is_zero());

  FastForwardTime(expiration + base::Seconds(1));
  // Passing the expiry of the user bypass entries should disable user bypass
  // for |kFirstPartySite| leaving non-bypassed site(s) unaffected.
  EXPECT_FALSE(
      cookie_settings_->IsStoragePartitioningBypassEnabled(kFirstPartySite));
  EXPECT_FALSE(
      cookie_settings_->IsStoragePartitioningBypassEnabled(kBlockedSite));
}

TEST_F(CookieSettingsTestUserBypass,
       TrackingProtectionUserBypassTemporaryExceptions) {
  EXPECT_FALSE(
      cookie_settings_->IsStoragePartitioningBypassEnabled(kFirstPartySite));
  EXPECT_FALSE(
      cookie_settings_->IsStoragePartitioningBypassEnabled(kBlockedSite));

  tracking_protection_settings_->AddTrackingProtectionException(
      kFirstPartySite, /*is_user_bypass_exception=*/true);
  EXPECT_TRUE(
      cookie_settings_->IsStoragePartitioningBypassEnabled(kFirstPartySite));
  EXPECT_FALSE(
      cookie_settings_->IsStoragePartitioningBypassEnabled(kBlockedSite));

  base::TimeDelta expiration =
      content_settings::features::kUserBypassUIExceptionExpiration.Get();
  ASSERT_FALSE(expiration.is_zero());

  FastForwardTime(expiration + base::Seconds(1));
  // Passing the expiry of the user bypass entries should disable user bypass
  // for |kFirstPartySite| leaving non-bypassed site(s) unaffected.
  EXPECT_FALSE(
      cookie_settings_->IsStoragePartitioningBypassEnabled(kFirstPartySite));
  EXPECT_FALSE(
      cookie_settings_->IsStoragePartitioningBypassEnabled(kBlockedSite));
}
#endif

TEST_F(CookieSettingsTestUserBypass,
       UserBypassThirdPartyCookiesTemporaryExceptions) {
  GURL first_party_url = kFirstPartySiteForCookies.RepresentativeUrl();
  GURL same_site_url = kSameSiteSite;
  SettingInfo info;

  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(kFirstPartySite, &info));
  EXPECT_EQ(info.metadata.expiration(), base::Time());

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_NE(kSupports3pcBlocking, cookie_settings_->IsThirdPartyAccessAllowed(
                                      kFirstPartySite, &info));
  EXPECT_EQ(info.metadata.expiration(), base::Time());

  cookie_settings_->SetCookieSettingForUserBypass(first_party_url);
  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(first_party_url, &info));
  base::TimeDelta expiration_delta =
      content_settings::features::kUserBypassUIExceptionExpiration.Get();
  EXPECT_EQ(info.metadata.expiration(), base::Time::Now() + expiration_delta);
  SettingInfo exception_info;
  // Verify that the correct exception is created.
  EXPECT_EQ(settings_map_->GetContentSetting(GURL(), first_party_url,
                                             ContentSettingsType::COOKIES,
                                             &exception_info),
            CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(exception_info.primary_pattern.MatchesAllHosts());
  EXPECT_EQ(
      exception_info.secondary_pattern,
      ContentSettingsPattern::FromURLToSchemefulSitePattern(first_party_url));

  EXPECT_EQ(
      settings_map_->GetContentSetting(
          GURL(), same_site_url, ContentSettingsType::COOKIES, &exception_info),
      CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(exception_info.primary_pattern.MatchesAllHosts());
  EXPECT_EQ(
      exception_info.secondary_pattern,
      ContentSettingsPattern::FromURLToSchemefulSitePattern(first_party_url));

  cookie_settings_->ResetThirdPartyCookieSetting(first_party_url);
  EXPECT_NE(kSupports3pcBlocking, cookie_settings_->IsThirdPartyAccessAllowed(
                                      first_party_url, nullptr));
  EXPECT_NE(kSupports3pcBlocking, cookie_settings_->IsThirdPartyAccessAllowed(
                                      same_site_url, nullptr));
  // Verify that the exception was removed.
  EXPECT_EQ(settings_map_->GetContentSetting(GURL(), first_party_url,
                                             ContentSettingsType::COOKIES,
                                             &exception_info),
            CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(exception_info.primary_pattern.MatchesAllHosts());
  EXPECT_TRUE(exception_info.secondary_pattern.MatchesAllHosts());
}

TEST_F(CookieSettingsTestUserBypass, ResetThirdPartyCookiesExceptions) {
  GURL first_party_url = kFirstPartySiteForCookies.RepresentativeUrl();

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_NE(kSupports3pcBlocking, cookie_settings_->IsThirdPartyAccessAllowed(
                                      kFirstPartySite, nullptr));

  cookie_settings_->SetThirdPartyCookieSetting(first_party_url,
                                               CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(first_party_url, nullptr));

  cookie_settings_->ResetThirdPartyCookieSetting(first_party_url);
  EXPECT_NE(kSupports3pcBlocking, cookie_settings_->IsThirdPartyAccessAllowed(
                                      first_party_url, nullptr));

  cookie_settings_->SetCookieSettingForUserBypass(first_party_url);
  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(first_party_url, nullptr));

  cookie_settings_->ResetThirdPartyCookieSetting(first_party_url);
  EXPECT_NE(kSupports3pcBlocking, cookie_settings_->IsThirdPartyAccessAllowed(
                                      first_party_url, nullptr));
  SettingInfo info;
  // Verify that the exception was removed.
  EXPECT_EQ(settings_map_->GetContentSetting(
                GURL(), first_party_url, ContentSettingsType::COOKIES, &info),
            CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(info.primary_pattern.MatchesAllHosts());
  EXPECT_TRUE(info.secondary_pattern.MatchesAllHosts());
}

TEST_F(CookieSettingsTestUserBypass,
       UserBypassThirdPartyCookiesIncognitoExceptions) {
  // User bypass exceptions created in incognito should always be permanent.
  GURL first_party_url = kFirstPartySiteForCookies.RepresentativeUrl();
  SettingInfo info;

  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(kFirstPartySite, &info));
  EXPECT_EQ(info.metadata.expiration(), base::Time());

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_NE(kSupports3pcBlocking,
            cookie_settings_incognito_->IsThirdPartyAccessAllowed(
                kFirstPartySite, &info));
  EXPECT_EQ(info.metadata.expiration(), base::Time());

  cookie_settings_incognito_->SetCookieSettingForUserBypass(first_party_url);
  EXPECT_TRUE(cookie_settings_incognito_->IsThirdPartyAccessAllowed(
      first_party_url, &info));

  base::TimeDelta expiration =
      content_settings::features::kUserBypassUIExceptionExpiration.Get();
  ASSERT_FALSE(expiration.is_zero());
  EXPECT_TRUE(info.metadata.expiration().is_null());

  SettingInfo exception_info;
  // Verify that the correct exception is created.
  EXPECT_EQ(settings_map_->GetContentSetting(GURL(), first_party_url,
                                             ContentSettingsType::COOKIES,
                                             &exception_info),
            CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(exception_info.primary_pattern.MatchesAllHosts());
  EXPECT_EQ(
      exception_info.secondary_pattern,
      ContentSettingsPattern::FromURLToSchemefulSitePattern(first_party_url));

  cookie_settings_incognito_->ResetThirdPartyCookieSetting(first_party_url);
  EXPECT_NE(kSupports3pcBlocking,
            cookie_settings_incognito_->IsThirdPartyAccessAllowed(
                first_party_url, nullptr));
  // Verify that the exception was removed.
  EXPECT_EQ(settings_map_->GetContentSetting(GURL(), first_party_url,
                                             ContentSettingsType::COOKIES,
                                             &exception_info),
            CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(exception_info.primary_pattern.MatchesAllHosts());
  EXPECT_TRUE(exception_info.secondary_pattern.MatchesAllHosts());
}

TEST_F(CookieSettingsTest, KeepBlocked) {
  // Keep blocked cookies.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);
  cookie_settings_->SetCookieSetting(kHttpsSite, CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, true));
}

TEST_F(CookieSettingsTest, DeleteSessionOnly) {
  // Keep session_only http cookies if https is allowed.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  cookie_settings_->SetCookieSetting(kHttpsSite, CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kSubDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kSubDomain, true));

  // Delete cookies if site is session only.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  cookie_settings_->SetCookieSetting(kHttpsSite, CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, true));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kSubDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kSubDomain, true));

  // Http blocked, https allowed - keep secure and non secure cookies.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  cookie_settings_->SetCookieSetting(kHttpSite, CONTENT_SETTING_BLOCK);
  cookie_settings_->SetCookieSetting(kHttpsSite, CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kSubDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kSubDomain, true));

  // Http and https session only, all is deleted.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);
  cookie_settings_->SetCookieSetting(kHttpSite, CONTENT_SETTING_SESSION_ONLY);
  cookie_settings_->SetCookieSetting(kHttpsSite, CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, true));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kSubDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kSubDomain, true));
}

TEST_F(CookieSettingsTest, DeleteSessionOnlyWithThirdPartyBlocking) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_TRUE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, false));
}

TEST_F(CookieSettingsTest, DeletionWithDifferentPorts) {
  // Keep cookies for site with special port.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  cookie_settings_->SetCookieSetting(kHttpsSite8080, CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, true));

  // Delete cookies with special port.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  cookie_settings_->SetCookieSetting(kHttpsSite8080,
                                     CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, true));
}

TEST_F(CookieSettingsTest, DeletionWithSubDomains) {
  // Cookies accessible by subdomains are kept.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  cookie_settings_->SetCookieSetting(kHttpsSubdomainSite,
                                     CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, true));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kSubDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kSubDomain, true));

  // Cookies that have a session_only subdomain but are accessible by allowed
  // domains are kept.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);
  cookie_settings_->SetCookieSetting(kHttpsSubdomainSite,
                                     CONTENT_SETTING_SESSION_ONLY);
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kSubDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kSubDomain, true));

  // Cookies created by session_only subdomains are deleted.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  cookie_settings_->SetCookieSetting(kHttpsSubdomainSite,
                                     CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kSubDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kSubDomain, true));
}

TEST_F(CookieSettingsTest, DeleteCookiesWithThirdPartyException) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);
  cookie_settings_->SetThirdPartyCookieSetting(kHttpsSite,
                                               CONTENT_SETTING_SESSION_ONLY);
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, true));
}

TEST_F(CookieSettingsTest, CookiesThirdPartyBlockedExplicitAllow) {
  cookie_settings_->SetCookieSetting(kAllowedSite, CONTENT_SETTING_ALLOW);
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kAllowedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));

  // Extensions should always be allowed to use cookies.
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kAllowedSite, kExtensionSiteForCookies,
      /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
}

TEST_F(CookieSettingsTest, CookiesThirdPartyBlockedAllSitesAllowed) {
  net::CookieSettingOverrides cookie_setting_overrides;

  cookie_settings_->SetCookieSetting(kAllowedSite, CONTENT_SETTING_ALLOW);
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  // As an example for a url that matches all hosts but not all origins,
  // match all HTTPS sites.
  settings_map_->SetContentSettingCustomScope(
      kAllHttpsSitesPattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);

  // |kAllowedSite| should be allowed.
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kAllowedSite, kBlockedSiteForCookies, /*top_frame_origin*/ std::nullopt,
      cookie_setting_overrides));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));

  // HTTPS sites should be allowed in a first-party context.
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kHttpsSite, kHttpsSiteForCookies, /*top_frame_origin=*/std::nullopt,
      cookie_setting_overrides));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));

  // HTTP sites should be allowed.
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kFirstPartySite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/std::nullopt, cookie_setting_overrides));
  EXPECT_TRUE(cookie_settings_->IsCookieSessionOnly(kFirstPartySite));

  // Third-party cookies should be blocked.
  EXPECT_NE(kSupports3pcBlocking,
            cookie_settings_->IsFullCookieAccessAllowed(
                kFirstPartySite, kBlockedSiteForCookies,
                /*top_frame_origin=*/std::nullopt, cookie_setting_overrides));
  EXPECT_NE(kSupports3pcBlocking,
            cookie_settings_->IsFullCookieAccessAllowed(
                kHttpsSite, kBlockedSiteForCookies,
                /*top_frame_origin=*/std::nullopt, cookie_setting_overrides));
}

TEST_F(CookieSettingsTest, CookiesBlockEverything) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kFirstPartySite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kAllowedSite, kFirstPartySiteForCookies,
      /*top_frame_origin*/ std::nullopt, net::CookieSettingOverrides()));
}

TEST_F(CookieSettingsTest, CookiesBlockEverythingExceptAllowed) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  cookie_settings_->SetCookieSetting(kAllowedSite, CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kFirstPartySite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kAllowedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kAllowedSite, kAllowedSiteForCookies, /*top_frame_origin=*/std::nullopt,
      net::CookieSettingOverrides()));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));
}

#if !BUILDFLAG(IS_IOS)
TEST_F(CookieSettingsTest, GetCookieSettingAllowedTelemetry) {
  const GURL top_level_url = GURL(kFirstPartySite);
  const GURL url = GURL(kAllowedSite);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kOff));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, net::SiteForCookies(), top_level_url,
                net::CookieSettingOverrides(), nullptr),
            CONTENT_SETTING_ALLOW);
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 1);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(net::cookie_util::StorageAccessResult::ACCESS_ALLOWED),
      1);
}

TEST_P(CookieSettingsTestP, GetCookieSettingSAA) {
  const GURL top_level_url = GURL(kFirstPartySite);
  const GURL url = GURL(kAllowedSite);
  const GURL third_url = GURL(kBlockedSite);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));

  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            SettingWithSaaOverride());
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 1);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(BlockedStorageAccessResultWithSaaOverride()), 1);

  // Invalid pair the |top_level_url| granting access to |url| is now
  // being loaded under |url| as the top level url.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                top_level_url, net::SiteForCookies(), url,
                GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  // Invalid pairs where a |third_url| is used.
  EXPECT_EQ(
      cookie_settings_->GetCookieSetting(url, net::SiteForCookies(), third_url,
                                         GetCookieSettingOverrides(), nullptr),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                third_url, net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

// Test that http exceptions also affect websocket requests.
TEST_P(CookieSettingsTestP, GetCookieSettingSAAWebsocket) {
  const GURL top_level_url = GURL(kFirstPartySite);
  const GURL url = GURL(kHttpsSite);

  GURL::Replacements ws_replacement;
  ws_replacement.SetSchemeStr(url::kWsScheme);
  const GURL ws_url = url.ReplaceComponents(ws_replacement);

  GURL::Replacements wss_replacement;
  wss_replacement.SetSchemeStr(url::kWssScheme);
  const GURL wss_url = url.ReplaceComponents(wss_replacement);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW);

  // Https should be allowed.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            SettingWithSaaOverride());
  // Secure websocket is also allowed.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                wss_url, net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            SettingWithSaaOverride());
  // Insecure websocket stays blocked.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                ws_url, net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_P(CookieSettingsTestP, GetCookieSettingSAAViaFedCM) {
  const GURL top_level_url = GURL(kFirstPartySite);
  const GURL url = GURL(kAllowedSite);
  const GURL third_url = GURL(kBlockedSite);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));

  cookie_settings_ = new CookieSettings(
      settings_map_.get(), &prefs_, tracking_protection_settings_.get(), false,
      base::BindLambdaForTesting([&]() -> ContentSettingsForOneType {
        return ContentSettingsForOneType{
            ContentSettingPatternSource(
                ContentSettingsPattern::FromURLToSchemefulSitePattern(url),
                ContentSettingsPattern::FromURLToSchemefulSitePattern(
                    top_level_url),
                content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW),
                ProviderType::kNone, /*incognito=*/false),
        };
      }),
      /*tpcd_metadata_manager=*/nullptr, "chrome-extension");

  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            SettingWithSaaViaAPI());
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 1);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(BlockedStorageAccessResultWithSaaViaAPI()), 1);

  // Grants are not bidrectional.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                top_level_url, net::SiteForCookies(), url,
                GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  // Unrelated contexts do not get access.
  EXPECT_EQ(
      cookie_settings_->GetCookieSetting(url, net::SiteForCookies(), third_url,
                                         GetCookieSettingOverrides(), nullptr),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                third_url, net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

// A top-level storage access grant should behave similarly to standard SAA
// grants.
TEST_P(CookieSettingsTestP, GetCookieSettingTopLevelStorageAccess) {
  const GURL top_level_url(kFirstPartySite);
  const GURL url(kAllowedSite);
  const GURL third_url(kBlockedSite);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));

  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            SettingWithTopLevelSaaOverride());
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 1);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(BlockedStorageAccessResultWithTopLevelSaaOverride()), 1);

  // Invalid pair the |top_level_url| granting access to |url| is now being
  // loaded under |url| as the top level url.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                top_level_url, net::SiteForCookies(), url,
                GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  // Invalid pairs where a |third_url| is used.
  EXPECT_EQ(
      cookie_settings_->GetCookieSetting(url, net::SiteForCookies(), third_url,
                                         GetCookieSettingOverrides(), nullptr),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                third_url, net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

// Subdomains of the granted resource url should not gain access if a valid
// grant exists; the grant should also not apply on different schemes.
TEST_P(CookieSettingsTestP, GetCookieSettingSAAResourceWildcards) {
  const GURL top_level_url = GURL(kFirstPartySite);
  const GURL url = GURL(kHttpsSite);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));

  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            SettingWithSaaOverride());
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                GURL(kHttpsSubdomainSite), net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                GURL(kHttpSite), net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

// Subdomains of the granted top level url should not grant access if a valid
// grant exists; the grant should also not apply on different schemes.
TEST_P(CookieSettingsTestP, GetCookieSettingSAATopLevelWildcards) {
  const GURL top_level_url = GURL(kHttpsSite);
  const GURL url = GURL(kFirstPartySite);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));

  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            SettingWithSaaOverride());
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, net::SiteForCookies(), GURL(kHttpsSubdomainSite),
                GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, net::SiteForCookies(), GURL(kHttpSite),
                GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

// Explicit settings should be respected regardless of whether Storage Access
// API is enabled and/or has grants.
TEST_P(CookieSettingsTestP, GetCookieSettingRespectsExplicitSettings) {
  const GURL top_level_url = GURL(kFirstPartySite);
  const GURL url = GURL(kAllowedSite);

  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

// Once a grant expires access should no longer be given.
TEST_P(CookieSettingsTestP, GetCookieSettingSAAExpiredGrant) {
  const GURL top_level_url = GURL(kFirstPartySite);
  const GURL url = GURL(kAllowedSite);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));

  ContentSettingConstraints constraints;
  constraints.set_lifetime(base::Seconds(100));
  constraints.set_session_model(mojom::SessionModel::USER_SESSION);

  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW, constraints);

  // When requesting our setting for the url/top-level combination our grant is
  // for access should be allowed iff SAA is enabled. For any other domain pairs
  // access should still be blocked.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            SettingWithSaaOverride());

  // If we fastforward past the expiration of our grant the result should be
  // CONTENT_SETTING_BLOCK now.
  FastForwardTime(base::Seconds(101));
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_P(CookieSettingsTestP, GetCookieSetting3pcdTrial) {
  const GURL top_level_url(kFirstPartySite);
  const GURL url(kAllowedSite);
  const GURL third_url(kBlockedSite);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  prefs_.SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);

  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      ContentSettingsType::TPCD_TRIAL, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            SettingWith3pcdTrialSetting());
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 1);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(BlockedStorageAccessResultWith3pcdTrialSetting()), 1);

  // Check override trial setting.
  auto overrides = GetCookieSettingOverrides();
  overrides.Put(net::CookieSettingOverride::kSkipTPCDTrial);
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, net::SiteForCookies(), top_level_url, overrides, nullptr),
            CONTENT_SETTING_BLOCK);

  // Invalid pair the |top_level_url| granting access to |url| is now being
  // loaded under |url| as the top level url.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                top_level_url, net::SiteForCookies(), url,
                GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  // Invalid pairs where a |third_url| is used.
  EXPECT_EQ(
      cookie_settings_->GetCookieSetting(url, net::SiteForCookies(), third_url,
                                         GetCookieSettingOverrides(), nullptr),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                third_url, net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_P(CookieSettingsTestP, GetCookieSetting3pcdMetadataGrants) {
  const GURL top_level_url(kFirstPartySite);
  const GURL url(kAllowedSite);
  const GURL third_url(kBlockedSite);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  prefs_.SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);

  ContentSettingsForOneType tpcd_metadata_grants;
  tpcd_metadata_grants.emplace_back(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      base::Value(ContentSetting::CONTENT_SETTING_ALLOW),
      content_settings::ProviderType::kNone, false);
  tpcd_metadata_manager_->SetGrantsForTesting(tpcd_metadata_grants);

  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            SettingWith3pcdMetadataGrantEligibleOverride());

  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 1);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(
          BlockedStorageAccessResultWith3pcdMetadataGrantOverride()),
      1);

  // Check override metadata grant.
  auto overrides = GetCookieSettingOverrides();
  overrides.Put(net::CookieSettingOverride::kSkipTPCDMetadataGrant);
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, net::SiteForCookies(), top_level_url, overrides, nullptr),
            CONTENT_SETTING_BLOCK);

  // Invalid pair the |top_level_url| granting access to |url| is now being
  // loaded under |url| as the top level url.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                top_level_url, net::SiteForCookies(), url,
                GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(
      cookie_settings_->IsAllowedByTpcdMetadataGrant(top_level_url, url));

  // Invalid pairs where a |third_url| is used.
  EXPECT_EQ(
      cookie_settings_->GetCookieSetting(url, net::SiteForCookies(), third_url,
                                         GetCookieSettingOverrides(), nullptr),
      CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(cookie_settings_->IsAllowedByTpcdMetadataGrant(third_url, url));

  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                third_url, net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(
      cookie_settings_->IsAllowedByTpcdMetadataGrant(top_level_url, third_url));
}

TEST_P(CookieSettingsTestP, GetCookieSetting3pcdHeuristicsGrants) {
  const GURL first_party_url(kFirstPartySite);
  const GURL third_party_url(kAllowedSite);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  {
    base::RunLoop run_loop;
    prefs_.SetInteger(prefs::kCookieControlsMode,
                      static_cast<int>(CookieControlsMode::kBlockThirdParty));
    prefs_.SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);
    run_loop.RunUntilIdle();
  }

  // Expect that cookies are blocked before setting the temporary grant.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                third_party_url, net::SiteForCookies(), first_party_url,
                GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  const base::TimeDelta expiration = base::Seconds(5);
  cookie_settings_->SetTemporaryCookieGrantForHeuristic(
      third_party_url, first_party_url, expiration);

  // Expect that cookies are now allowed, and the histogram has been updated.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                third_party_url, net::SiteForCookies(), first_party_url,
                GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_ALLOW);
  // Expect 2 total requests for the two calls to GetCookieSetting.
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 2);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(net::cookie_util::StorageAccessResult::
                           ACCESS_ALLOWED_3PCD_HEURISTICS_GRANT),
      1);

  // Check override heuristics grant.
  auto overrides = GetCookieSettingOverrides();
  overrides.Put(net::CookieSettingOverride::kSkipTPCDHeuristicsGrant);
  EXPECT_EQ(
      cookie_settings_->GetCookieSetting(third_party_url, net::SiteForCookies(),
                                         first_party_url, overrides, nullptr),
      CONTENT_SETTING_BLOCK);

  FastForwardTime(expiration + base::Seconds(1));

  // Expect that cookies are blocked again after the grant expires.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                third_party_url, net::SiteForCookies(), first_party_url,
                GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, SetTemporaryCookieGrantForHeuristicOverrides) {
  const GURL first_party_url(kFirstPartySite);
  const GURL third_party_url(kAllowedSite);
  const base::TimeDelta expiration_short = base::Seconds(5);
  const base::TimeDelta expiration_long = base::Seconds(60);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  prefs_.SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);

  // Expect that cookies are blocked before setting the temporary grant.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                third_party_url, net::SiteForCookies(), first_party_url,
                net::CookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  // Create a grant and verify that cookies are now allowed.
  cookie_settings_->SetTemporaryCookieGrantForHeuristic(
      third_party_url, first_party_url, expiration_short);
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                third_party_url, net::SiteForCookies(), first_party_url,
                net::CookieSettingOverrides(), nullptr),
            CONTENT_SETTING_ALLOW);

  // Create a longer grant and verify that this extends the TTL of the first
  // grant.
  cookie_settings_->SetTemporaryCookieGrantForHeuristic(
      third_party_url, first_party_url, expiration_long);
  FastForwardTime(expiration_short + base::Seconds(1));
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                third_party_url, net::SiteForCookies(), first_party_url,
                net::CookieSettingOverrides(), nullptr),
            CONTENT_SETTING_ALLOW);

  // Create a shorter grant and verify that this does NOT shorten the TTL of the
  // longer grant.
  cookie_settings_->SetTemporaryCookieGrantForHeuristic(
      third_party_url, first_party_url, expiration_short);
  FastForwardTime(expiration_short + base::Seconds(1));
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                third_party_url, net::SiteForCookies(), first_party_url,
                net::CookieSettingOverrides(), nullptr),
            CONTENT_SETTING_ALLOW);

  // Expect that cookies are blocked again after the longer grant expires.
  FastForwardTime(expiration_long + base::Seconds(1));
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                third_party_url, net::SiteForCookies(), first_party_url,
                net::CookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, SetTemporaryCookieGrantForHeuristicSchemeless) {
  const GURL first_party_http_url(kHttpSite);
  const GURL first_party_https_url(kHttpsSite);
  const GURL third_party_url(kAllowedSite);
  const base::TimeDelta expiration = base::Seconds(5);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  prefs_.SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);

  // Expect that cookies are blocked before setting the temporary grant.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                third_party_url, net::SiteForCookies(), first_party_https_url,
                net::CookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  // Set a (schemeful) cookie grant on the HTTP URL.
  cookie_settings_->SetTemporaryCookieGrantForHeuristic(
      third_party_url, first_party_http_url, expiration);

  // This grant should not affect the HTTPS URL.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                third_party_url, net::SiteForCookies(), first_party_https_url,
                net::CookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  // Set a schemeless cookie grant on the HTTP URL.
  cookie_settings_->SetTemporaryCookieGrantForHeuristic(
      third_party_url, first_party_http_url, expiration,
      /*use_schemeless_patterns=*/true);

  // This grant should enable cookie access on the HTTPS first-party URL.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                third_party_url, net::SiteForCookies(), first_party_https_url,
                net::CookieSettingOverrides(), nullptr),
            CONTENT_SETTING_ALLOW);

  // Expect that cookies are blocked again after the grant expires.
  FastForwardTime(expiration + base::Seconds(1));
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                third_party_url, net::SiteForCookies(), first_party_https_url,
                net::CookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}
#endif

TEST_F(CookieSettingsTest, ExtensionsRegularSettings) {
  cookie_settings_->SetCookieSetting(kBlockedSite, CONTENT_SETTING_BLOCK);

  // Regular cookie settings also apply to extensions.
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kExtensionSiteForCookies,
      /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
}

TEST_F(CookieSettingsTest, ExtensionsOwnCookies) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Extensions can always use cookies (and site data) in their own origin.
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kExtensionURL, kExtensionSiteForCookies,
      /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
#else
  // Except if extensions are disabled. Then the extension-specific checks do
  // not exist and the default setting is to block.
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kExtensionURL, kExtensionSiteForCookies,
      /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
#endif
}

TEST_F(CookieSettingsTest, ExtensionsThirdParty) {
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));

  // XHRs stemming from extensions are exempt from third-party cookie blocking
  // rules (as the first party is always the extension's security origin).
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kExtensionSiteForCookies,
      /*top_frame_origin=*/std::nullopt, net::CookieSettingOverrides()));
}

TEST_F(CookieSettingsTest, ThirdPartyException) {
  GURL first_party_url = kFirstPartySiteForCookies.RepresentativeUrl();
  net::CookieSettingOverrides cookie_setting_overrides;

  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(kFirstPartySite, nullptr));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kHttpsSite, kFirstPartySiteForCookies, /*top_frame_origin=*/std::nullopt,
      cookie_setting_overrides));

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_NE(kSupports3pcBlocking, cookie_settings_->IsThirdPartyAccessAllowed(
                                      kFirstPartySite, nullptr));
  EXPECT_NE(kSupports3pcBlocking,
            cookie_settings_->IsFullCookieAccessAllowed(
                kHttpsSite, kFirstPartySiteForCookies,
                /*top_frame_origin=*/std::nullopt, cookie_setting_overrides));

  cookie_settings_->SetThirdPartyCookieSetting(first_party_url,
                                               CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(first_party_url, nullptr));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kHttpsSite, kFirstPartySiteForCookies, /*top_frame_origin=*/std::nullopt,
      cookie_setting_overrides));
  SettingInfo info;
  // Verify that the correct exception is created.
  EXPECT_EQ(settings_map_->GetContentSetting(
                GURL(), first_party_url, ContentSettingsType::COOKIES, &info),
            CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(info.primary_pattern.MatchesAllHosts());
  EXPECT_EQ(info.secondary_pattern,
            ContentSettingsPattern::FromURLNoWildcard(first_party_url));

  cookie_settings_->ResetThirdPartyCookieSetting(first_party_url);
  EXPECT_NE(kSupports3pcBlocking, cookie_settings_->IsThirdPartyAccessAllowed(
                                      first_party_url, nullptr));
  EXPECT_NE(kSupports3pcBlocking,
            cookie_settings_->IsFullCookieAccessAllowed(
                kHttpsSite, kFirstPartySiteForCookies,
                /*top_frame_origin=*/std::nullopt, cookie_setting_overrides));
  // Verify that the exception was removed.
  EXPECT_EQ(settings_map_->GetContentSetting(
                GURL(), first_party_url, ContentSettingsType::COOKIES, &info),
            CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(info.primary_pattern.MatchesAllHosts());
  EXPECT_TRUE(info.secondary_pattern.MatchesAllHosts());

  cookie_settings_->SetCookieSetting(kHttpsSite, CONTENT_SETTING_ALLOW);
  EXPECT_NE(kSupports3pcBlocking, cookie_settings_->IsThirdPartyAccessAllowed(
                                      kFirstPartySite, nullptr));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kHttpsSite, kFirstPartySiteForCookies, /*top_frame_origin=*/std::nullopt,
      cookie_setting_overrides));
}

// The TRACKING_PROTECTION content setting is not registered on iOS
#if !BUILDFLAG(IS_IOS)
TEST_F(CookieSettingsTest,
       TrackingProtectionExceptionReadWhenNoCookieException) {
  GURL first_party_url = kFirstPartySiteForCookies.RepresentativeUrl();
  net::CookieSettingOverrides cookie_setting_overrides;

  // Set default to block 3PCs
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_FALSE(
      cookie_settings_->IsThirdPartyAccessAllowed(first_party_url, nullptr));
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kHttpsSite, kFirstPartySiteForCookies, /*top_frame_origin=*/std::nullopt,
      cookie_setting_overrides));

  // Add Tracking Protection exception
  tracking_protection_settings_->AddTrackingProtectionException(
      first_party_url);
  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(first_party_url, nullptr));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kHttpsSite, kFirstPartySiteForCookies, /*top_frame_origin=*/std::nullopt,
      cookie_setting_overrides));

  // Explicitly block 3PCs for the URL. This should take priority over the
  // Tracking Protection exception
  cookie_settings_->SetThirdPartyCookieSetting(first_party_url,
                                               CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(
      cookie_settings_->IsThirdPartyAccessAllowed(first_party_url, nullptr));
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kHttpsSite, kFirstPartySiteForCookies, /*top_frame_origin=*/std::nullopt,
      cookie_setting_overrides));
}
#endif

TEST_F(CookieSettingsTest, ManagedThirdPartyException) {
  SettingInfo info;
  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(kFirstPartySite, &info));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kHttpsSite, kFirstPartySiteForCookies, /*top_frame_origin=*/std::nullopt,
      net::CookieSettingOverrides()));
  EXPECT_EQ(info.source, SettingSource::kUser);

  prefs_.SetManagedPref(prefs::kManagedDefaultCookiesSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_FALSE(
      cookie_settings_->IsThirdPartyAccessAllowed(kFirstPartySite, &info));
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kHttpsSite, kFirstPartySiteForCookies, /*top_frame_origin=*/std::nullopt,
      net::CookieSettingOverrides()));
  EXPECT_EQ(info.source, SettingSource::kPolicy);
}

TEST_F(CookieSettingsTest, ThirdPartySettingObserver) {
  CookieSettingsObserver observer(cookie_settings_.get());
  EXPECT_FALSE(observer.last_value());
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_EQ(kSupports3pcBlocking, observer.last_value());
}

TEST_F(CookieSettingsTest, PreservesBlockingStateFrom3pcdOnOffboarding) {
  // CookieControlsMode starts in the default state when we onboard.
  prefs_.SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);
  EXPECT_EQ(prefs_.GetInteger(prefs::kCookieControlsMode),
            static_cast<int>(CookieControlsMode::kIncognitoOnly));

  // If the block all toggle is off when we offboard, the CookieControlsMode
  // pref stays the same.
  prefs_.SetBoolean(prefs::kBlockAll3pcToggleEnabled, false);
  prefs_.SetBoolean(prefs::kTrackingProtection3pcdEnabled, false);
  EXPECT_EQ(prefs_.GetInteger(prefs::kCookieControlsMode),
            static_cast<int>(CookieControlsMode::kIncognitoOnly));

  // If the block all toggle is on when we offboard, the CookieControlsMode
  // pref is changed to BlockThirdParty.
  prefs_.SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);
  prefs_.SetBoolean(prefs::kBlockAll3pcToggleEnabled, true);
  prefs_.SetBoolean(prefs::kTrackingProtection3pcdEnabled, false);
  EXPECT_EQ(prefs_.GetInteger(prefs::kCookieControlsMode),
            static_cast<int>(CookieControlsMode::kBlockThirdParty));
}

TEST_F(CookieSettingsTest, LegacyCookieAccessAllowAll) {
  settings_map_->SetDefaultContentSetting(
      ContentSettingsType::LEGACY_COOKIE_ACCESS, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(net::CookieAccessSemantics::LEGACY,
            cookie_settings_->GetCookieAccessSemanticsForDomain(kDomain));
  EXPECT_EQ(net::CookieAccessSemantics::LEGACY,
            cookie_settings_->GetCookieAccessSemanticsForDomain(kDotDomain));
}

TEST_F(CookieSettingsTest, LegacyCookieAccessBlockAll) {
  settings_map_->SetDefaultContentSetting(
      ContentSettingsType::LEGACY_COOKIE_ACCESS, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(net::CookieAccessSemantics::NONLEGACY,
            cookie_settings_->GetCookieAccessSemanticsForDomain(kDomain));
  EXPECT_EQ(net::CookieAccessSemantics::NONLEGACY,
            cookie_settings_->GetCookieAccessSemanticsForDomain(kDotDomain));
}

TEST_F(CookieSettingsTest, LegacyCookieAccessAllowDomainPattern) {
  // Override the policy provider for this test, since the legacy cookie access
  // setting can only be set by policy.
  TestUtils::OverrideProvider(settings_map_.get(),
                              std::make_unique<MockProvider>(),
                              ProviderType::kPolicyProvider);
  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString(kDomain),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::LEGACY_COOKIE_ACCESS, CONTENT_SETTING_ALLOW);
  const struct {
    net::CookieAccessSemantics status;
    std::string cookie_domain;
  } kTestCases[] = {
      // These two test cases are LEGACY because they match the setting.
      {net::CookieAccessSemantics::LEGACY, kDomain},
      {net::CookieAccessSemantics::LEGACY, kDotDomain},
      // These two test cases default into NONLEGACY.
      // Subdomain does not match pattern.
      {net::CookieAccessSemantics::NONLEGACY, kSubDomain},
      {net::CookieAccessSemantics::NONLEGACY, kOtherDomain}};
  for (const auto& test : kTestCases) {
    EXPECT_EQ(test.status, cookie_settings_->GetCookieAccessSemanticsForDomain(
                               test.cookie_domain));
  }
}

TEST_F(CookieSettingsTest, LegacyCookieAccessAllowDomainWildcardPattern) {
  // Override the policy provider for this test, since the legacy cookie access
  // setting can only be set by policy.
  TestUtils::OverrideProvider(settings_map_.get(),
                              std::make_unique<MockProvider>(),
                              ProviderType::kPolicyProvider);
  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString(kDomainWildcardPattern),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::LEGACY_COOKIE_ACCESS, CONTENT_SETTING_ALLOW);
  const struct {
    net::CookieAccessSemantics status;
    std::string cookie_domain;
  } kTestCases[] = {
      // These three test cases are LEGACY because they match the setting.
      {net::CookieAccessSemantics::LEGACY, kDomain},
      {net::CookieAccessSemantics::LEGACY, kDotDomain},
      // Subdomain matches pattern.
      {net::CookieAccessSemantics::LEGACY, kSubDomain},
      // This test case defaults into NONLEGACY.
      {net::CookieAccessSemantics::NONLEGACY, kOtherDomain}};
  for (const auto& test : kTestCases) {
    EXPECT_EQ(test.status, cookie_settings_->GetCookieAccessSemanticsForDomain(
                               test.cookie_domain));
  }
}

TEST_F(CookieSettingsTest, GetStorageAccessStatus) {
  GURL url = kFirstPartySite;
  url::Origin top_frame_origin = url::Origin::Create(kAllowedSite);
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));

  EXPECT_EQ(cookie_settings_->GetStorageAccessStatus(
                url, net::SiteForCookies::FromUrl(url),
                url::Origin::Create(url), net::CookieSettingOverrides()),
            std::nullopt);

  EXPECT_EQ(
      cookie_settings_->GetStorageAccessStatus(
          url, net::SiteForCookies::FromUrl(url), url::Origin::Create(url),
          net::CookieSettingOverrides(
              {net::CookieSettingOverride::kStorageAccessGrantEligible})),
      std::nullopt);

  EXPECT_EQ(cookie_settings_->GetStorageAccessStatus(
                url, net::SiteForCookies(), top_frame_origin,
                net::CookieSettingOverrides()),
// We expect kActive when running the following in IOS due to the behavior of
// `CookieSettings::ShouldBlockThirdPartyCookiesInternal()`.
#if BUILDFLAG(IS_IOS)
            net::cookie_util::StorageAccessStatus::kActive
#else
            net::cookie_util::StorageAccessStatus::kNone
#endif
  );

  EXPECT_EQ(cookie_settings_->GetStorageAccessStatus(
                url, net::SiteForCookies(), top_frame_origin,
                net::CookieSettingOverrides(
                    {net::CookieSettingOverride::kStorageAccessGrantEligible})),
#if BUILDFLAG(IS_IOS)
            net::cookie_util::StorageAccessStatus::kActive
#else
            net::cookie_util::StorageAccessStatus::kNone
#endif
  );

  settings_map_->SetContentSettingDefaultScope(
      url, kAllowedSite, ContentSettingsType::STORAGE_ACCESS,
      CONTENT_SETTING_ALLOW);

  EXPECT_EQ(cookie_settings_->GetStorageAccessStatus(
                url, net::SiteForCookies(), top_frame_origin,
                net::CookieSettingOverrides()),
#if BUILDFLAG(IS_IOS)
            net::cookie_util::StorageAccessStatus::kActive
#else
            net::cookie_util::StorageAccessStatus::kInactive
#endif
  );

  EXPECT_EQ(cookie_settings_->GetStorageAccessStatus(
                url, net::SiteForCookies(), top_frame_origin,
                net::CookieSettingOverrides(
                    {net::CookieSettingOverride::kStorageAccessGrantEligible})),
            net::cookie_util::StorageAccessStatus::kActive);

  EXPECT_EQ(cookie_settings_->GetStorageAccessStatus(
                url, net::SiteForCookies(), top_frame_origin,
                net::CookieSettingOverrides(
                    {net::CookieSettingOverride::
                         kStorageAccessGrantEligibleViaHeader})),
            net::cookie_util::StorageAccessStatus::kActive);
}

// NOTE: These tests will fail if their FINAL name is of length greater than 256
// characters. Thus, try to avoid (unnecessary) generalized parameterization
// when possible.
std::string CustomTestName(
    const testing::TestParamInfo<CookieSettingsTestP::ParamType>& info) {
  std::stringstream custom_test_name;
  custom_test_name << "GrantSource_" << info.param;
  return custom_test_name.str();
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    CookieSettingsTestP,
#if BUILDFLAG(IS_IOS)
    testing::Values(GrantSource::kNoneGranted),
#else
    testing::Range(GrantSource::kNoneGranted, GrantSource::kGrantSourceCount),
#endif
    CustomTestName);

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    AreThirdPartyCookiesLimited,
#if BUILDFLAG(IS_IOS)
    testing::Values(GrantSource::kNoneGranted),
#else
    testing::Range(GrantSource::kNoneGranted, GrantSource::kGrantSourceCount),
#endif
    CustomTestName);

#if !BUILDFLAG(IS_IOS)
class CookieSettingsTopLevelTpcdTrialTest
    : public CookieSettingsTestBase,
      public testing::
          WithParamInterface</*net::features::kTopLevelTpcdTrialSettings:*/
                             bool> {
 public:
  CookieSettingsTopLevelTpcdTrialTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (IsTopLevel3pcdTrialEligible()) {
      enabled_features.push_back(net::features::kTopLevelTpcdTrialSettings);
    } else {
      disabled_features.push_back(net::features::kTopLevelTpcdTrialSettings);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool IsTopLevel3pcdTrialEligible() const { return GetParam(); }

  net::CookieSettingOverrides GetCookieSettingOverrides() const {
    net::CookieSettingOverrides overrides;
    return overrides;
  }

  // Assumes the cookie access would be blocked if not for a
  // `ContentSettingsType::TOP_LEVEL_TPCD_TRIAL` setting.
  ContentSetting SettingWithTopLevel3pcdTrialSetting() const {
    return IsTopLevel3pcdTrialEligible() ? CONTENT_SETTING_ALLOW
                                         : CONTENT_SETTING_BLOCK;
  }

  // Assumes the storage access result would be blocked if not for a
  // `ContentSettingsType::TOP_LEVEL_TPCD_TRIAL` setting.
  net::cookie_util::StorageAccessResult
  BlockedStorageAccessResultWithTopLevel3pcdTrialSetting() const {
    if (IsTopLevel3pcdTrialEligible()) {
      return net::cookie_util::StorageAccessResult::
          ACCESS_ALLOWED_TOP_LEVEL_3PCD_TRIAL;
    }
    return net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
  }

  void AddSettingForTopLevelTpcdTrial(GURL top_level_url,
                                      ContentSetting setting) {
    // Top-level 3pcd trial settings use
    // |WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE| by default and as a result
    // only use a primary pattern (with wildcard placeholder for the secondary
    // pattern).
    settings_map_->SetContentSettingDefaultScope(
        top_level_url, GURL(), ContentSettingsType::TOP_LEVEL_TPCD_TRIAL,
        CONTENT_SETTING_ALLOW);
  }
};

TEST_P(CookieSettingsTopLevelTpcdTrialTest, GetCookieSettingTopLevel3pcdTrial) {
  const GURL top_level_url(kFirstPartySite);
  const GURL url(kAllowedSite);
  const GURL third_url(kBlockedSite);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  prefs_.SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);

  AddSettingForTopLevelTpcdTrial(top_level_url, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            SettingWithTopLevel3pcdTrialSetting());
  histogram_tester.ExpectUniqueSample(
      kAllowedRequestsHistogram,
      BlockedStorageAccessResultWithTopLevel3pcdTrialSetting(), 1);

  // Check override trial setting.
  auto overrides = GetCookieSettingOverrides();
  overrides.Put(net::CookieSettingOverride::kSkipTopLevelTPCDTrial);
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, net::SiteForCookies(), top_level_url, overrides, nullptr),
            CONTENT_SETTING_BLOCK);

  // Valid pairs where |top_level_url| grants access to other urls.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                third_url, net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            SettingWithTopLevel3pcdTrialSetting());
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                GURL(), net::SiteForCookies(), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            SettingWithTopLevel3pcdTrialSetting());

  // Invalid pair where the |top_level_url| granting access to embedded sites is
  // now being loaded under |url| as the top level url.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                top_level_url, net::SiteForCookies(), url,
                GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  // Invalid pairs where a |third_url| is used as the top-level url.
  EXPECT_EQ(
      cookie_settings_->GetCookieSetting(url, net::SiteForCookies(), third_url,
                                         GetCookieSettingOverrides(), nullptr),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                top_level_url, net::SiteForCookies(), third_url,
                GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         CookieSettingsTopLevelTpcdTrialTest,
                         testing::Bool());

#endif

#if !BUILDFLAG(IS_IOS)
class CookieSettingsTopLevelTpcdOriginTrialTest
    : public CookieSettingsTestBase {
 public:
  CookieSettingsTopLevelTpcdOriginTrialTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    enabled_features.push_back(net::features::kTpcdMetadataGrants);
    enabled_features.push_back(net::features::kTopLevelTpcdOriginTrial);

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void AddSettingForTopLevel3pcdOriginTrial(GURL top_level_url,
                                            ContentSetting setting) {
    // Top-level 3pcd origin trial settings use
    // `WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE` by default and as a result
    // only use a primary pattern (with wildcard placeholder for the secondary
    // pattern).
    settings_map_->SetContentSettingDefaultScope(
        top_level_url, GURL(), ContentSettingsType::TOP_LEVEL_TPCD_ORIGIN_TRIAL,
        CONTENT_SETTING_BLOCK);
  }
};

TEST_F(CookieSettingsTopLevelTpcdOriginTrialTest,
       GetCookieSetting3pcdOriginTrial) {
  const GURL top_level_url(kFirstPartySite);
  const GURL url(kAllowedSite);
  const GURL third_url(kBlockedSite);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  prefs_.SetBoolean(prefs::kTrackingProtection3pcdEnabled, false);

  // Verify third-party cookie access is blocked by a Top-level 3PCD origin
  // trial setting.
  AddSettingForTopLevel3pcdOriginTrial(top_level_url, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, net::SiteForCookies(), top_level_url,
                net::CookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
  histogram_tester.ExpectUniqueSample(
      kAllowedRequestsHistogram,
      static_cast<int>(net::cookie_util::StorageAccessResult::ACCESS_BLOCKED),
      1);

  // Add a mitigation setting (e.g., 3PCD metadata grant) to unblock third-party
  // cookies.
  ContentSettingsForOneType tpcd_metadata_grants;
  tpcd_metadata_grants.emplace_back(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      base::Value(ContentSetting::CONTENT_SETTING_ALLOW),
      content_settings::ProviderType::kNone, false);
  tpcd_metadata_manager_->SetGrantsForTesting(tpcd_metadata_grants);

  // Verify the mitigation setting unblocks cookies.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, net::SiteForCookies(), top_level_url,
                net::CookieSettingOverrides(), nullptr),
            CONTENT_SETTING_ALLOW);
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 2);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(net::cookie_util::StorageAccessResult::
                           ACCESS_ALLOWED_3PCD_METADATA_GRANT),
      1);

  // Check override mitigation setting.
  net::CookieSettingOverrides overrides;
  overrides.Put(net::CookieSettingOverride::kSkipTPCDMetadataGrant);
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, net::SiteForCookies(), top_level_url, overrides, nullptr),
            CONTENT_SETTING_BLOCK);

  // Invalid pair the `top_level_url` granting access to `url` is now being
  // loaded under `url` as the top level url.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                top_level_url, net::SiteForCookies(), url,
                net::CookieSettingOverrides(), nullptr),
            CONTENT_SETTING_ALLOW);

  // Invalid pairs where a `third_url` is used as the top-level url.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, net::SiteForCookies(), third_url,
                net::CookieSettingOverrides(), nullptr),
            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                top_level_url, net::SiteForCookies(), third_url,
                net::CookieSettingOverrides(), nullptr),
            CONTENT_SETTING_ALLOW);
}

#endif

}  // namespace content_settings
