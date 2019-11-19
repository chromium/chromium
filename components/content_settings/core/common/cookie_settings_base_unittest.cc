// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/cookie_settings_base.h"

#include "base/bind.h"
#include "base/callback.h"
#include "net/cookies/cookie_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content_settings {
namespace {

constexpr char kDomain[] = "foo.com";

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

  // CookieSettingsBase:
  void GetCookieSettingInternal(const GURL& url,
                                const GURL& first_party_url,
                                bool is_third_party_request,
                                content_settings::SettingSource* source,
                                ContentSetting* cookie_setting) const override {
    *cookie_setting = callback_.Run(url);
  }
  void GetSettingForLegacyCookieAccess(const std::string& cookie_domain,
                                       ContentSetting* setting) const override {
    GURL cookie_domain_url =
        net::cookie_util::CookieOriginToURL(cookie_domain, false);
    *setting = callback_.Run(cookie_domain_url);
  }
  bool ShouldIgnoreSameSiteRestrictions(
      const GURL& url,
      const GURL& site_for_cookies) const override {
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
  EXPECT_FALSE(settings.IsCookieAccessAllowed(GURL(kDomain), GURL(kDomain)));
}

TEST(CookieSettingsBaseTest, CookieAccessAllowedWithAllowSetting) {
  CallbackCookieSettings settings(
      base::BindRepeating([](const GURL&) { return CONTENT_SETTING_ALLOW; }));
  EXPECT_TRUE(settings.IsCookieAccessAllowed(GURL(kDomain), GURL(kDomain)));
}

TEST(CookieSettingsBaseTest, CookieAccessAllowedWithSessionOnlySetting) {
  CallbackCookieSettings settings(base::BindRepeating(
      [](const GURL&) { return CONTENT_SETTING_SESSION_ONLY; }));
  EXPECT_TRUE(settings.IsCookieAccessAllowed(GURL(kDomain), GURL(kDomain)));
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
  EXPECT_FALSE(settings.IsCookieSessionOnly(GURL(kDomain)));
}

TEST(CookieSettingsBaseTest, IsCookieSessionOnlyWithBlockSetting) {
  CallbackCookieSettings settings(
      base::BindRepeating([](const GURL&) { return CONTENT_SETTING_BLOCK; }));
  EXPECT_FALSE(settings.IsCookieSessionOnly(GURL(kDomain)));
}

TEST(CookieSettingsBaseTest, IsCookieSessionOnlySessionWithOnlySetting) {
  CallbackCookieSettings settings(base::BindRepeating(
      [](const GURL&) { return CONTENT_SETTING_SESSION_ONLY; }));
  EXPECT_TRUE(settings.IsCookieSessionOnly(GURL(kDomain)));
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

}  // namespace
}  // namespace content_settings
