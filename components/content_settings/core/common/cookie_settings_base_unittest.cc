// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/cookie_settings_base.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content_settings {
namespace {

constexpr char kDomain[] = "foo.com";
const GURL kURL = GURL(kDomain);
const url::Origin kOrigin = url::Origin::Create(kURL);
const net::SiteForCookies kSiteForCookies =
    net::SiteForCookies::FromOrigin(kOrigin);

using GetSettingCallback = base::RepeatingCallback<ContentSetting(const GURL&)>;

ContentSettingPatternSource CreateSetting(ContentSetting setting) {
  return ContentSettingPatternSource(
      ContentSettingsPattern::FromString(kDomain),
      ContentSettingsPattern::Wildcard(), base::Value(setting), std::string(),
      false);
}

ContentSettingPatternSource CreateThirdPartySetting(ContentSetting setting) {
  return ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString(kDomain), base::Value(setting),
      std::string(), false);
}

class CallbackCookieSettings : public CookieSettingsBase {
 public:
  explicit CallbackCookieSettings(GetSettingCallback callback)
      : callback_(std::move(callback)) {}

  ContentSetting GetContentSetting(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      content_settings::SettingInfo* info) const override {
    return callback_.Run(primary_url);
  }

  // CookieSettingsBase:
  bool ShouldAlwaysAllowCookies(const GURL& url,
                                const GURL& first_party_url) const override {
    return false;
  }

  bool ShouldBlockThirdPartyCookies() const override { return false; }

  bool IsThirdPartyCookiesAllowedScheme(
      const std::string& scheme) const override {
    return false;
  }

  bool IsStorageAccessApiEnabled() const override { return true; }

  bool ShouldIgnoreSameSiteRestrictions(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies) const override {
    NOTREACHED();
    return false;
  }

 private:
  GetSettingCallback callback_;
};

TEST(CookieSettingsBaseTest, ShouldDeleteSessionOnly) {
  CallbackCookieSettings settings(base::BindRepeating(
      [](const GURL&) { return CONTENT_SETTING_SESSION_ONLY; }));
  EXPECT_TRUE(settings.ShouldDeleteCookieOnExit({}, kDomain, false));
}

TEST(CookieSettingsBaseTest, ShouldNotDeleteAllowed) {
  CallbackCookieSettings settings(
      base::BindRepeating([](const GURL&) { return CONTENT_SETTING_ALLOW; }));
  EXPECT_FALSE(settings.ShouldDeleteCookieOnExit({}, kDomain, false));
}

TEST(CookieSettingsBaseTest, ShouldNotDeleteAllowedHttps) {
  CallbackCookieSettings settings(base::BindRepeating([](const GURL& url) {
    return url.SchemeIsCryptographic() ? CONTENT_SETTING_ALLOW
                                       : CONTENT_SETTING_BLOCK;
  }));
  EXPECT_FALSE(settings.ShouldDeleteCookieOnExit({}, kDomain, false));
  EXPECT_FALSE(settings.ShouldDeleteCookieOnExit({}, kDomain, true));
}

TEST(CookieSettingsBaseTest, ShouldDeleteDomainSettingSessionOnly) {
  CallbackCookieSettings settings(
      base::BindRepeating([](const GURL&) { return CONTENT_SETTING_BLOCK; }));
  EXPECT_TRUE(settings.ShouldDeleteCookieOnExit(
      {CreateSetting(CONTENT_SETTING_SESSION_ONLY)}, kDomain, false));
}

TEST(CookieSettingsBaseTest, ShouldDeleteDomainThirdPartySettingSessionOnly) {
  CallbackCookieSettings settings(
      base::BindRepeating([](const GURL&) { return CONTENT_SETTING_BLOCK; }));
  EXPECT_TRUE(settings.ShouldDeleteCookieOnExit(
      {CreateThirdPartySetting(CONTENT_SETTING_SESSION_ONLY)}, kDomain, false));
}

TEST(CookieSettingsBaseTest, ShouldNotDeleteDomainSettingAllow) {
  CallbackCookieSettings settings(
      base::BindRepeating([](const GURL&) { return CONTENT_SETTING_BLOCK; }));
  EXPECT_FALSE(settings.ShouldDeleteCookieOnExit(
      {CreateSetting(CONTENT_SETTING_ALLOW)}, kDomain, false));
}

