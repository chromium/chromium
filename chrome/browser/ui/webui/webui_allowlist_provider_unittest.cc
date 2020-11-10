// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/webui_allowlist_provider.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "ui/webui/webui_allowlist.h"

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/test/gtest_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"

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

class ContentSettingsChangeObserver : public content_settings::Observer {
 public:
  ContentSettingsChangeObserver() = default;
  ContentSettingsChangeObserver(const ContentSettingsChangeObserver&) = delete;
  void operator=(const ContentSettingsChangeObserver&) = delete;
  ~ContentSettingsChangeObserver() override = default;

  size_t change_counter() { return change_counter_; }

  // content_settings::Observer:
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type) override {
    change_counter_++;
  }

 private:
  size_t change_counter_ = 0;
};

TEST_F(WebUIAllowlistProviderTest, OnlyNotifyOnChange) {
  auto* map = GetHostContentSettingsMap(profile());
  map->SetDefaultContentSetting(ContentSettingsType::BLUETOOTH_GUARD,
                                CONTENT_SETTING_BLOCK);

  ContentSettingsChangeObserver change_observer;
  map->AddObserver(&change_observer);

  const url::Origin origin1 = url::Origin::Create(GURL("chrome://test"));

  auto* allowlist = WebUIAllowlist::GetOrCreate(profile());
  allowlist->RegisterAutoGrantedPermission(
      origin1, ContentSettingsType::BLUETOOTH_GUARD);
  EXPECT_EQ(1U, change_observer.change_counter());

  // Registering the same permission should not trigger OnContentSettingChanged.
  allowlist->RegisterAutoGrantedPermission(
      origin1, ContentSettingsType::BLUETOOTH_GUARD);
  EXPECT_EQ(1U, change_observer.change_counter());

  // Registering a different permission should trigger OnContentSettingChanged.
  allowlist->RegisterAutoGrantedPermission(origin1,
                                           ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(2U, change_observer.change_counter());

  // Registering a different origin should trigger OnContentSettingChanged.
  const url::Origin origin2 = url::Origin::Create(GURL("chrome://test2"));
  allowlist->RegisterAutoGrantedPermission(origin2,
                                           ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(3U, change_observer.change_counter());
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
