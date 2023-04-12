// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/webui_allowlist_provider.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "ui/webui/webui_allowlist.h"

#include <map>
#include <memory>

#include "base/test/gtest_util.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"

class WebUIAllowlistProviderTest : public ChromeRenderViewHostTestHarness {
 public:
  HostContentSettingsMap* GetHostContentSettingsMap(Profile* profile) {
    return HostContentSettingsMapFactory::GetForProfile(profile);
  }
};

TEST_F(WebUIAllowlistProviderTest, RegisterChrome) {
  auto* map = GetHostContentSettingsMap(profile());
  map->SetDefaultContentSetting(ContentSettingsType::BLUETOOTH_GUARD,
                                CONTENT_SETTING_BLOCK);
  map->SetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS,
                                CONTENT_SETTING_BLOCK);
  map->SetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                CONTENT_SETTING_BLOCK);

  // Check |url_allowed| is not affected by allowlisted_schemes. This mechanism
  // take precedence over allowlist provider.
  const GURL url_allowed = GURL("chrome://test/");
  ASSERT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(url_allowed, url_allowed,
                                   ContentSettingsType::BLUETOOTH_GUARD));

  const GURL url_ordinary = GURL("https://example.com");
  ASSERT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(url_ordinary, url_ordinary,
                                   ContentSettingsType::BLUETOOTH_GUARD));
  ASSERT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(url_ordinary, url_ordinary,
                                   ContentSettingsType::NOTIFICATIONS));

  auto* allowlist = WebUIAllowlist::GetOrCreate(profile());
  allowlist->RegisterAutoGrantedPermission(
      url::Origin::Create(url_allowed), ContentSettingsType::BLUETOOTH_GUARD);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(url_allowed, url_allowed,
                                   ContentSettingsType::BLUETOOTH_GUARD));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(url_ordinary, url_ordinary,
                                   ContentSettingsType::BLUETOOTH_GUARD));

  const GURL url_no_permission_webui = GURL("chrome://no-perm");
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      map->GetContentSetting(url_no_permission_webui, url_no_permission_webui,
                             ContentSettingsType::BLUETOOTH_GUARD));
}

TEST_F(WebUIAllowlistProviderTest, RegisterChromeUntrusted) {
  auto* map = GetHostContentSettingsMap(profile());
  map->SetDefaultContentSetting(ContentSettingsType::BLUETOOTH_GUARD,
                                CONTENT_SETTING_BLOCK);
  map->SetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS,
                                CONTENT_SETTING_BLOCK);
  map->SetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                CONTENT_SETTING_BLOCK);

  // Check |url_allowed| is not affected by allowlisted_schemes. This mechanism
  // take precedence over allowlist provider.
  const GURL url_allowed = GURL("chrome-untrusted://test/");
  ASSERT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(url_allowed, url_allowed,
                                   ContentSettingsType::BLUETOOTH_GUARD));

  auto* allowlist = WebUIAllowlist::GetOrCreate(profile());
  allowlist->RegisterAutoGrantedPermission(
      url::Origin::Create(url_allowed), ContentSettingsType::BLUETOOTH_GUARD);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(url_allowed, url_allowed,
                                   ContentSettingsType::BLUETOOTH_GUARD));

  const GURL url_no_permission_webui = GURL("chrome-untrusted://no-perm");
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      map->GetContentSetting(url_no_permission_webui, url_no_permission_webui,
                             ContentSettingsType::BLUETOOTH_GUARD));
}

#if DCHECK_IS_ON()
#define MAYBE_UnsupportedSchemes UnsupportedSchemes
#else
#define MAYBE_UnsupportedSchemes DISABLED_UnsupportedSchemes
#endif
TEST_F(WebUIAllowlistProviderTest, MAYBE_UnsupportedSchemes) {
  auto* allowlist = WebUIAllowlist::GetOrCreate(profile());

  std::string unsupported_urls[] = {
      "http://example.com",
      "https://example.com",
      "file:///file",
  };

  for (const auto& url : unsupported_urls) {
    EXPECT_DEATH_IF_SUPPORTED(allowlist->RegisterAutoGrantedPermission(
                                  url::Origin::Create(GURL(url)),
                                  ContentSettingsType::BLUETOOTH_GUARD),
                              std::string());
  }
}

#if DCHECK_IS_ON()
#define MAYBE_UnsupportedThirdPartyCookiesSettings \
  UnsupportedThirdPartyCookiesSettings
#else
#define MAYBE_UnsupportedThirdPartyCookiesSettings \
  DISABLED_UnsupportedThirdPartyCookiesSettings
