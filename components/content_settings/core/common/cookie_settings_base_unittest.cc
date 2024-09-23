// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/cookie_settings_base.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "net/base/features.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content_settings {
namespace {

constexpr char kDomain[] = "foo.com";

using GetSettingCallback = base::RepeatingCallback<
    ContentSetting(const GURL&, ContentSettingsType, SettingInfo*)>;

ContentSettingPatternSource CreateSetting(ContentSetting setting) {
  return ContentSettingPatternSource(
      ContentSettingsPattern::FromString(kDomain),
      ContentSettingsPattern::Wildcard(), base::Value(setting),
      ProviderType::kNone, false);
}

ContentSettingPatternSource CreateThirdPartySetting(ContentSetting setting) {
  return ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString(kDomain), base::Value(setting),
      ProviderType::kNone, false);
}

class CallbackCookieSettings : public CookieSettingsBase {
 public:
  explicit CallbackCookieSettings(GetSettingCallback callback)
      : callback_(std::move(callback)) {}

  // A simple constructor that returns a specified setting for COOKIES, ALLOW
  // for TOP_LEVEL_TPCD_ORIGIN_TRIAL, and BLOCK otherwise.
  explicit CallbackCookieSettings(ContentSetting setting)
      : callback_(base::BindLambdaForTesting(
            [setting](const GURL&, ContentSettingsType type, SettingInfo*) {
              if (type == ContentSettingsType::COOKIES) {
                return setting;
              }

              if (type == ContentSettingsType::TOP_LEVEL_TPCD_ORIGIN_TRIAL) {
                return CONTENT_SETTING_ALLOW;
              }

              return CONTENT_SETTING_BLOCK;
            })) {}

  ContentSetting GetContentSetting(const GURL& primary_url,
                                   const GURL& secondary_url,
                                   ContentSettingsType content_type,
                                   SettingInfo* info) const override {
    if (info) {
      info->primary_pattern = ContentSettingsPattern::Wildcard();
      info->secondary_pattern = ContentSettingsPattern::Wildcard();
    }
    return callback_.Run(primary_url, content_type, info);
  }

  // CookieSettingsBase:
  bool ShouldAlwaysAllowCookies(const GURL& url,
                                const GURL& first_party_url) const override {
    return false;
  }

  bool ShouldBlockThirdPartyCookies() const override { return false; }
  bool MitigationsEnabledFor3pcd() const override { return false; }

  bool IsThirdPartyCookiesAllowedScheme(
      const std::string& scheme) const override {
    return false;
  }

  bool ShouldIgnoreSameSiteRestrictions(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies) const override {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

 private:
  GetSettingCallback callback_;
  ContentSettingsType type_;
};

class CookieSettingsBaseTest : public testing::Test {
 public:
  CookieSettingsBaseTest()
      : url_(base::StrCat({"https://", kDomain})),
        origin_(url::Origin::Create(url_)),
        site_for_cookies_(net::SiteForCookies::FromOrigin(origin_)) {
    EXPECT_FALSE(origin_.opaque());
  }

