// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/cookie_settings.h"

#include "base/scoped_observer.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/features.h"
#include "net/cookies/cookie_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content_settings {

namespace {

class CookieSettingsObserver : public CookieSettings::Observer {
 public:
  CookieSettingsObserver(CookieSettings* settings) : settings_(settings) {
    scoped_observer_.Add(settings);
  }

  void OnThirdPartyCookieBlockingChanged(
      bool block_third_party_cookies) override {
    ASSERT_EQ(block_third_party_cookies,
              settings_->ShouldBlockThirdPartyCookies());
    last_value_ = block_third_party_cookies;
  }

  bool last_value() { return last_value_; }

 private:
  CookieSettings* settings_;
  bool last_value_ = false;
  ScopedObserver<CookieSettings, CookieSettings::Observer> scoped_observer_{
      this};

  DISALLOW_COPY_AND_ASSIGN(CookieSettingsObserver);
};

class CookieSettingsTest : public testing::Test {
 public:
  CookieSettingsTest()
      : kBlockedSite("http://ads.thirdparty.com"),
        kAllowedSite("http://good.allays.com"),
        kFirstPartySite("http://cool.things.com"),
        kChromeURL("chrome://foo"),
        kExtensionURL("chrome-extension://deadbeef"),
        kDomain("example.com"),
        kDotDomain(".example.com"),
        kSubDomain("www.example.com"),
        kOtherDomain("www.not-example.com"),
        kDomainWildcardPattern("[*.]example.com"),
        kHttpSite("http://example.com"),
        kHttpsSite("https://example.com"),
        kHttpsSubdomainSite("https://www.example.com"),
        kHttpsSite8080("https://example.com:8080"),
        kAllHttpsSitesPattern(ContentSettingsPattern::FromString("https://*")) {
    feature_list_.InitAndDisableFeature(
        net::features::kSameSiteByDefaultCookies);
  }

  ~CookieSettingsTest() override { settings_map_->ShutdownOnUIThread(); }

  void SetUp() override {
    ContentSettingsRegistry::GetInstance()->ResetForTest();
    CookieSettings::RegisterProfilePrefs(prefs_.registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    settings_map_ = new HostContentSettingsMap(
        &prefs_, false /* is_off_the_record */, false /* store_last_modified */,
        false /* migrate_requesting_and_top_level_origin_settings */);
    cookie_settings_ = new CookieSettings(settings_map_.get(), &prefs_, false,
                                          "chrome-extension");
    cookie_settings_incognito_ = new CookieSettings(
        settings_map_.get(), &prefs_, true, "chrome-extension");
  }

 protected:
  bool ShouldDeleteCookieOnExit(const std::string& domain, bool is_https) {
    ContentSettingsForOneType settings;
    cookie_settings_->GetCookieSettings(&settings);
    return cookie_settings_->ShouldDeleteCookieOnExit(settings, domain,
                                                      is_https);
  }

  // There must be a valid ThreadTaskRunnerHandle in HostContentSettingsMap's
  // scope.
  base::test::SingleThreadTaskEnvironment task_environment_;

  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
  scoped_refptr<CookieSettings> cookie_settings_;
  scoped_refptr<CookieSettings> cookie_settings_incognito_;
  const GURL kBlockedSite;
  const GURL kAllowedSite;
  const GURL kFirstPartySite;
  const GURL kChromeURL;
  const GURL kExtensionURL;
  const std::string kDomain;
  const std::string kDotDomain;
  const std::string kSubDomain;
  const std::string kOtherDomain;
  const std::string kDomainWildcardPattern;
  const GURL kHttpSite;
  const GURL kHttpsSite;
  const GURL kHttpsSubdomainSite;
  const GURL kHttpsSite8080;
  ContentSettingsPattern kAllHttpsSitesPattern;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(CookieSettingsTest, TestWhitelistedScheme) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(cookie_settings_->IsCookieAccessAllowed(kHttpSite, kChromeURL));
  EXPECT_TRUE(cookie_settings_->IsCookieAccessAllowed(kHttpsSite, kChromeURL));
  EXPECT_TRUE(cookie_settings_->IsCookieAccessAllowed(kChromeURL, kHttpSite));
#if BUILDFLAG(ENABLE_EXTENSIONS)
  EXPECT_TRUE(
      cookie_settings_->IsCookieAccessAllowed(kExtensionURL, kExtensionURL));
#else
  EXPECT_FALSE(
      cookie_settings_->IsCookieAccessAllowed(kExtensionURL, kExtensionURL));
#endif
  EXPECT_FALSE(
      cookie_settings_->IsCookieAccessAllowed(kExtensionURL, kHttpSite));
}

TEST_F(CookieSettingsTest, CookiesBlockSingle) {
  cookie_settings_->SetCookieSetting(kBlockedSite, CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(
      cookie_settings_->IsCookieAccessAllowed(kBlockedSite, kBlockedSite));
}

TEST_F(CookieSettingsTest, CookiesBlockThirdParty) {
  prefs_.SetBoolean(prefs::kBlockThirdPartyCookies, true);
  EXPECT_FALSE(
      cookie_settings_->IsCookieAccessAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));
}

// Test fixture with ImprovedCookieControls enabled.
class ImprovedCookieControlsCookieSettingsTest : public CookieSettingsTest {
 public:
  ImprovedCookieControlsCookieSettingsTest() : CookieSettingsTest() {
    feature_list_.InitAndEnableFeature(kImprovedCookieControls);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ImprovedCookieControlsCookieSettingsTest, CookiesControlsDefault) {
  EXPECT_TRUE(
      cookie_settings_->IsCookieAccessAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings_incognito_->IsCookieAccessAllowed(
      kBlockedSite, kFirstPartySite));
}

TEST_F(ImprovedCookieControlsCookieSettingsTest, CookiesControlsEnabled) {
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kOn));
  EXPECT_FALSE(
      cookie_settings_->IsCookieAccessAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings_incognito_->IsCookieAccessAllowed(
      kBlockedSite, kFirstPartySite));
}

TEST_F(ImprovedCookieControlsCookieSettingsTest, CookiesControlsDisabled) {
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kOff));
  EXPECT_TRUE(
      cookie_settings_->IsCookieAccessAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings_incognito_->IsCookieAccessAllowed(
      kBlockedSite, kFirstPartySite));
}