#endif
TEST_F(WebUIAllowlistProviderTest, MAYBE_UnsupportedThirdPartyCookiesSettings) {
  auto* allowlist = WebUIAllowlist::GetOrCreate(profile());

  std::string unsupported_top_level_origins[] = {
      "http://example.com",
      "https://example.com",
      "file:///file",
  };

  for (const auto& url : unsupported_top_level_origins) {
    EXPECT_DEATH_IF_SUPPORTED(allowlist->RegisterAutoGrantedThirdPartyCookies(
                                  url::Origin::Create(GURL(url)),
                                  {ContentSettingsPattern::FromURL(GURL(url))}),
                              std::string());
  }
}

#if DCHECK_IS_ON()
#define MAYBE_InvalidContentSetting InvalidContentSetting
#else
#define MAYBE_InvalidContentSetting DISABLED_InvalidContentSetting
#endif
TEST_F(WebUIAllowlistProviderTest, MAYBE_InvalidContentSetting) {
  auto* allowlist = WebUIAllowlist::GetOrCreate(profile());

  EXPECT_DEATH_IF_SUPPORTED(
      allowlist->RegisterAutoGrantedPermission(
          url::Origin::Create(GURL("chrome://test/")),
          ContentSettingsType::BLUETOOTH_GUARD, CONTENT_SETTING_DEFAULT),
      std::string());
}

TEST_F(WebUIAllowlistProviderTest, AutoGrantPermissionIsPerProfile) {
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  // Create two profiles.
  Profile* profile1 = profile_manager.CreateTestingProfile("1");
  auto* map1 = GetHostContentSettingsMap(profile1);
  map1->SetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                 CONTENT_SETTING_BLOCK);
  Profile* profile2 = profile_manager.CreateTestingProfile("2");
  auto* map2 = GetHostContentSettingsMap(profile2);
  map2->SetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                 CONTENT_SETTING_BLOCK);

  GURL url = GURL("chrome://test");

  // Register GEOLOCATION with |profile1|.
  WebUIAllowlist::GetOrCreate(profile1)->RegisterAutoGrantedPermission(
      url::Origin::Create(url), ContentSettingsType::GEOLOCATION);

  // Check permissions are granted to the correct profile.
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      map1->GetContentSetting(url, url, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      map2->GetContentSetting(url, url, ContentSettingsType::GEOLOCATION));
}

TEST_F(WebUIAllowlistProviderTest, RegisterDevtools) {
  auto* map = GetHostContentSettingsMap(profile());
  map->SetDefaultContentSetting(ContentSettingsType::BLUETOOTH_GUARD,
                                CONTENT_SETTING_BLOCK);

  // Check |url_allowed| is not affected by allowlisted_schemes. This mechanism
  // take precedence over allowlist provider.
  const GURL url_allowed = GURL("devtools://devtools");
  ASSERT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(url_allowed, url_allowed,
                                   ContentSettingsType::BLUETOOTH_GUARD));

  auto* allowlist = WebUIAllowlist::GetOrCreate(profile());
  allowlist->RegisterAutoGrantedPermission(
      url::Origin::Create(url_allowed), ContentSettingsType::BLUETOOTH_GUARD);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(url_allowed, url_allowed,
                                   ContentSettingsType::BLUETOOTH_GUARD));

  const GURL url_no_permission_webui = GURL("devtools://other");
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      map->GetContentSetting(url_no_permission_webui, url_no_permission_webui,
                             ContentSettingsType::BLUETOOTH_GUARD));
}

TEST_F(WebUIAllowlistProviderTest, RegisterWithPermissionList) {
  auto* map = GetHostContentSettingsMap(profile());
  map->SetDefaultContentSetting(ContentSettingsType::BLUETOOTH_GUARD,
                                CONTENT_SETTING_BLOCK);
  map->SetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS,
                                CONTENT_SETTING_BLOCK);

  const GURL url_chrome = GURL("chrome://test");

  auto* allowlist = WebUIAllowlist::GetOrCreate(profile());
  allowlist->RegisterAutoGrantedPermissions(
      url::Origin::Create(url_chrome), {ContentSettingsType::BLUETOOTH_GUARD,
                                        ContentSettingsType::NOTIFICATIONS});

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(url_chrome, url_chrome,
                                   ContentSettingsType::BLUETOOTH_GUARD));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(url_chrome, url_chrome,
                                   ContentSettingsType::NOTIFICATIONS));
}