 protected:
  const GURL url_;
  const url::Origin origin_;
  const net::SiteForCookies site_for_cookies_;
};

TEST_F(CookieSettingsBaseTest, ShouldDeleteSessionOnly) {
  CallbackCookieSettings settings(
      base::BindRepeating([](const GURL&, ContentSettingsType, SettingInfo*) {
        return CONTENT_SETTING_SESSION_ONLY;
      }));

  EXPECT_TRUE(settings.ShouldDeleteCookieOnExit(
      {}, kDomain, net::CookieSourceScheme::kNonSecure));
}

TEST_F(CookieSettingsBaseTest, ShouldNotDeleteAllowed) {
  CallbackCookieSettings settings(CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(settings.ShouldDeleteCookieOnExit(
      {}, kDomain, net::CookieSourceScheme::kNonSecure));
}

TEST_F(CookieSettingsBaseTest, ShouldNotDeleteAllowedHttps) {
  base::test::ScopedFeatureList features_;
  features_.InitAndDisableFeature(net::features::kEnableSchemeBoundCookies);
  CallbackCookieSettings settings(base::BindRepeating(
      [](const GURL& url, ContentSettingsType, SettingInfo*) {
        return url.SchemeIsCryptographic() ? CONTENT_SETTING_ALLOW
                                           : CONTENT_SETTING_BLOCK;
      }));
  EXPECT_FALSE(settings.ShouldDeleteCookieOnExit(
      {}, kDomain, net::CookieSourceScheme::kNonSecure));
  EXPECT_FALSE(settings.ShouldDeleteCookieOnExit(
      {}, kDomain, net::CookieSourceScheme::kSecure));
}

TEST_F(CookieSettingsBaseTest,
       ShouldDeleteIsSchemeAwareWithSchemeBoundCookies) {
  base::test::ScopedFeatureList features_;
  features_.InitAndEnableFeature(net::features::kEnableSchemeBoundCookies);
  CallbackCookieSettings settings(base::BindRepeating(
      [](const GURL& url, ContentSettingsType, SettingInfo*) {
        return url.SchemeIsCryptographic() ? CONTENT_SETTING_ALLOW
                                           : CONTENT_SETTING_SESSION_ONLY;
      }));
  EXPECT_TRUE(settings.ShouldDeleteCookieOnExit(
      {}, kDomain, net::CookieSourceScheme::kNonSecure));
  EXPECT_FALSE(settings.ShouldDeleteCookieOnExit(
      {}, kDomain, net::CookieSourceScheme::kSecure));
}

TEST_F(CookieSettingsBaseTest, ShouldDeleteDomainSettingSessionOnly) {
  CallbackCookieSettings settings(CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(settings.ShouldDeleteCookieOnExit(
      {CreateSetting(CONTENT_SETTING_SESSION_ONLY)}, kDomain,
      net::CookieSourceScheme::kNonSecure));
}

TEST_F(CookieSettingsBaseTest, ShouldDeleteDomainThirdPartySettingSessionOnly) {
  CallbackCookieSettings settings(CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(settings.ShouldDeleteCookieOnExit(
      {CreateThirdPartySetting(CONTENT_SETTING_SESSION_ONLY)}, kDomain,
      net::CookieSourceScheme::kNonSecure));
}

TEST_F(CookieSettingsBaseTest, ShouldNotDeleteDomainSettingAllow) {
  CallbackCookieSettings settings(CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(settings.ShouldDeleteCookieOnExit(
      {CreateSetting(CONTENT_SETTING_ALLOW)}, kDomain,
      net::CookieSourceScheme::kNonSecure));
}

TEST_F(CookieSettingsBaseTest,
       ShouldNotDeleteDomainSettingAllowAfterSessionOnly) {
  CallbackCookieSettings settings(CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(settings.ShouldDeleteCookieOnExit(
      {CreateSetting(CONTENT_SETTING_SESSION_ONLY),
       CreateSetting(CONTENT_SETTING_ALLOW)},
      kDomain, net::CookieSourceScheme::kNonSecure));
}

TEST_F(CookieSettingsBaseTest, ShouldNotDeleteDomainSettingBlock) {
  CallbackCookieSettings settings(CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(settings.ShouldDeleteCookieOnExit(
      {CreateSetting(CONTENT_SETTING_BLOCK)}, kDomain,
      net::CookieSourceScheme::kNonSecure));
}

TEST_F(CookieSettingsBaseTest, ShouldNotDeleteNoDomainMatch) {
  CallbackCookieSettings settings(CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(settings.ShouldDeleteCookieOnExit(
      {CreateSetting(CONTENT_SETTING_SESSION_ONLY)}, "other.com",
      net::CookieSourceScheme::kNonSecure));
}

TEST_F(CookieSettingsBaseTest, ShouldNotDeleteNoThirdPartyDomainMatch) {
  CallbackCookieSettings settings(CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(settings.ShouldDeleteCookieOnExit(
      {CreateThirdPartySetting(CONTENT_SETTING_SESSION_ONLY)}, "other.com",
      net::CookieSourceScheme::kNonSecure));
}

TEST_F(CookieSettingsBaseTest, CookieAccessNotAllowedWithBlockedSetting) {
  CallbackCookieSettings settings(CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(settings.IsFullCookieAccessAllowed(
      url_, site_for_cookies_, origin_, net::CookieSettingOverrides()));
}

TEST_F(CookieSettingsBaseTest, CookieAccessAllowedWithAllowSetting) {
  CallbackCookieSettings settings(CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(settings.IsFullCookieAccessAllowed(
      url_, site_for_cookies_, origin_, net::CookieSettingOverrides()));
}

TEST_F(CookieSettingsBaseTest, ThirdPartyCookiesOverriden) {
  const GURL kThirdPartyURL = GURL("https://3p.com");

  CallbackCookieSettings settings(CONTENT_SETTING_ALLOW);
  net::CookieSettingOverrides overrides{};
  overrides.Put(net::CookieSettingOverride::kForceDisableThirdPartyCookies);

  EXPECT_TRUE(settings.IsFullCookieAccessAllowed(url_, site_for_cookies_,
                                                 origin_, overrides));
  EXPECT_FALSE(settings.IsFullCookieAccessAllowed(
      kThirdPartyURL, site_for_cookies_, origin_, overrides));
  EXPECT_TRUE(settings.IsFullCookieAccessAllowed(
      kThirdPartyURL, site_for_cookies_, origin_,
      net::CookieSettingOverrides()));
}

TEST_F(CookieSettingsBaseTest, CookieAccessAllowedWithSessionOnlySetting) {
  CallbackCookieSettings settings(CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(settings.IsFullCookieAccessAllowed(
      url_, site_for_cookies_, origin_, net::CookieSettingOverrides()));
}

TEST_F(CookieSettingsBaseTest, LegacyCookieAccessSemantics) {
  CallbackCookieSettings settings1(
      base::BindRepeating([](const GURL&, ContentSettingsType, SettingInfo*) {
        return CONTENT_SETTING_ALLOW;
      }));
  EXPECT_EQ(net::CookieAccessSemantics::LEGACY,
            settings1.GetCookieAccessSemanticsForDomain(std::string()));
  CallbackCookieSettings settings2(
      base::BindRepeating([](const GURL&, ContentSettingsType, SettingInfo*) {
        return CONTENT_SETTING_BLOCK;
      }));
  EXPECT_EQ(net::CookieAccessSemantics::NONLEGACY,
            settings2.GetCookieAccessSemanticsForDomain(std::string()));
}

TEST_F(CookieSettingsBaseTest, IsCookieSessionOnlyWithAllowSetting) {
  CallbackCookieSettings settings(CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(settings.IsCookieSessionOnly(url_));
}

TEST_F(CookieSettingsBaseTest, IsCookieSessionOnlyWithBlockSetting) {
  CallbackCookieSettings settings(CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(settings.IsCookieSessionOnly(url_));
}

TEST_F(CookieSettingsBaseTest, IsCookieSessionOnlySessionWithOnlySetting) {
  CallbackCookieSettings settings(CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(settings.IsCookieSessionOnly(url_));
}

TEST_F(CookieSettingsBaseTest, IsValidSetting) {
  EXPECT_FALSE(CookieSettingsBase::IsValidSetting(CONTENT_SETTING_DEFAULT));
  EXPECT_FALSE(CookieSettingsBase::IsValidSetting(CONTENT_SETTING_ASK));
  EXPECT_TRUE(CookieSettingsBase::IsValidSetting(CONTENT_SETTING_ALLOW));
  EXPECT_TRUE(CookieSettingsBase::IsValidSetting(CONTENT_SETTING_BLOCK));
  EXPECT_TRUE(CookieSettingsBase::IsValidSetting(CONTENT_SETTING_SESSION_ONLY));
}

TEST_F(CookieSettingsBaseTest, IsAllowed) {
  EXPECT_FALSE(CookieSettingsBase::IsAllowed(CONTENT_SETTING_BLOCK));
  EXPECT_TRUE(CookieSettingsBase::IsAllowed(CONTENT_SETTING_ALLOW));
  EXPECT_TRUE(CookieSettingsBase::IsAllowed(CONTENT_SETTING_SESSION_ONLY));
}

TEST_F(CookieSettingsBaseTest, IsValidLegacyAccessSetting) {
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
  CallbackCookieSettings settings(CONTENT_SETTING_ALLOW);

  net::CookieSettingOverrides overrides = settings.SettingOverridesForStorage();

  EXPECT_EQ(
      overrides.Has(net::CookieSettingOverride::kStorageAccessGrantEligible),
      PermissionGrantsUnpartitionedStorage() || IsStoragePartitioned());
  EXPECT_EQ(
      overrides.Has(
          net::CookieSettingOverride::kTopLevelStorageAccessGrantEligible),
      IsStoragePartitioned());
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    CookieSettingsBaseStorageAccessAPITest,
    testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace
}  // namespace content_settings