TEST_F(ImprovedCookieControlsCookieSettingsTest,
       CookiesControlsEnabledForIncognito) {
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kIncognitoOnly));
  EXPECT_TRUE(
      cookie_settings_->IsCookieAccessAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings_incognito_->IsCookieAccessAllowed(
      kBlockedSite, kFirstPartySite));
}

TEST_F(CookieSettingsTest, CookiesControlsEnabledButFeatureDisabled) {
  EXPECT_TRUE(
      cookie_settings_->IsCookieAccessAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings_incognito_->IsCookieAccessAllowed(
      kBlockedSite, kFirstPartySite));
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kOn));
  EXPECT_TRUE(
      cookie_settings_->IsCookieAccessAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings_incognito_->IsCookieAccessAllowed(
      kBlockedSite, kFirstPartySite));
}

TEST_F(CookieSettingsTest, CookiesAllowThirdParty) {
  EXPECT_TRUE(
      cookie_settings_->IsCookieAccessAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));
}

TEST_F(CookieSettingsTest, CookiesExplicitBlockSingleThirdParty) {
  cookie_settings_->SetCookieSetting(kBlockedSite, CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(
      cookie_settings_->IsCookieAccessAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(
      cookie_settings_->IsCookieAccessAllowed(kAllowedSite, kFirstPartySite));
}

TEST_F(CookieSettingsTest, CookiesExplicitSessionOnly) {
  cookie_settings_->SetCookieSetting(kBlockedSite,
                                     CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(
      cookie_settings_->IsCookieAccessAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));

  prefs_.SetBoolean(prefs::kBlockThirdPartyCookies, true);
  EXPECT_TRUE(
      cookie_settings_->IsCookieAccessAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));
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

TEST_F(CookieSettingsTest, CookiesThirdPartyBlockedExplicitAllow) {
  cookie_settings_->SetCookieSetting(kAllowedSite, CONTENT_SETTING_ALLOW);
  prefs_.SetBoolean(prefs::kBlockThirdPartyCookies, true);
  EXPECT_TRUE(
      cookie_settings_->IsCookieAccessAllowed(kAllowedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));

  // Extensions should always be allowed to use cookies.
  EXPECT_TRUE(
      cookie_settings_->IsCookieAccessAllowed(kAllowedSite, kExtensionURL));
}

TEST_F(CookieSettingsTest, CookiesThirdPartyBlockedAllSitesAllowed) {
  cookie_settings_->SetCookieSetting(kAllowedSite, CONTENT_SETTING_ALLOW);
  prefs_.SetBoolean(prefs::kBlockThirdPartyCookies, true);
  // As an example for a url that matches all hosts but not all origins,
  // match all HTTPS sites.
  settings_map_->SetContentSettingCustomScope(
      kAllHttpsSitesPattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, std::string(), CONTENT_SETTING_ALLOW);
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);

  // |kAllowedSite| should be allowed.
  EXPECT_TRUE(
      cookie_settings_->IsCookieAccessAllowed(kAllowedSite, kBlockedSite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));

  // HTTPS sites should be allowed in a first-party context.
  EXPECT_TRUE(cookie_settings_->IsCookieAccessAllowed(kHttpsSite, kHttpsSite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));

  // HTTP sites should be allowed, but session-only.
  EXPECT_TRUE(cookie_settings_->IsCookieAccessAllowed(kFirstPartySite,
                                                      kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsCookieSessionOnly(kFirstPartySite));

  // Third-party cookies should be blocked.
  EXPECT_FALSE(
      cookie_settings_->IsCookieAccessAllowed(kFirstPartySite, kBlockedSite));
  EXPECT_FALSE(
      cookie_settings_->IsCookieAccessAllowed(kHttpsSite, kBlockedSite));
}

TEST_F(CookieSettingsTest, CookiesBlockEverything) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  EXPECT_FALSE(cookie_settings_->IsCookieAccessAllowed(kFirstPartySite,
                                                       kFirstPartySite));
  EXPECT_FALSE(
      cookie_settings_->IsCookieAccessAllowed(kAllowedSite, kFirstPartySite));
}

TEST_F(CookieSettingsTest, CookiesBlockEverythingExceptAllowed) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  cookie_settings_->SetCookieSetting(kAllowedSite, CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(cookie_settings_->IsCookieAccessAllowed(kFirstPartySite,
                                                       kFirstPartySite));
  EXPECT_TRUE(
      cookie_settings_->IsCookieAccessAllowed(kAllowedSite, kFirstPartySite));
  EXPECT_TRUE(
      cookie_settings_->IsCookieAccessAllowed(kAllowedSite, kAllowedSite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));
}

TEST_F(CookieSettingsTest, ExtensionsRegularSettings) {
  cookie_settings_->SetCookieSetting(kBlockedSite, CONTENT_SETTING_BLOCK);

  // Regular cookie settings also apply to extensions.
  EXPECT_FALSE(
      cookie_settings_->IsCookieAccessAllowed(kBlockedSite, kExtensionURL));
}

TEST_F(CookieSettingsTest, ExtensionsOwnCookies) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Extensions can always use cookies (and site data) in their own origin.
  EXPECT_TRUE(
      cookie_settings_->IsCookieAccessAllowed(kExtensionURL, kExtensionURL));
#else
  // Except if extensions are disabled. Then the extension-specific checks do
  // not exist and the default setting is to block.
  EXPECT_FALSE(
      cookie_settings_->IsCookieAccessAllowed(kExtensionURL, kExtensionURL));
#endif
}

