// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_registry.h"

#include <string>

#include "base/values.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_uma_util.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Cannot use an anonymous namespace because WebsiteSettingsRegistry's
// constructor and destructor are private.
namespace content_settings {

using ::testing::Contains;
using ::testing::ElementsAre;

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
#if !BUILDFLAG(USE_BLINK)
  // Javascript shouldn't be registered on iOS.
  EXPECT_FALSE(registry()->Get(ContentSettingsType::JAVASCRIPT));
#endif

#if (BUILDFLAG(IS_IOS) && !BUILDFLAG(USE_BLINK)) || BUILDFLAG(IS_ANDROID)
  // Images shouldn't be registered on mobile.
  EXPECT_FALSE(registry()->Get(ContentSettingsType::IMAGES));
#endif

// Protected media identifier only registered on Android, Chrome OS and Windows.
#if defined(ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
  EXPECT_TRUE(registry()->Get(ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER));
#else
  EXPECT_FALSE(
      registry()->Get(ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER));
#endif

  // Cookies is registered on all platforms.
  EXPECT_TRUE(registry()->Get(ContentSettingsType::COOKIES));
}

TEST_F(ContentSettingsRegistryTest, Properties) {
  // The cookies type should be registered.
  const ContentSettingsInfo* info =
      registry()->Get(ContentSettingsType::COOKIES);
  ASSERT_TRUE(info);

  EXPECT_THAT(info->allowlisted_primary_schemes(),
              ElementsAre("chrome", "devtools"));
  EXPECT_THAT(info->third_party_cookie_allowed_secondary_schemes(),
              ElementsAre("devtools", "chrome-extension"));

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
  ASSERT_TRUE(website_settings_info->initial_default_value().is_int());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            website_settings_info->initial_default_value().GetInt());
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_EQ(PrefRegistry::NO_REGISTRATION_FLAGS,
            website_settings_info->GetPrefRegistrationFlags());
#else
  EXPECT_EQ(user_prefs::PrefRegistrySyncable::SYNCABLE_PREF,
            website_settings_info->GetPrefRegistrationFlags());
#endif

  // Check the WebsiteSettingsInfo is registered correctly.
  EXPECT_EQ(website_settings_registry()->Get(ContentSettingsType::COOKIES),
            website_settings_info);

  // Check that PRIVATE_NETWORK_GUARD is registered correctly.
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  info = registry()->Get(ContentSettingsType::PRIVATE_NETWORK_GUARD);
  ASSERT_TRUE(info);

  // Check the other properties are populated correctly.
  EXPECT_TRUE(info->IsSettingValid(CONTENT_SETTING_BLOCK));
  EXPECT_TRUE(info->IsSettingValid(CONTENT_SETTING_ASK));
  EXPECT_FALSE(info->IsSettingValid(CONTENT_SETTING_SESSION_ONLY));
  EXPECT_FALSE(info->IsSettingValid(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
            info->incognito_behavior());
#endif
}

TEST_F(ContentSettingsRegistryTest, Iteration) {
  // Check that cookies settings appear once during iteration.
  bool cookies_found = false;
  for (const ContentSettingsInfo* info : *registry()) {
    ContentSettingsType type = info->website_settings_info()->type();
    EXPECT_EQ(registry()->Get(type), info);
    if (type == ContentSettingsType::COOKIES) {
      EXPECT_FALSE(cookies_found);
      cookies_found = true;
    }
  }

  EXPECT_TRUE(cookies_found);
}