TEST(CookieSettingsBaseTest,
     ShouldNotDeleteDomainSettingAllowAfterSessionOnly) {
  CallbackCookieSettings settings(
      base::BindRepeating([](const GURL&) { return CONTENT_SETTING_BLOCK; }));
  EXPECT_FALSE(settings.ShouldDeleteCookieOnExit(
      {CreateSetting(CONTENT_SETTING_SESSION_ONLY),
       CreateSetting(CONTENT_SETTING_ALLOW)},
      kDomain, false));
}

TEST(CookieSettingsBaseTest, ShouldNotDeleteDomainSettingBlock) {
  CallbackCookieSettings settings(
      base::BindRepeating([](const GURL&) { return CONTENT_SETTING_BLOCK; }));
  EXPECT_FALSE(settings.ShouldDeleteCookieOnExit(
      {CreateSetting(CONTENT_SETTING_BLOCK)}, kDomain, false));
}

TEST(CookieSettingsBaseTest, ShouldNotDeleteNoDomainMatch) {
  CallbackCookieSettings settings(
      base::BindRepeating([](const GURL&) { return CONTENT_SETTING_BLOCK; }));
  EXPECT_FALSE(settings.ShouldDeleteCookieOnExit(
      {CreateSetting(CONTENT_SETTING_SESSION_ONLY)}, "other.com", false));
}

TEST(CookieSettingsBaseTest, ShouldNotDeleteNoThirdPartyDomainMatch) {
  CallbackCookieSettings settings(
      base::BindRepeating([](const GURL&) { return CONTENT_SETTING_BLOCK; }));
  EXPECT_FALSE(settings.ShouldDeleteCookieOnExit(
      {CreateThirdPartySetting(CONTENT_SETTING_SESSION_ONLY)}, "other.com",
      false));
}

TEST(CookieSettingsBaseTest, CookieAccessNotAllowedWithBlockedSetting) {
  CallbackCookieSettings settings(
      base::BindRepeating([](const GURL&) { return CONTENT_SETTING_BLOCK; }));
  EXPECT_FALSE(settings.IsFullCookieAccessAllowed(
      kURL, kSiteForCookies, kOrigin, net::CookieSettingOverrides()));
}

TEST(CookieSettingsBaseTest, CookieAccessAllowedWithAllowSetting) {
  CallbackCookieSettings settings(
      base::BindRepeating([](const GURL&) { return CONTENT_SETTING_ALLOW; }));
  EXPECT_TRUE(settings.IsFullCookieAccessAllowed(
      kURL, kSiteForCookies, kOrigin, net::CookieSettingOverrides()));
}

TEST(CookieSettingsBaseTest, CookieAccessAllowedWithSessionOnlySetting) {
  CallbackCookieSettings settings(base::BindRepeating(
      [](const GURL&) { return CONTENT_SETTING_SESSION_ONLY; }));
  EXPECT_TRUE(settings.IsFullCookieAccessAllowed(
      kURL, kSiteForCookies, kOrigin, net::CookieSettingOverrides()));
}

TEST(CookieSettingsBaseTest, LegacyCookieAccessSemantics) {
  CallbackCookieSettings settings1(
      base::BindRepeating([](const GURL&) { return CONTENT_SETTING_ALLOW; }));
  EXPECT_EQ(net::CookieAccessSemantics::LEGACY,
            settings1.GetCookieAccessSemanticsForDomain(std::string()));
  CallbackCookieSettings settings2(
      base::BindRepeating([](const GURL&) { return CONTENT_SETTING_BLOCK; }));
  EXPECT_EQ(net::CookieAccessSemantics::NONLEGACY,
            settings2.GetCookieAccessSemanticsForDomain(std::string()));
}

TEST(CookieSettingsBaseTest, IsCookieSessionOnlyWithAllowSetting) {
  CallbackCookieSettings settings(
      base::BindRepeating([](const GURL&) { return CONTENT_SETTING_ALLOW; }));
  EXPECT_FALSE(settings.IsCookieSessionOnly(kURL));
}

TEST(CookieSettingsBaseTest, IsCookieSessionOnlyWithBlockSetting) {
  CallbackCookieSettings settings(
      base::BindRepeating([](const GURL&) { return CONTENT_SETTING_BLOCK; }));
  EXPECT_FALSE(settings.IsCookieSessionOnly(kURL));
}