TEST_F(CookieSettingsTest, ExtensionsThirdParty) {
  prefs_.SetBoolean(prefs::kBlockThirdPartyCookies, true);

  // XHRs stemming from extensions are exempt from third-party cookie blocking
  // rules (as the first party is always the extension's security origin).
  EXPECT_TRUE(
      cookie_settings_->IsCookieAccessAllowed(kBlockedSite, kExtensionURL));
}

TEST_F(CookieSettingsTest, ThirdPartyException) {
  EXPECT_TRUE(cookie_settings_->IsThirdPartyAccessAllowed(kFirstPartySite));
  EXPECT_TRUE(
      cookie_settings_->IsCookieAccessAllowed(kHttpsSite, kFirstPartySite));

  prefs_.SetBoolean(prefs::kBlockThirdPartyCookies, true);
  EXPECT_FALSE(cookie_settings_->IsThirdPartyAccessAllowed(kFirstPartySite));
  EXPECT_FALSE(
      cookie_settings_->IsCookieAccessAllowed(kHttpsSite, kFirstPartySite));

  cookie_settings_->SetThirdPartyCookieSetting(kFirstPartySite,
                                               CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(cookie_settings_->IsThirdPartyAccessAllowed(kFirstPartySite));
  EXPECT_TRUE(
      cookie_settings_->IsCookieAccessAllowed(kHttpsSite, kFirstPartySite));

  cookie_settings_->ResetThirdPartyCookieSetting(kFirstPartySite);
  EXPECT_FALSE(cookie_settings_->IsThirdPartyAccessAllowed(kFirstPartySite));
  EXPECT_FALSE(
      cookie_settings_->IsCookieAccessAllowed(kHttpsSite, kFirstPartySite));

  cookie_settings_->SetCookieSetting(kHttpsSite, CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(cookie_settings_->IsThirdPartyAccessAllowed(kFirstPartySite));
  EXPECT_TRUE(
      cookie_settings_->IsCookieAccessAllowed(kHttpsSite, kFirstPartySite));
}

TEST_F(CookieSettingsTest, ThirdPartySettingObserver) {
  CookieSettingsObserver observer(cookie_settings_.get());
  EXPECT_FALSE(observer.last_value());
  prefs_.SetBoolean(prefs::kBlockThirdPartyCookies, true);
  EXPECT_TRUE(observer.last_value());
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

// Test SameSite-by-default disabled (default semantics is LEGACY)
// TODO(crbug.com/953306): Remove this when legacy code path is removed.
TEST_F(CookieSettingsTest,
       LegacyCookieAccessAllowDomainPattern_SameSiteByDefaultDisabled) {
  // Override the policy provider for this test, since the legacy cookie access
  // setting can only be set by policy.
  TestUtils::OverrideProvider(
      settings_map_.get(), std::make_unique<MockProvider>(),
      HostContentSettingsMap::ProviderType::POLICY_PROVIDER);
  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString(kDomain),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::LEGACY_COOKIE_ACCESS, std::string(),
      CONTENT_SETTING_BLOCK);
  const struct {
    net::CookieAccessSemantics status;
    std::string cookie_domain;
  } kTestCases[] = {
      // These two test cases are NONLEGACY because they match the setting.
      {net::CookieAccessSemantics::NONLEGACY, kDomain},
      {net::CookieAccessSemantics::NONLEGACY, kDotDomain},
      // These two test cases default into LEGACY.
      // Subdomain does not match pattern.
      {net::CookieAccessSemantics::LEGACY, kSubDomain},
      {net::CookieAccessSemantics::LEGACY, kOtherDomain}};
  for (const auto& test : kTestCases) {
    EXPECT_EQ(test.status, cookie_settings_->GetCookieAccessSemanticsForDomain(
                               test.cookie_domain));
  }
}

// Test SameSite-by-default disabled (default semantics is LEGACY)
// TODO(crbug.com/953306): Remove this when legacy code path is removed.
TEST_F(CookieSettingsTest,
       LegacyCookieAccessAllowDomainWildcardPattern_SameSiteByDefaultDisabled) {
  // Override the policy provider for this test, since the legacy cookie access
  // setting can only be set by policy.
  TestUtils::OverrideProvider(
      settings_map_.get(), std::make_unique<MockProvider>(),
      HostContentSettingsMap::ProviderType::POLICY_PROVIDER);
  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString(kDomainWildcardPattern),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::LEGACY_COOKIE_ACCESS, std::string(),
      CONTENT_SETTING_BLOCK);
  const struct {
    net::CookieAccessSemantics status;
    std::string cookie_domain;
  } kTestCases[] = {
      // These three test cases are NONLEGACY because they match the setting.
      {net::CookieAccessSemantics::NONLEGACY, kDomain},
      {net::CookieAccessSemantics::NONLEGACY, kDotDomain},
      // Subdomain matches pattern.
      {net::CookieAccessSemantics::NONLEGACY, kSubDomain},
      // This test case defaults into LEGACY.
      {net::CookieAccessSemantics::LEGACY, kOtherDomain}};
  for (const auto& test : kTestCases) {
    EXPECT_EQ(test.status, cookie_settings_->GetCookieAccessSemanticsForDomain(
                               test.cookie_domain));
  }
}

// Test fixture with SameSiteByDefaultCookies enabled.
class SameSiteByDefaultCookieSettingsTest : public CookieSettingsTest {
 public:
  SameSiteByDefaultCookieSettingsTest() : CookieSettingsTest() {
    feature_list_.InitAndEnableFeature(
        net::features::kSameSiteByDefaultCookies);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test SameSite-by-default enabled (default semantics is NONLEGACY)
TEST_F(SameSiteByDefaultCookieSettingsTest,
       LegacyCookieAccessAllowDomainPattern_SameSiteByDefaultEnabled) {
  // Override the policy provider for this test, since the legacy cookie access
  // setting can only be set by policy.
  TestUtils::OverrideProvider(
      settings_map_.get(), std::make_unique<MockProvider>(),
      HostContentSettingsMap::ProviderType::POLICY_PROVIDER);
  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString(kDomain),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::LEGACY_COOKIE_ACCESS, std::string(),
      CONTENT_SETTING_ALLOW);
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

// Test SameSite-by-default enabled (default semantics is NONLEGACY)
TEST_F(SameSiteByDefaultCookieSettingsTest,
       LegacyCookieAccessAllowDomainWildcardPattern_SameSiteByDefaultEnabled) {
  // Override the policy provider for this test, since the legacy cookie access
  // setting can only be set by policy.
  TestUtils::OverrideProvider(
      settings_map_.get(), std::make_unique<MockProvider>(),
      HostContentSettingsMap::ProviderType::POLICY_PROVIDER);
  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString(kDomainWildcardPattern),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::LEGACY_COOKIE_ACCESS, std::string(),
      CONTENT_SETTING_ALLOW);
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

}  // namespace

}  // namespace content_settings