TEST_F(WebUIAllowlistProviderTest,
       RegisterThirdPartyCookiesWithAllCookiesBlocked) {
  auto* map = GetHostContentSettingsMap(profile());
  map->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                CONTENT_SETTING_BLOCK);
  map->SetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS,
                                CONTENT_SETTING_BLOCK);

  const GURL top_level_url = GURL("chrome-untrusted://test/");
  const GURL third_party_url = GURL("https://example.com/");

  // Check |url_allowed| is not affected by allowlisted_schemes, which takes
  // precedence over allowlist provider.
  ASSERT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(third_party_url, top_level_url,
                                   ContentSettingsType::COOKIES));

  auto* allowlist = WebUIAllowlist::GetOrCreate(profile());
  allowlist->RegisterAutoGrantedThirdPartyCookies(
      url::Origin::Create(top_level_url),
      {ContentSettingsPattern::FromURL(third_party_url)});

  auto cookies_settings = CookieSettingsFactory::GetForProfile(profile());

  // Allowlisted origin embedded in the correct top-level origin can use
  // cookies.
  EXPECT_TRUE(cookies_settings->IsFullCookieAccessAllowed(
      third_party_url, net::SiteForCookies::FromUrl(top_level_url),
      url::Origin::Create(top_level_url), net::CookieSettingOverrides()));

  // Allowlisted origin on its own can't use cookies.
  EXPECT_FALSE(cookies_settings->IsFullCookieAccessAllowed(
      third_party_url, net::SiteForCookies::FromUrl(third_party_url),
      url::Origin::Create(third_party_url), net::CookieSettingOverrides()));

  // Allowlisted origin embedded in Web top-level origin can't use cookies.
  EXPECT_FALSE(cookies_settings->IsFullCookieAccessAllowed(
      GURL("https://example2.com"),
      net::SiteForCookies::FromUrl(third_party_url),
      url::Origin::Create(third_party_url), net::CookieSettingOverrides()));

  // Allowlisted origin making subresource request (e.g. image) can't use
  // cookies.
  EXPECT_FALSE(cookies_settings->IsFullCookieAccessAllowed(
      third_party_url, net::SiteForCookies(), absl::nullopt,
      net::CookieSettingOverrides()));

  // Allowlisted origin embedded in the wrong WebUI origin can't use cookies.
  const GURL url_no_permission_webui = GURL("chrome-untrusted://no-perm");
  EXPECT_FALSE(cookies_settings->IsFullCookieAccessAllowed(
      third_party_url, net::SiteForCookies::FromUrl(url_no_permission_webui),
      url::Origin::Create(url_no_permission_webui),
      net::CookieSettingOverrides()));

  // Other permissions aren't affected.
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(third_party_url, top_level_url,
                                   ContentSettingsType::NOTIFICATIONS));
}

TEST_F(WebUIAllowlistProviderTest,
       RegisterThirdPartyCookiesWithThirdPartyCookiesBlocked) {
  auto* map = GetHostContentSettingsMap(profile());
  map->SetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS,
                                CONTENT_SETTING_BLOCK);
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));
  auto cookies_settings = CookieSettingsFactory::GetForProfile(profile());

  const GURL top_level_url = GURL("chrome-untrusted://test/");
  const GURL third_party_url = GURL("https://example.com/");

  auto* allowlist = WebUIAllowlist::GetOrCreate(profile());
  allowlist->RegisterAutoGrantedThirdPartyCookies(
      url::Origin::Create(top_level_url),
      {ContentSettingsPattern::FromURL(third_party_url)});

  EXPECT_TRUE(cookies_settings->IsFullCookieAccessAllowed(
      third_party_url, net::SiteForCookies::FromUrl(top_level_url),
      url::Origin::Create(top_level_url), net::CookieSettingOverrides()));
  // Allowlisted origin on its own can use cookies, because only third-party
  // cookies are blocked.
  EXPECT_TRUE(cookies_settings->IsFullCookieAccessAllowed(
      third_party_url, net::SiteForCookies::FromUrl(third_party_url),
      url::Origin::Create(third_party_url), net::CookieSettingOverrides()));

  // Allowlisted origin embedded in Web top-level origin can't use cookies.
  EXPECT_FALSE(cookies_settings->IsFullCookieAccessAllowed(
      GURL("https://example2.com"),
      net::SiteForCookies::FromUrl(third_party_url),
      url::Origin::Create(third_party_url), net::CookieSettingOverrides()));

  // Allowlisted origin embedded in the wrong WebUI origin can't use cookies.
  const GURL url_no_permission_webui = GURL("chrome-untrusted://no-perm");
  EXPECT_FALSE(cookies_settings->IsFullCookieAccessAllowed(
      third_party_url, net::SiteForCookies::FromUrl(url_no_permission_webui),
      url::Origin::Create(url_no_permission_webui),
      net::CookieSettingOverrides()));

  // Allowlisted origin making subresource request (e.g. image) can't use
  // cookies.
  EXPECT_FALSE(cookies_settings->IsFullCookieAccessAllowed(
      third_party_url, net::SiteForCookies(), absl::nullopt,
      net::CookieSettingOverrides()));

  // Other permissions aren't affected.
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(third_party_url, top_level_url,
                                   ContentSettingsType::NOTIFICATIONS));
}