TEST(CookieSettingsBaseTest, IsCookieSessionOnlySessionWithOnlySetting) {
  CallbackCookieSettings settings(base::BindRepeating(
      [](const GURL&) { return CONTENT_SETTING_SESSION_ONLY; }));
  EXPECT_TRUE(settings.IsCookieSessionOnly(kURL));
}

TEST(CookieSettingsBaseTest, IsValidSetting) {
  EXPECT_FALSE(CookieSettingsBase::IsValidSetting(CONTENT_SETTING_DEFAULT));
  EXPECT_FALSE(CookieSettingsBase::IsValidSetting(CONTENT_SETTING_ASK));
  EXPECT_TRUE(CookieSettingsBase::IsValidSetting(CONTENT_SETTING_ALLOW));
  EXPECT_TRUE(CookieSettingsBase::IsValidSetting(CONTENT_SETTING_BLOCK));
  EXPECT_TRUE(CookieSettingsBase::IsValidSetting(CONTENT_SETTING_SESSION_ONLY));
}

TEST(CookieSettingsBaseTest, IsAllowed) {
  EXPECT_FALSE(CookieSettingsBase::IsAllowed(CONTENT_SETTING_BLOCK));
  EXPECT_TRUE(CookieSettingsBase::IsAllowed(CONTENT_SETTING_ALLOW));
  EXPECT_TRUE(CookieSettingsBase::IsAllowed(CONTENT_SETTING_SESSION_ONLY));
}

TEST(CookieSettingsBaseTest, IsValidLegacyAccessSetting) {
  EXPECT_FALSE(CookieSettingsBase::IsValidSettingForLegacyAccess(
      CONTENT_SETTING_DEFAULT));
  EXPECT_FALSE(
      CookieSettingsBase::IsValidSettingForLegacyAccess(CONTENT_SETTING_ASK));
  EXPECT_TRUE(
      CookieSettingsBase::IsValidSettingForLegacyAccess(CONTENT_SETTING_ALLOW));
  EXPECT_TRUE(
      CookieSettingsBase::IsValidSettingForLegacyAccess(CONTENT_SETTING_BLOCK));
  EXPECT_FALSE(CookieSettingsBase::IsValidSettingForLegacyAccess(
      CONTENT_SETTING_SESSION_ONLY));
}

class CookieSettingsBaseStorageAccessAPITest
    : public testing::TestWithParam<std::tuple<bool, bool>> {
 public:
  CookieSettingsBaseStorageAccessAPITest() {
    CookieSettingsBase::SetStorageAccessAPIGrantsUnpartitionedStorageForTesting(
        PermissionGrantsUnpartitionedStorage());

    std::vector<base::test::FeatureRefAndParams> enabled;
    std::vector<base::test::FeatureRef> disabled;
    if (IsStoragePartitioned()) {
      enabled.push_back({net::features::kThirdPartyStoragePartitioning, {}});
    } else {
      disabled.push_back(net::features::kThirdPartyStoragePartitioning);
    }
    features_.InitWithFeaturesAndParameters(enabled, disabled);
  }

  bool PermissionGrantsUnpartitionedStorage() const {
    return std::get<0>(GetParam());
  }
  bool IsStoragePartitioned() const { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_P(CookieSettingsBaseStorageAccessAPITest,
       SettingOverridesForStorageAccessAPIs) {
  CallbackCookieSettings settings(
      base::BindRepeating([](const GURL&) { return CONTENT_SETTING_ALLOW; }));

  net::CookieSettingOverrides overrides = settings.SettingOverridesForStorage();

  EXPECT_EQ(
      overrides.Has(net::CookieSettingOverride::kStorageAccessGrantEligible),
      PermissionGrantsUnpartitionedStorage() || IsStoragePartitioned());
  EXPECT_EQ(
      overrides.Has(
          net::CookieSettingOverride::kTopLevelStorageAccessGrantEligible),
      IsStoragePartitioned());
  EXPECT_EQ(overrides.Has(net::CookieSettingOverride::k3pcdSupport),
            IsStoragePartitioned());
  EXPECT_EQ(
      overrides.Has(net::CookieSettingOverride::k3pcdMetadataGrantEligible),
      IsStoragePartitioned());
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    CookieSettingsBaseStorageAccessAPITest,
    testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace
}  // namespace content_settings