// Settings that control access to user data should not be inherited.
// Check that only safe settings are inherited in incognito.
TEST_F(ContentSettingsRegistryTest, Inheritance) {
  // These settings are safe to inherit in incognito mode because they only
  // disable features like popup blocking, download blocking or ad blocking.
  // They do not allow access to user data.
  const ContentSettingsType safe_types[] = {
      ContentSettingsType::POPUPS,
      ContentSettingsType::AUTOMATIC_DOWNLOADS,
      ContentSettingsType::ADS,
      ContentSettingsType::DURABLE_STORAGE,
      ContentSettingsType::LEGACY_COOKIE_ACCESS,
      ContentSettingsType::INSECURE_PRIVATE_NETWORK,
      ContentSettingsType::REQUEST_DESKTOP_SITE,
      ContentSettingsType::KEYBOARD_LOCK,
      ContentSettingsType::POINTER_LOCK,
  };

  for (const ContentSettingsInfo* info : *registry()) {
    SCOPED_TRACE("Content setting: " + info->website_settings_info()->name());
    // TODO(crbug.com/41353652): Check IsSettingValid() because
    // "protocol-handler" and "mixed-script" don't have a proper initial default
    // value.

    // ALLOW-by-default settings are not affected by incognito_behavior, so
    // they should be marked as INHERIT_IN_INCOGNITO.
    if (info->IsSettingValid(CONTENT_SETTING_ALLOW) &&
        info->GetInitialDefaultSetting() == CONTENT_SETTING_ALLOW) {
      // Top-level 3pcd origin trial content settings are a special case that
      // should not be inherited in incognito, despite being ALLOW-by-default.
      if (info->website_settings_info()->type() ==
          ContentSettingsType::TOP_LEVEL_TPCD_ORIGIN_TRIAL) {
        EXPECT_EQ(info->incognito_behavior(),
                  ContentSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
        continue;
      }

      EXPECT_EQ(info->incognito_behavior(),
                ContentSettingsInfo::INHERIT_IN_INCOGNITO);
      continue;
    }
    // Tracking protection content setting should be inherited in incognito.
    if (info->website_settings_info()->type() ==
            ContentSettingsType::TRACKING_PROTECTION &&
        info->GetInitialDefaultSetting() == CONTENT_SETTING_BLOCK) {
      EXPECT_EQ(info->incognito_behavior(),
                ContentSettingsInfo::INHERIT_IN_INCOGNITO);
      continue;
    }

    if (info->incognito_behavior() ==
        ContentSettingsInfo::INHERIT_IN_INCOGNITO) {
      EXPECT_THAT(safe_types, Contains(info->website_settings_info()->type()));
    }
  }
}

TEST_F(ContentSettingsRegistryTest, IsDefaultSettingValid) {
  const ContentSettingsInfo* info =
      registry()->Get(ContentSettingsType::COOKIES);
  EXPECT_TRUE(info->IsDefaultSettingValid(CONTENT_SETTING_ALLOW));

  info = registry()->Get(ContentSettingsType::MEDIASTREAM_MIC);
  EXPECT_FALSE(info->IsDefaultSettingValid(CONTENT_SETTING_ALLOW));

  info = registry()->Get(ContentSettingsType::MEDIASTREAM_CAMERA);
  EXPECT_FALSE(info->IsDefaultSettingValid(CONTENT_SETTING_ALLOW));

#if BUILDFLAG(IS_CHROMEOS)
  info = registry()->Get(ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER);
  EXPECT_TRUE(info->IsDefaultSettingValid(CONTENT_SETTING_ALLOW));
#endif

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  info = registry()->Get(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);
  EXPECT_FALSE(info->IsDefaultSettingValid(CONTENT_SETTING_ALLOW));

  info = registry()->Get(ContentSettingsType::PRIVATE_NETWORK_GUARD);
  EXPECT_FALSE(info->IsDefaultSettingValid(CONTENT_SETTING_ALLOW));
#endif
}

// Check the correct factory default setting is retrieved. Note the factory
// default settings are hard coded, so changing them in ContentSettingsRegistry
// would require this test to be updated.
TEST_F(ContentSettingsRegistryTest, GetInitialDefaultSetting) {
// There is no default-ask content setting on iOS, so skip testing it there.
#if !BUILDFLAG(IS_IOS)
  const ContentSettingsInfo* notifications =
      registry()->Get(ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(CONTENT_SETTING_ASK, notifications->GetInitialDefaultSetting());
#endif

  const ContentSettingsInfo* cookies =
      registry()->Get(ContentSettingsType::COOKIES);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, cookies->GetInitialDefaultSetting());

  const ContentSettingsInfo* popups =
      registry()->Get(ContentSettingsType::POPUPS);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, popups->GetInitialDefaultSetting());

  const ContentSettingsInfo* insecure_private_network =
      registry()->Get(ContentSettingsType::INSECURE_PRIVATE_NETWORK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            insecure_private_network->GetInitialDefaultSetting());

  const ContentSettingsInfo* federated_identity =
      registry()->Get(ContentSettingsType::FEDERATED_IDENTITY_API);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            federated_identity->GetInitialDefaultSetting());

  const ContentSettingsInfo* federated_identity_auto_reauthn = registry()->Get(
      ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            federated_identity_auto_reauthn->GetInitialDefaultSetting());
}

TEST_F(ContentSettingsRegistryTest, SettingsHaveAHistogramMapping) {
  size_t count = 0;
  std::set<int> values;
  for (const WebsiteSettingsInfo* info : *website_settings_registry()) {
    int value = content_settings_uma_util::ContentSettingTypeToHistogramValue(
        info->type());
    EXPECT_GT(value, 0);
    count++;
    values.insert(value);
  }
  // Validate that values are unique.
  EXPECT_EQ(count, values.size());
}

}  // namespace content_settings
