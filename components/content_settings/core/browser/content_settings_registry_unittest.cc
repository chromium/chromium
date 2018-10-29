// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_settings {

class ContentSettingsRegistryTest : public testing::Test {
 protected:
  ContentSettingsRegistryTest() : registry_(&website_settings_registry_) {}
  ContentSettingsRegistry* registry() { return &registry_; }
  WebsiteSettingsRegistry* website_settings_registry() {
    return &website_settings_registry_;
  }

 private:
  WebsiteSettingsRegistry website_settings_registry_;
  ContentSettingsRegistry registry_;
};

TEST_F(ContentSettingsRegistryTest, GetPlatformDependent) {
#if defined(OS_IOS)
  // Javascript shouldn't be registered on iOS.
  EXPECT_FALSE(registry()->Get(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
#endif

#if defined(OS_IOS) || defined(OS_ANDROID)
  // Images shouldn't be registered on mobile.
  EXPECT_FALSE(registry()->Get(CONTENT_SETTINGS_TYPE_IMAGES));
#endif

// Protected media identifier only get registered on android and chromeos.
#if defined(ANDROID) || defined(OS_CHROMEOS)
  EXPECT_TRUE(
      registry()->Get(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER));
#else
  EXPECT_FALSE(
      registry()->Get(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER));
#endif

  // Cookies is registered on all platforms.
  EXPECT_TRUE(registry()->Get(CONTENT_SETTINGS_TYPE_COOKIES));
}

TEST_F(ContentSettingsRegistryTest, Properties) {
  // The cookies type should be registered.
  const ContentSettingsInfo* info =
      registry()->Get(CONTENT_SETTINGS_TYPE_COOKIES);
  ASSERT_TRUE(info);

  // Check that the whitelisted types are correct.
  std::vector<std::string> expected_whitelist;
  expected_whitelist.push_back("chrome");
  expected_whitelist.push_back("chrome-devtools");
  EXPECT_EQ(expected_whitelist, info->whitelisted_schemes());

  // Check the other properties are populated correctly.
  EXPECT_TRUE(info->IsSettingValid(CONTENT_SETTING_SESSION_ONLY));
  EXPECT_FALSE(info->IsSettingValid(CONTENT_SETTING_ASK));
  EXPECT_EQ(ContentSettingsInfo::INHERIT_IN_INCOGNITO,
            info->incognito_behavior());

  // Check the WebsiteSettingsInfo is populated correctly.
  const WebsiteSettingsInfo* website_settings_info =
      info->website_settings_info();
  EXPECT_EQ("cookies", website_settings_info->name());
  EXPECT_EQ("profile.content_settings.exceptions.cookies",
            website_settings_info->pref_name());
  EXPECT_EQ("profile.default_content_setting_values.cookies",
            website_settings_info->default_value_pref_name());
  int setting;
  ASSERT_TRUE(
      website_settings_info->initial_default_value()->GetAsInteger(&setting));
  EXPECT_EQ(CONTENT_SETTING_ALLOW, setting);
#if defined(OS_ANDROID) || defined(OS_IOS)
  EXPECT_EQ(PrefRegistry::NO_REGISTRATION_FLAGS,
            website_settings_info->GetPrefRegistrationFlags());
#else
  EXPECT_EQ(user_prefs::PrefRegistrySyncable::SYNCABLE_PREF,
            website_settings_info->GetPrefRegistrationFlags());
#endif

  // Check the WebsiteSettingsInfo is registered correctly.
  EXPECT_EQ(website_settings_registry()->Get(CONTENT_SETTINGS_TYPE_COOKIES),
            website_settings_info);
}

TEST_F(ContentSettingsRegistryTest, Iteration) {
  // Check that plugins and cookies settings appear once during iteration.
  bool plugins_found = false;
  bool cookies_found = false;
  for (const ContentSettingsInfo* info : *registry()) {
    ContentSettingsType type = info->website_settings_info()->type();
    EXPECT_EQ(registry()->Get(type), info);
    if (type == CONTENT_SETTINGS_TYPE_PLUGINS) {
      EXPECT_FALSE(plugins_found);
      plugins_found = true;
    } else if (type == CONTENT_SETTINGS_TYPE_COOKIES) {
      EXPECT_FALSE(cookies_found);
      cookies_found = true;
    }
  }

#if defined(OS_ANDROID) || defined(OS_IOS)
  EXPECT_FALSE(plugins_found);
#else
  EXPECT_TRUE(plugins_found);
#endif

  EXPECT_TRUE(cookies_found);
}

// Settings that control access to user data should not be inherited.
// Check that only safe settings are inherited in incognito.
TEST_F(ContentSettingsRegistryTest, Inheritance) {
  // These settings are safe to inherit in incognito mode because they only
  // disable features like popup blocking, download blocking or ad blocking.
  // They do not allow access to user data.
  const ContentSettingsType whitelist[] = {
      CONTENT_SETTINGS_TYPE_POPUPS,               //
      CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS,  //
      CONTENT_SETTINGS_TYPE_ADS,                  //
      CONTENT_SETTINGS_TYPE_DURABLE_STORAGE,
  };

  for (const ContentSettingsInfo* info : *registry()) {
    SCOPED_TRACE("Content setting: " + info->website_settings_info()->name());
    // TODO(crbug.com/781756): Check IsSettingValid() because "protocol-handler"
    // and "mixed-script" don't have a proper initial default value.
    if (info->IsSettingValid(CONTENT_SETTING_ALLOW) &&
        info->GetInitialDefaultSetting() == CONTENT_SETTING_ALLOW) {
      // ALLOW-by-default settings are not affected by incognito_behavior, so
      // they should be marked as INHERIT_IN_INCOGNITO.
      EXPECT_EQ(info->incognito_behavior(),
                ContentSettingsInfo::INHERIT_IN_INCOGNITO);
      continue;
    }
    if (info->incognito_behavior() ==
            ContentSettingsInfo::INHERIT_IN_INCOGNITO &&
        !base::ContainsValue(whitelist, info->website_settings_info()->type()))
      FAIL() << "Content setting not whitelisted.";
  }
}

TEST_F(ContentSettingsRegistryTest, IsDefaultSettingValid) {
  const ContentSettingsInfo* info =
      registry()->Get(CONTENT_SETTINGS_TYPE_COOKIES);
  EXPECT_TRUE(info->IsDefaultSettingValid(CONTENT_SETTING_ALLOW));

#if !defined(OS_IOS)
  info = registry()->Get(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
  EXPECT_FALSE(info->IsDefaultSettingValid(CONTENT_SETTING_ALLOW));

  info = registry()->Get(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
  EXPECT_FALSE(info->IsDefaultSettingValid(CONTENT_SETTING_ALLOW));
#endif

#if defined(OS_CHROMEOS)
  info = registry()->Get(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER);
  EXPECT_FALSE(info->IsDefaultSettingValid(CONTENT_SETTING_ALLOW));
#endif
}

// Check the correct factory default setting is retrieved. Note the factory
// default settings are hard coded, so changing them in ContentSettingsRegistry
// would require this test to be updated.
TEST_F(ContentSettingsRegistryTest, GetInitialDefaultSetting) {
// There is no default-ask content setting on iOS, so skip testing it there.
#if !defined(OS_IOS)
  const ContentSettingsInfo* notifications =
      registry()->Get(CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  EXPECT_EQ(CONTENT_SETTING_ASK, notifications->GetInitialDefaultSetting());
#endif

  const ContentSettingsInfo* cookies =
      registry()->Get(CONTENT_SETTINGS_TYPE_COOKIES);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, cookies->GetInitialDefaultSetting());

  const ContentSettingsInfo* popups =
      registry()->Get(CONTENT_SETTINGS_TYPE_POPUPS);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, popups->GetInitialDefaultSetting());
}

}  // namespace content_settings
