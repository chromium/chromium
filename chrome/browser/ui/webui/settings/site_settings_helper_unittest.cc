// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/site_settings_helper.h"

#include "base/functional/callback_helpers.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/permissions/object_permission_context_base.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/test/permission_test_util.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/test/test_extension_dir.h"
#endif

namespace site_settings {

namespace {
constexpr ContentSettingsType kContentType = ContentSettingsType::GEOLOCATION;
constexpr ContentSettingsType kContentTypeCookies =
    ContentSettingsType::COOKIES;
constexpr ContentSettingsType kContentTypeFileSystem =
    ContentSettingsType::FILE_SYSTEM_WRITE_GUARD;
constexpr ContentSettingsType kContentTypeNotifications =
    ContentSettingsType::NOTIFICATIONS;
}

class SiteSettingsHelperTest : public testing::Test {
 public:
  void VerifySetting(const base::Value::List& exceptions,
                     int index,
                     const std::string& pattern,
                     const std::string& pattern_display_name,
                     const ContentSetting setting) {
    const base::Value& value = exceptions[index];
    EXPECT_TRUE(value.is_dict());
    const base::Value::Dict& dict = value.GetDict();
    const std::string* actual_pattern = dict.FindString("origin");
    ASSERT_TRUE(actual_pattern);
    EXPECT_EQ(pattern, *actual_pattern);
    const std::string* actual_display_name = dict.FindString(kDisplayName);
    ASSERT_TRUE(actual_display_name);
    EXPECT_EQ(pattern_display_name, *actual_display_name);
    const std::string* actual_setting = dict.FindString(kSetting);
    ASSERT_TRUE(actual_setting);
    EXPECT_EQ(content_settings::ContentSettingToString(setting),
              *actual_setting);
  }

  void AddSetting(HostContentSettingsMap* map,
                  const std::string& pattern,
                  ContentSetting setting) {
    map->SetContentSettingCustomScope(
        ContentSettingsPattern::FromString(pattern),
        ContentSettingsPattern::Wildcard(), kContentType, setting);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(SiteSettingsHelperTest, ExceptionListWithEmbargoedAndBlockedOrigins) {
  TestingProfile profile;

  constexpr char kOriginToEmbargo[] = "https://embargoed.co.uk:443";
  auto* auto_blocker =
      PermissionDecisionAutoBlockerFactory::GetForProfile(&profile);
  for (size_t i = 0; i < 3; ++i) {
    auto_blocker->RecordDismissAndEmbargo(GURL(kOriginToEmbargo),
                                          kContentTypeNotifications, false);
  }

  constexpr char kOriginToBlock[] = "https://www.blocked.com:443";
  auto* map = HostContentSettingsMapFactory::GetForProfile(&profile);
  map->SetContentSettingDefaultScope(GURL(kOriginToBlock), GURL(kOriginToBlock),
                                     kContentTypeNotifications,
                                     CONTENT_SETTING_BLOCK);

  base::Value::List exceptions;
  site_settings::GetExceptionsForContentType(kContentTypeNotifications,
                                             &profile,
                                             /*extension_registry=*/nullptr,
                                             /*web_ui=*/nullptr,
                                             /*incognito=*/false, &exceptions);

  // |exceptions| size should be 2. One blocked and one embargoed origins.
  ASSERT_EQ(2U, exceptions.size());

  // Get last added origin.
  absl::optional<bool> is_embargoed =
      exceptions[0].GetDict().FindBool(site_settings::kIsEmbargoed);
  ASSERT_TRUE(is_embargoed.has_value());
  // Last added origin is blocked, |embargo| key should be false.
  EXPECT_FALSE(*is_embargoed);

  // Get embargoed origin.
  is_embargoed = exceptions[1].GetDict().FindBool(site_settings::kIsEmbargoed);
  ASSERT_TRUE(is_embargoed.has_value());
  EXPECT_TRUE(*is_embargoed);
}

TEST_F(SiteSettingsHelperTest, ExceptionListShowsIncognitoEmbargoed) {
  TestingProfile profile;
  constexpr char kOriginToBlock[] = "https://www.blocked.com:443";
  constexpr char kOriginToEmbargo[] = "https://embargoed.co.uk:443";
  constexpr char kOriginToEmbargoIncognito[] =
      "https://embargoed.incognito.co.uk:443";

  // Add an origin under embargo for non incognito profile.
  {
    auto* auto_blocker =
        PermissionDecisionAutoBlockerFactory::GetForProfile(&profile);
    for (size_t i = 0; i < 3; ++i) {
      auto_blocker->RecordDismissAndEmbargo(GURL(kOriginToEmbargo),
                                            kContentTypeNotifications, false);
    }

    // Check that origin is under embargo.
    ASSERT_EQ(CONTENT_SETTING_BLOCK,
              auto_blocker
                  ->GetEmbargoResult(GURL(kOriginToEmbargo),
                                     kContentTypeNotifications)
                  ->content_setting);
  }

  // Check there is 1 embargoed origin for a non-incognito profile.
  {
    base::Value::List exceptions;
    site_settings::GetExceptionsForContentType(
        kContentTypeNotifications, &profile, /*extension_registry=*/nullptr,
        /*web_ui=*/nullptr,
        /*incognito=*/false, &exceptions);
    ASSERT_EQ(1U, exceptions.size());
  }

  TestingProfile* incognito_profile =
      TestingProfile::Builder().BuildIncognito(&profile);

  // Check there are no blocked origins for an incognito profile.
  {
    base::Value::List exceptions;
    site_settings::GetExceptionsForContentType(kContentTypeNotifications,
                                               incognito_profile,
                                               /*extension_registry=*/nullptr,
                                               /*web_ui=*/nullptr,
                                               /*incognito=*/true, &exceptions);
    ASSERT_TRUE(exceptions.empty());
  }

  {
    auto* incognito_map =
        HostContentSettingsMapFactory::GetForProfile(incognito_profile);
    incognito_map->SetContentSettingDefaultScope(
        GURL(kOriginToBlock), GURL(kOriginToBlock), kContentTypeNotifications,
        CONTENT_SETTING_BLOCK);
  }

  // Check there is only 1 blocked origin for an incognito profile.
  {
    base::Value::List exceptions;
    site_settings::GetExceptionsForContentType(kContentTypeNotifications,
                                               incognito_profile,
                                               /*extension_registry=*/nullptr,
                                               /*web_ui=*/nullptr,
                                               /*incognito=*/true, &exceptions);
    // The exceptions size should be 1 because previously embargoed origin
    // was for a non-incognito profile.
    ASSERT_EQ(1U, exceptions.size());
  }

  // Add an origin under embargo for incognito profile.
  {
    auto* incognito_auto_blocker =
        PermissionDecisionAutoBlockerFactory::GetForProfile(incognito_profile);
    for (size_t i = 0; i < 3; ++i) {
      incognito_auto_blocker->RecordDismissAndEmbargo(
          GURL(kOriginToEmbargoIncognito), kContentTypeNotifications, false);
    }
    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              incognito_auto_blocker
                  ->GetEmbargoResult(GURL(kOriginToEmbargoIncognito),
                                     kContentTypeNotifications)
                  ->content_setting);
  }

  // Check there are 2 blocked or embargoed origins for an incognito profile.
  {
    base::Value::List exceptions;
    site_settings::GetExceptionsForContentType(kContentTypeNotifications,
                                               incognito_profile,
                                               /*extension_registry=*/nullptr,
                                               /*web_ui=*/nullptr,
                                               /*incognito=*/true, &exceptions);
    ASSERT_EQ(2U, exceptions.size());
  }
}

TEST_F(SiteSettingsHelperTest, ExceptionListShowsEmbargoed) {
  TestingProfile profile;
  constexpr char kOriginToBlock[] = "https://www.blocked.com:443";
  constexpr char kOriginToEmbargo[] = "https://embargoed.co.uk:443";

  // Check there is no blocked origins.
  {
    base::Value::List exceptions;
    site_settings::GetExceptionsForContentType(
        kContentTypeNotifications, &profile, /*extension_registry=*/nullptr,
        /*web_ui=*/nullptr,
        /*incognito=*/false, &exceptions);
    ASSERT_TRUE(exceptions.empty());
  }

  auto* map = HostContentSettingsMapFactory::GetForProfile(&profile);
  map->SetContentSettingDefaultScope(GURL(kOriginToBlock), GURL(kOriginToBlock),
                                     kContentTypeNotifications,
                                     CONTENT_SETTING_BLOCK);
  {
    // Check there is 1 blocked origin.
    base::Value::List exceptions;
    site_settings::GetExceptionsForContentType(
        kContentTypeNotifications, &profile, /*extension_registry=*/nullptr,
        /*web_ui=*/nullptr,
        /*incognito=*/false, &exceptions);
    ASSERT_EQ(1U, exceptions.size());
  }

  // Add an origin under embargo.
  auto* auto_blocker =
      PermissionDecisionAutoBlockerFactory::GetForProfile(&profile);
  const GURL origin_to_embargo(kOriginToEmbargo);
  for (size_t i = 0; i < 3; ++i) {
    auto_blocker->RecordDismissAndEmbargo(origin_to_embargo,
                                          kContentTypeNotifications, false);
  }

  // Check that origin is under embargo.
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            auto_blocker
                ->GetEmbargoResult(origin_to_embargo, kContentTypeNotifications)
                ->content_setting);

  // Check there are 2 blocked origins.
  {
    base::Value::List exceptions;
    site_settings::GetExceptionsForContentType(
        kContentTypeNotifications, &profile, /*extension_registry=*/nullptr,
        /*web_ui=*/nullptr,
        /*incognito=*/false, &exceptions);
    // The size should be 2, 1st is blocked origin, 2nd is embargoed origin.
    ASSERT_EQ(2U, exceptions.size());

    // Fetch and check the first origin.
    const base::Value* value = &exceptions[0];
    ASSERT_TRUE(value->is_dict());
    const base::Value::Dict* dictionary = &value->GetDict();
    const std::string* primary_pattern =
        dictionary->FindString(site_settings::kOrigin);
    ASSERT_TRUE(primary_pattern);
    const std::string* display_name =
        dictionary->FindString(site_settings::kDisplayName);
    ASSERT_TRUE(display_name);

    EXPECT_EQ(kOriginToBlock, *primary_pattern);
    EXPECT_EQ(kOriginToBlock, *display_name);

    // Fetch and check the second origin.
    value = &exceptions[1];
    ASSERT_TRUE(value->is_dict());
    dictionary = &value->GetDict();

    primary_pattern = dictionary->FindString(site_settings::kOrigin);
    ASSERT_TRUE(primary_pattern);
    display_name = dictionary->FindString(site_settings::kDisplayName);
    ASSERT_TRUE(display_name);

    EXPECT_EQ(kOriginToEmbargo, *primary_pattern);
    EXPECT_EQ(kOriginToEmbargo, *display_name);
  }

  {
    // Non-permission types should not DCHECK when there is autoblocker data
    // present.
    base::Value::List exceptions;
    site_settings::GetExceptionsForContentType(
        kContentTypeCookies, &profile, /*extension_registry=*/nullptr,
        /*web_ui=*/nullptr,
        /*incognito=*/false, &exceptions);
    ASSERT_TRUE(exceptions.empty());
  }
}

// Test that the exception list contains embargo information for
// FEDERATED_IDENTITY_API even though FEDERATED_IDENTITY_API is a content
// setting (and not a permission).
TEST_F(SiteSettingsHelperTest, ExceptionListFedCmEmbargo) {
  TestingProfile profile;

  constexpr char kOriginToEmbargo[] = "https://embargoed.co.uk:443";
  auto* auto_blocker =
      PermissionDecisionAutoBlockerFactory::GetForProfile(&profile);
  auto_blocker->RecordDismissAndEmbargo(
      GURL(kOriginToEmbargo), ContentSettingsType::FEDERATED_IDENTITY_API,
      /*dismissed_prompt_was_quiet=*/false);

  base::Value::List exceptions;
  site_settings::GetExceptionsForContentType(
      ContentSettingsType::FEDERATED_IDENTITY_API, &profile,
      /*extension_registry=*/nullptr,
      /*web_ui=*/nullptr,
      /*incognito=*/false, &exceptions);

  // |exceptions| should have an exception for the embargoed origin.
  ASSERT_EQ(1U, exceptions.size());

  absl::optional<bool> is_embargoed =
      exceptions[0].GetDict().FindBool(site_settings::kIsEmbargoed);
  ASSERT_TRUE(is_embargoed.has_value());
  EXPECT_TRUE(*is_embargoed);
  const std::string* primary_pattern =
      exceptions[0].GetDict().FindString(site_settings::kOrigin);
  ASSERT_TRUE(primary_pattern);
  EXPECT_EQ(kOriginToEmbargo, *primary_pattern);
}

TEST_F(SiteSettingsHelperTest, CheckExceptionOrder) {
  TestingProfile profile;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  base::Value::List exceptions;
  // Check that the initial state of the map is empty.
  GetExceptionsForContentType(kContentType, &profile,
                              /*extension_registry=*/nullptr,
                              /*web_ui=*/nullptr,
                              /*incognito=*/false, &exceptions);
  EXPECT_TRUE(exceptions.empty());

  map->SetDefaultContentSetting(kContentType, CONTENT_SETTING_ALLOW);

  // Add a policy exception.
  std::string star_google_com = "http://[*.]google.com";
  auto policy_provider = std::make_unique<content_settings::MockProvider>();
  policy_provider->SetWebsiteSetting(
      ContentSettingsPattern::FromString(star_google_com),
      ContentSettingsPattern::Wildcard(), kContentType,
      base::Value(CONTENT_SETTING_BLOCK));
  policy_provider->set_read_only(true);
  content_settings::TestUtils::OverrideProvider(
      map, std::move(policy_provider), HostContentSettingsMap::POLICY_PROVIDER);

  // Add user preferences.
  std::string http_star = "http://*";
  std::string maps_google_com = "http://maps.google.com";
  AddSetting(map, http_star, CONTENT_SETTING_BLOCK);
  AddSetting(map, maps_google_com, CONTENT_SETTING_BLOCK);
  AddSetting(map, star_google_com, CONTENT_SETTING_ALLOW);

  // Add an extension exception.
  std::string drive_google_com = "http://drive.google.com";
  auto extension_provider = std::make_unique<content_settings::MockProvider>();
  extension_provider->SetWebsiteSetting(
      ContentSettingsPattern::FromString(drive_google_com),
      ContentSettingsPattern::Wildcard(), kContentType,
      base::Value(CONTENT_SETTING_ASK));
  extension_provider->set_read_only(true);
  content_settings::TestUtils::OverrideProvider(
      map, std::move(extension_provider),
      HostContentSettingsMap::CUSTOM_EXTENSION_PROVIDER);

  exceptions.clear();
  GetExceptionsForContentType(kContentType, &profile,
                              /*extension_registry=*/nullptr,
                              /*web_ui=*/nullptr,
                              /*incognito=*/false, &exceptions);

  EXPECT_EQ(5u, exceptions.size());

  // The policy exception should be returned first, the extension exception
  // second and pref exceptions afterwards.
  // The default content setting should not be returned.
  int i = 0;
  // From policy provider:
  VerifySetting(exceptions, i++, star_google_com, star_google_com,
                CONTENT_SETTING_BLOCK);
  // From extension provider:
  VerifySetting(exceptions, i++, drive_google_com, drive_google_com,
                CONTENT_SETTING_ASK);
  // From user preferences:
  VerifySetting(exceptions, i++, maps_google_com, maps_google_com,
                CONTENT_SETTING_BLOCK);
  VerifySetting(exceptions, i++, star_google_com, star_google_com,
                CONTENT_SETTING_ALLOW);
  VerifySetting(exceptions, i++, http_star, "http://*", CONTENT_SETTING_BLOCK);
}

// Tests the following content setting sources: Chrome default, user-set global
// default, user-set pattern, user-set origin setting, extension, and policy.
TEST_F(SiteSettingsHelperTest, ContentSettingSource) {
  TestingProfile profile;
  profile.SetPermissionControllerDelegate(
      permissions::GetPermissionControllerDelegate(&profile));
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL origin("https://www.example.com/");
  std::string source;
  std::string display_name;
  ContentSetting content_setting;

  // Built in Chrome default.
  content_setting = GetContentSettingForOrigin(
      &profile, map, origin, kContentType, &source, &display_name);
  EXPECT_EQ(SiteSettingSourceToString(SiteSettingSource::kDefault), source);
  EXPECT_EQ(CONTENT_SETTING_ASK, content_setting);

  // User-set global default.
  map->SetDefaultContentSetting(kContentType, CONTENT_SETTING_ALLOW);
  content_setting = GetContentSettingForOrigin(
      &profile, map, origin, kContentType, &source, &display_name);
  EXPECT_EQ(SiteSettingSourceToString(SiteSettingSource::kDefault), source);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, content_setting);

  // User-set pattern.
  AddSetting(map, "https://*", CONTENT_SETTING_BLOCK);
  content_setting = GetContentSettingForOrigin(
      &profile, map, origin, kContentType, &source, &display_name);
  EXPECT_EQ(SiteSettingSourceToString(SiteSettingSource::kPreference), source);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, content_setting);

  // User-set origin setting.
  map->SetContentSettingDefaultScope(origin, origin, kContentType,
                                     CONTENT_SETTING_ALLOW);
  content_setting = GetContentSettingForOrigin(
      &profile, map, origin, kContentType, &source, &display_name);
  EXPECT_EQ(SiteSettingSourceToString(SiteSettingSource::kPreference), source);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, content_setting);

  // Extension.
  auto extension_provider = std::make_unique<content_settings::MockProvider>();
  extension_provider->SetWebsiteSetting(ContentSettingsPattern::FromURL(origin),
                                        ContentSettingsPattern::FromURL(origin),
                                        kContentType,
                                        base::Value(CONTENT_SETTING_BLOCK));
  extension_provider->set_read_only(true);
  content_settings::TestUtils::OverrideProvider(
      map, std::move(extension_provider),
      HostContentSettingsMap::CUSTOM_EXTENSION_PROVIDER);
  content_setting = GetContentSettingForOrigin(
      &profile, map, origin, kContentType, &source, &display_name);
  EXPECT_EQ(SiteSettingSourceToString(SiteSettingSource::kExtension), source);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, content_setting);

  // Enterprise policy.
  auto policy_provider = std::make_unique<content_settings::MockProvider>();
  policy_provider->SetWebsiteSetting(ContentSettingsPattern::FromURL(origin),
                                     ContentSettingsPattern::FromURL(origin),
                                     kContentType,
                                     base::Value(CONTENT_SETTING_ALLOW));
  policy_provider->set_read_only(true);
  content_settings::TestUtils::OverrideProvider(
      map, std::move(policy_provider), HostContentSettingsMap::POLICY_PROVIDER);
  content_setting = GetContentSettingForOrigin(
      &profile, map, origin, kContentType, &source, &display_name);
  EXPECT_EQ(SiteSettingSourceToString(SiteSettingSource::kPolicy), source);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, content_setting);

  // Insecure origins.
  content_setting = GetContentSettingForOrigin(
      &profile, map, GURL("http://www.insecure_http_site.com/"), kContentType,
      &source, &display_name);
  EXPECT_EQ(SiteSettingSourceToString(SiteSettingSource::kInsecureOrigin),
            source);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, content_setting);
}

TEST_F(SiteSettingsHelperTest, CookieExceptions) {
  TestingProfile profile;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  struct TestCase {
    std::string primary_pattern;
    std::string secondary_pattern;
    ContentSetting initial_setting;
    ContentSetting updated_setting;
  };

  auto test_cases = std::vector<TestCase>{
      {"*", "[*.]allowed-top-frame.com", CONTENT_SETTING_ALLOW,
       CONTENT_SETTING_ALLOW},
      {"[*.]allowed.com", "*", CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW},
      {"[*.]allowed.com", "[*.]allowed-top-frame.com", CONTENT_SETTING_ALLOW,
       CONTENT_SETTING_ALLOW},
      {"*", "[*.]session-top-frame.com", CONTENT_SETTING_SESSION_ONLY,
       CONTENT_SETTING_ALLOW},
      {"[*.]session.com", "*", CONTENT_SETTING_SESSION_ONLY,
       CONTENT_SETTING_SESSION_ONLY},
      {"[*.]session.com", "[*.]session-top-frame.com",
       CONTENT_SETTING_SESSION_ONLY, CONTENT_SETTING_ALLOW},
      {"[*.]blocked.com", "[*.]blocked-top-frame.com", CONTENT_SETTING_BLOCK,
       CONTENT_SETTING_BLOCK},
      {"*", "[*.]blocked-top-frame.com", CONTENT_SETTING_BLOCK,
       CONTENT_SETTING_BLOCK},
      {"[*.]blocked.com", "*", CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK},
  };

  for (const auto& test_case : test_cases) {
    map->SetContentSettingCustomScope(
        ContentSettingsPattern::FromString(test_case.primary_pattern),
        ContentSettingsPattern::FromString(test_case.secondary_pattern),
        kContentTypeCookies, test_case.initial_setting);
  }

  for (const auto feature_state : std::vector<bool>{true, false}) {
    base::test::ScopedFeatureList feature_list_;
    feature_list_.InitWithFeatureState(
        privacy_sandbox::kPrivacySandboxSettings4, feature_state);

    base::Value::List exceptions;
    site_settings::GetExceptionsForContentType(kContentTypeCookies, &profile,
                                               /*extension_registry=*/nullptr,
                                               /*web_ui=*/nullptr,
                                               /*incognito=*/false,
                                               &exceptions);

    // Convert the test cases, and the returned dictionary, into tuples for
    // unordered comparison, as the order of exception is not relevant.
    std::vector<std::tuple<std::string, std::string, std::string>> expected;
    std::vector<std::tuple<std::string, std::string, std::string>> actual;
    base::ranges::transform(
        test_cases, std::back_inserter(expected), [&](const auto& test_case) {
          // make_tuple as we've some temporary rvalues.
          return std::make_tuple(
              test_case.primary_pattern,
              test_case.secondary_pattern ==
                      ContentSettingsPattern::Wildcard().ToString()
                  ? ""
                  : test_case.secondary_pattern,
              content_settings::ContentSettingToString(
                  feature_state ? test_case.updated_setting
                                : test_case.initial_setting));
        });
    base::ranges::transform(
        exceptions, std::back_inserter(actual), [](const auto& exception) {
          const base::Value::Dict& dict = exception.GetDict();
          return std::forward_as_tuple(*dict.FindString(kOrigin),
                                       *dict.FindString(kEmbeddingOrigin),
                                       *dict.FindString(kSetting));
        });

    EXPECT_THAT(actual, testing::UnorderedElementsAreArray(expected))
        << "Privacy Sandbox Settings 4 "
        << (feature_state ? "enabled" : "disabled");
  }
}

namespace {

void ExpectValidChooserExceptionObject(
    const base::Value::Dict& actual_exception_object,
    const std::string& expected_chooser_type,
    const std::u16string& expected_display_name,
    const base::Value::Dict& expected_chooser_object) {
  const std::string* actual_chooser_type =
      actual_exception_object.FindString(kChooserType);
  ASSERT_TRUE(actual_chooser_type);
  EXPECT_EQ(*actual_chooser_type, expected_chooser_type);

  const std::string* actual_display_name =
      actual_exception_object.FindString(kDisplayName);
  ASSERT_TRUE(actual_display_name);
  EXPECT_EQ(base::UTF8ToUTF16(*actual_display_name), expected_display_name);

  const base::Value::Dict* actual_chooser_object =
      actual_exception_object.FindDict(kObject);
  ASSERT_TRUE(actual_chooser_object);
  EXPECT_EQ(*actual_chooser_object, expected_chooser_object);

  const base::Value::List* sites_list =
      actual_exception_object.FindList(kSites);
  ASSERT_TRUE(sites_list);
}

void ExpectValidSiteExceptionObject(const base::Value& actual_site_object,
                                    const std::string& display_name,
                                    const GURL& origin,
                                    const std::string source,
                                    bool incognito) {
  ASSERT_TRUE(actual_site_object.is_dict());

  const base::Value::Dict& actual_site_dict = actual_site_object.GetDict();
  const std::string* display_name_value =
      actual_site_dict.FindString(kDisplayName);
  ASSERT_TRUE(display_name_value);
  EXPECT_EQ(*display_name_value, display_name);

  const std::string* origin_value = actual_site_dict.FindString(kOrigin);
  ASSERT_TRUE(origin_value);
  EXPECT_EQ(*origin_value, origin.DeprecatedGetOriginAsURL().spec());

  const std::string* setting_value = actual_site_dict.FindString(kSetting);
  ASSERT_TRUE(setting_value);
  EXPECT_EQ(*setting_value,
            content_settings::ContentSettingToString(CONTENT_SETTING_DEFAULT));

  const std::string* source_value = actual_site_dict.FindString(kSource);
  ASSERT_TRUE(source_value);
  EXPECT_EQ(*source_value, source);

  absl::optional<bool> incognito_value = actual_site_dict.FindBool(kIncognito);
  ASSERT_TRUE(incognito_value.has_value());
  EXPECT_EQ(*incognito_value, incognito);
}

}  // namespace

TEST_F(SiteSettingsHelperTest, CreateChooserExceptionObject) {
  const std::string kUsbChooserGroupName(
      ContentSettingsTypeToGroupName(ContentSettingsType::USB_CHOOSER_DATA));
  const std::string& kPolicySource =
      SiteSettingSourceToString(SiteSettingSource::kPolicy);
  const std::string& kPreferenceSource =
      SiteSettingSourceToString(SiteSettingSource::kPreference);
  const std::u16string& kObjectName = u"Gadget";
  ChooserExceptionDetails exception_details;

  // Create a chooser object for testing.
  base::Value::Dict chooser_object;
  chooser_object.Set("name", kObjectName);

  // Add a user permission for an origin of |kGoogleUrl|.
  const GURL kGoogleUrl("https://google.com");
  exception_details.insert({kGoogleUrl.DeprecatedGetOriginAsURL(),
                            kPreferenceSource, /*incognito=*/false});
  {
    auto exception = CreateChooserExceptionObject(
        /*display_name=*/kObjectName,
        /*object=*/base::Value(chooser_object.Clone()),
        /*chooser_type=*/kUsbChooserGroupName,
        /*chooser_exception_details=*/exception_details,
        /*profile=*/nullptr);
    ExpectValidChooserExceptionObject(
        exception, /*chooser_type=*/kUsbChooserGroupName,
        /*display_name=*/kObjectName, chooser_object);

    const auto& sites_list = exception.Find(kSites)->GetList();
    ExpectValidSiteExceptionObject(
        /*actual_site_object=*/sites_list[0],
        /*display_name=*/kGoogleUrl.DeprecatedGetOriginAsURL().spec(),
        /*origin=*/kGoogleUrl,
        /*source=*/kPreferenceSource,
        /*incognito=*/false);
  }

  // Add a user permissions for an origin of
  // |kAndroidUrl| granted in an off the record profile.
  const GURL kAndroidUrl("https://android.com");
  exception_details.insert({kAndroidUrl.DeprecatedGetOriginAsURL(),
                            kPreferenceSource, /*incognito=*/true});

  {
    auto exception = CreateChooserExceptionObject(
        /*display_name=*/kObjectName,
        /*object=*/base::Value(chooser_object.Clone()),
        /*chooser_type=*/kUsbChooserGroupName,
        /*chooser_exception_details=*/exception_details,
        /*profile=*/nullptr);
    ExpectValidChooserExceptionObject(
        exception,
        /*expected_chooser_type=*/kUsbChooserGroupName,
        /*expected_display_name=*/kObjectName, chooser_object);

    // The set sorts the sites by origin, so |kAndroidUrl| should
    // be first, followed by the origin |kGoogleOrigin|.
    const auto& sites_list = exception.Find(kSites)->GetList();
    ExpectValidSiteExceptionObject(
        /*actual_site_object=*/sites_list[0],
        /*display_name=*/kAndroidUrl.DeprecatedGetOriginAsURL().spec(),
        /*origin=*/kAndroidUrl,
        /*source=*/kPreferenceSource,
        /*incognito=*/true);
    ExpectValidSiteExceptionObject(
        /*actual_site_object=*/sites_list[1],
        /*display_name=*/kGoogleUrl.DeprecatedGetOriginAsURL().spec(),
        /*origin=*/kGoogleUrl,
        /*source=*/kPreferenceSource,
        /*incognito=*/false);
  }

  // Add a policy permission for an origin of |kGoogleUrl|.
  exception_details.insert({kGoogleUrl.DeprecatedGetOriginAsURL(),
                            kPolicySource, /*incognito=*/false});
  {
    auto exception = CreateChooserExceptionObject(
        /*display_name=*/kObjectName,
        /*object=*/base::Value(chooser_object.Clone()),
        /*chooser_type=*/kUsbChooserGroupName,
        /*chooser_exception_details=*/exception_details,
        /*profile=*/nullptr);
    ExpectValidChooserExceptionObject(exception,
                                      /*chooser_type=*/kUsbChooserGroupName,
                                      /*display_name=*/kObjectName,
                                      chooser_object);

    // The set sorts the sites by origin, but the CreateChooserExceptionObject
    // method sorts the sites further by the source. Therefore, policy granted
    // sites are listed before user granted sites.
    const auto& sites_list = exception.Find(kSites)->GetList();
    ExpectValidSiteExceptionObject(
        /*actual_site_object=*/sites_list[0],
        /*display_name=*/kGoogleUrl.DeprecatedGetOriginAsURL().spec(),
        /*origin=*/kGoogleUrl,
        /*source=*/kPolicySource,
        /*incognito=*/false);
    ExpectValidSiteExceptionObject(
        /*actual_site_object=*/sites_list[1],
        /*display_name=*/kAndroidUrl.DeprecatedGetOriginAsURL().spec(),
        /*origin=*/kAndroidUrl,
        /*source=*/kPreferenceSource,
        /*incognito=*/true);
    ExpectValidSiteExceptionObject(
        /*actual_site_object=*/sites_list[2],
        /*display_name=*/kGoogleUrl.DeprecatedGetOriginAsURL().spec(),
        /*origin=*/kGoogleUrl,
        /*source=*/kPreferenceSource,
        /*incognito=*/false);
  }
}

namespace {

constexpr char kUsbPolicySetting[] = R"(
    [
      {
        "devices": [{ "vendor_id": 6353, "product_id": 5678 }],
        "urls": ["https://chromium.org"]
      }, {
        "devices": [{ "vendor_id": 6353 }],
        "urls": ["https://google.com,https://android.com"]
      }, {
        "devices": [{ "vendor_id": 6354 }],
        "urls": ["https://android.com,"]
      }, {
        "devices": [{}],
        "urls": ["https://google.com,https://google.com"]
      }
    ])";

class SiteSettingsHelperChooserExceptionTest : public testing::Test {
 protected:
  const GURL kGoogleUrl{"https://google.com"};
  const GURL kChromiumUrl{"https://chromium.org"};
  const GURL kAndroidUrl{"https://android.com"};
  const GURL kTestUrl{"https://test.com"};

  Profile* profile() { return &profile_; }

  void SetUp() override { SetUpUsbChooserContext(); }

  // Sets up the UsbChooserContext with two devices and permissions for these
  // devices. It also adds three policy defined permissions. The two devices
  // represent the two types of USB devices, persistent and ephemeral, that can
  // be granted permission.
  void SetUpUsbChooserContext() {
    device::mojom::UsbDeviceInfoPtr persistent_device_info =
        device_manager_.CreateAndAddDevice(6353, 5678, "Google", "Gizmo",
                                           "123ABC");
    device::mojom::UsbDeviceInfoPtr ephemeral_device_info =
        device_manager_.CreateAndAddDevice(6354, 0, "Google", "Gadget", "");

    auto* chooser_context = UsbChooserContextFactory::GetForProfile(profile());
    mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager;
    device_manager_.AddReceiver(
        device_manager.InitWithNewPipeAndPassReceiver());
    chooser_context->SetDeviceManagerForTesting(std::move(device_manager));
    chooser_context->GetDevices(base::DoNothing());
    base::RunLoop().RunUntilIdle();

    const auto kAndroidOrigin = url::Origin::Create(kAndroidUrl);
    const auto kChromiumOrigin = url::Origin::Create(kChromiumUrl);
    const auto kTestOrigin = url::Origin::Create(kTestUrl);

    // Add the user granted permissions for testing. "Gizmo" is allowed on two
    // origins, one overlapping with the policy and one distinct. "Gadget" is
    // allowed on one origin which is overlapping with the policy.
    chooser_context->GrantDevicePermission(kTestOrigin,
                                           *persistent_device_info);
    chooser_context->GrantDevicePermission(kChromiumOrigin,
                                           *persistent_device_info);
    chooser_context->GrantDevicePermission(kAndroidOrigin,
                                           *ephemeral_device_info);

    // Add the policy granted permissions for testing.
    auto policy_value = base::JSONReader::Read(kUsbPolicySetting);
    DCHECK(policy_value);
    profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                               std::move(*policy_value));
  }

  device::FakeUsbDeviceManager device_manager_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

void ExpectDisplayNameEq(const base::Value& actual_exception_object,
                         const std::string& display_name) {
  const std::string* actual_display_name =
      actual_exception_object.GetDict().FindString(kDisplayName);
  ASSERT_TRUE(actual_display_name);
  EXPECT_EQ(*actual_display_name, display_name);
}

}  // namespace

TEST_F(SiteSettingsHelperChooserExceptionTest,
       GetChooserExceptionListFromProfile) {
  const std::string kUsbChooserGroupName(
      ContentSettingsTypeToGroupName(ContentSettingsType::USB_CHOOSER_DATA));
  const ChooserTypeNameEntry* chooser_type =
      ChooserTypeFromGroupName(kUsbChooserGroupName);
  const std::string& kPolicySource =
      SiteSettingSourceToString(SiteSettingSource::kPolicy);
  const std::string& kPreferenceSource =
      SiteSettingSourceToString(SiteSettingSource::kPreference);

  // The chooser exceptions are ordered by display name. Their corresponding
  // sites are ordered by permission source precedence, then by the origin.
  // User granted permissions that are also granted by policy are combined with
  // the policy so that duplicate permissions are not displayed.
  base::Value::List exceptions_list =
      GetChooserExceptionListFromProfile(profile(), *chooser_type);
  ASSERT_EQ(exceptions_list.size(), 4u);

  // This exception should describe the permissions for any device with the
  // vendor ID corresponding to "Google Inc.". There are no user granted
  // permissions that intersect with this permission, and this policy only
  // grants one permission to the "https://android.com" origin.
  {
    const auto& exception = exceptions_list[0];
    ExpectDisplayNameEq(exception,
                        /*display_name=*/"Devices from Google Inc.");

    const auto& sites_list = *exception.GetDict().FindList(kSites);
    ASSERT_EQ(sites_list.size(), 1u);
    ExpectValidSiteExceptionObject(
        sites_list[0],
        /*display_name=*/kAndroidUrl.DeprecatedGetOriginAsURL().spec(),
        /*origin=*/kAndroidUrl,
        /*source=*/kPolicySource,
        /*incognito=*/false);
  }

  // This exception should describe the permissions for any device.
  // There are no user granted permissions that intersect with this permission,
  // and this policy only grants one permission to the following
  // site: "https://google.com".
  {
    const auto& exception = exceptions_list[1];
    ExpectDisplayNameEq(exception,
                        /*display_name=*/"Devices from any vendor");

    const auto& sites_list = *exception.GetDict().FindList(kSites);
    ASSERT_EQ(sites_list.size(), 1u);
    ExpectValidSiteExceptionObject(
        sites_list[0],
        /*display_name=*/kGoogleUrl.DeprecatedGetOriginAsURL().spec(),
        /*origin=*/kGoogleUrl,
        /*source=*/kPolicySource,
        /*incognito=*/false);
  }

  // This exception should describe the permissions for any device with the
  // vendor ID 6354. There is a user granted permission for a device with that
  // vendor ID, so the site list for this exception will only have the policy
  // granted permission, which is the following: "https://android.com"
  {
    const auto& exception = exceptions_list[2];
    ExpectDisplayNameEq(exception,
                        /*display_name=*/"Devices from vendor 0x18D2");

    const auto& sites_list = *exception.GetDict().FindList(kSites);
    ASSERT_EQ(sites_list.size(), 1u);
    ExpectValidSiteExceptionObject(
        sites_list[0],
        /*display_name=*/kAndroidUrl.DeprecatedGetOriginAsURL().spec(),
        /*origin=*/kAndroidUrl,
        /*source=*/kPolicySource,
        /*incognito=*/false);
  }

  // This exception should describe the permissions for the "Gizmo" device.
  // The user granted permissions are the following:
  // * "https://chromium.org"
  // * "https://test.org"
  // The policy granted permission is the following:
  // * "https://chromium.org"
  // The chromium granted permission should be coalesced into the policy
  // permissions. The test one does not overlap with any policy permission so
  // it will be a separate preference-sourced exception.
  {
    const auto& exception = exceptions_list[3];
    ExpectDisplayNameEq(exception, /*display_name=*/"Gizmo");

    const auto& sites_list = *exception.GetDict().FindList(kSites);
    ASSERT_EQ(sites_list.size(), 2u);
    ExpectValidSiteExceptionObject(
        sites_list[0],
        /*display_name=*/kChromiumUrl.DeprecatedGetOriginAsURL().spec(),
        /*origin=*/kChromiumUrl,
        /*source=*/kPolicySource,
        /*incognito=*/false);
    ExpectValidSiteExceptionObject(
        sites_list[1],
        /*display_name=*/kTestUrl.DeprecatedGetOriginAsURL().spec(),
        /*origin=*/kTestUrl,
        /*source=*/kPreferenceSource,
        /*incognito=*/false);
  }
}

// TODO(crbug.com/1373962): Remove this testing class when
// Persistent Permissions is launched.
class PersistentPermissionsSiteSettingsHelperTest
    : public SiteSettingsHelperTest {
 public:
  PersistentPermissionsSiteSettingsHelperTest() {
    // Enable Persisted Permissions.
    feature_list_.InitAndEnableFeature(
        features::kFileSystemAccessPersistentPermissions);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Confirms that the allowed URLs returned from `GetGrantedEntries` are
// in accordance with File System Access Persisted Permissions.
TEST_F(PersistentPermissionsSiteSettingsHelperTest,
       ExceptionsGrantedViaPersistentPermissions) {
  TestingProfile profile;
  GURL origin("https://www.example.com/");
  const url::Origin kTestOrigin = url::Origin::Create(origin);

  const base::FilePath kTestPath =
      base::FilePath(FILE_PATH_LITERAL("/foo/bar"));

  const base::FilePath kTestPath2 = base::FilePath(FILE_PATH_LITERAL("/a/b/"));

  // Initialize and populate the `grants` object with permissions.
  ChromeFileSystemAccessPermissionContext* context =
      FileSystemAccessPermissionContextFactory::GetForProfile(&profile);
  auto empty_grants = context->GetPermissionGrants(kTestOrigin);
  EXPECT_TRUE(empty_grants.file_write_grants.empty());

  auto file_write_grant = context->GetWritePermissionGrant(
      kTestOrigin, kTestPath,
      ChromeFileSystemAccessPermissionContext::HandleType::kFile,
      ChromeFileSystemAccessPermissionContext::UserAction::kSave);
  auto file_read_grant = context->GetWritePermissionGrant(
      kTestOrigin, kTestPath2,
      ChromeFileSystemAccessPermissionContext::HandleType::kFile,
      ChromeFileSystemAccessPermissionContext::UserAction::kSave);
  auto populated_grants = context->GetPermissionGrants(kTestOrigin);
  EXPECT_FALSE(populated_grants.file_write_grants.empty());

  base::Value::List exceptions;
  site_settings::GetExceptionsForContentType(kContentTypeFileSystem, &profile,
                                             /*extension_registry=*/nullptr,
                                             /*web_ui=*/nullptr,
                                             /*incognito=*/false, &exceptions);

  // |exceptions| size should be 2 to account for the file write grant
  // and the file read grant. The display name and source of the
  // grants should match the file path and the "default" source,
  // respectively.
  EXPECT_EQ(exceptions.size(), 2U);
  ASSERT_EQ(exceptions[0].GetDict().Find("displayName")->GetString(), "/a/b/");
  ASSERT_EQ(exceptions[0].GetDict().Find("source")->GetString(), "default");
  ASSERT_EQ(exceptions[1].GetDict().Find("displayName")->GetString(),
            "/foo/bar");
  ASSERT_EQ(exceptions[1].GetDict().Find("source")->GetString(), "default");
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
class SiteSettingsHelperExtensionTest
    : public extensions::ExtensionServiceTestBase {
 public:
  SiteSettingsHelperExtensionTest()
      : extensions::ExtensionServiceTestBase(
            std::make_unique<content::BrowserTaskEnvironment>()) {}

  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    // The test profile is initialized in InitializeEmptyExtensionService().
    InitializeEmptyExtensionService();
  }

  scoped_refptr<const extensions::Extension> LoadExtension(
      const std::string& extension_name) {
    extensions::TestExtensionDir extension_directory;
    constexpr char kManifestTemplate[] = R"({
          "name": "%s",
          "version": "1",
          "manifest_version": 3
        })";
    extension_directory.WriteManifest(
        base::StringPrintf(kManifestTemplate, extension_name.c_str()));
    extensions::ChromeTestExtensionLoader loader(profile());
    return loader.LoadExtension(extension_directory.UnpackedPath());
  }

  void UnloadExtension(std::string extension_id) {
    auto* extension_service =
        extensions::ExtensionSystem::Get(profile())->extension_service();
    ASSERT_TRUE(extension_service);
    extension_service->UnloadExtension(
        extension_id, extensions::UnloadedExtensionReason::DISABLE);
  }
};

TEST_F(SiteSettingsHelperExtensionTest, CreateChooserExceptionObject) {
  const std::string kUsbChooserGroupName(
      ContentSettingsTypeToGroupName(ContentSettingsType::USB_CHOOSER_DATA));
  const std::string& kPreferenceSource =
      SiteSettingSourceToString(SiteSettingSource::kPreference);
  const std::u16string& kObjectName = u"Gadget";
  ChooserExceptionDetails exception_details;
  const std::string extension_name = "Test Extension";

  // Load the extension with name as |extension_name|.
  auto extension = LoadExtension(extension_name);

  // Create a chooser object for testing.
  base::Value::Dict chooser_object;
  chooser_object.Set("name", kObjectName);

  // Add a user permissions for an extension.
  auto extension_origin = extension->origin();
  exception_details.insert(
      {extension_origin.GetURL(), kPreferenceSource, /*incognito=*/false});

  // When the extension is loaded, the display name is extension's name.
  {
    auto exception = CreateChooserExceptionObject(
        /*display_name=*/kObjectName,
        /*object=*/base::Value(chooser_object.Clone()),
        /*chooser_type=*/kUsbChooserGroupName,
        /*chooser_exception_details=*/exception_details, profile());
    ExpectValidChooserExceptionObject(
        exception,
        /*expected_chooser_type=*/kUsbChooserGroupName,
        /*expected_display_name=*/kObjectName, chooser_object);

    const auto& sites_list = exception.Find(kSites)->GetList();
    ASSERT_EQ(sites_list.size(), 1u);
    ExpectValidSiteExceptionObject(
        /*actual_site_object=*/sites_list[0],
        /*display_name=*/extension_name,
        /*origin=*/extension_origin.GetURL(),
        /*source=*/kPreferenceSource,
        /*incognito=*/false);
  }

  // When the extension is unloaded, the display name is extension's origin as
  // the extension isn't available for the profile.
  UnloadExtension(extension->id());
  {
    auto exception = CreateChooserExceptionObject(
        /*display_name=*/kObjectName,
        /*object=*/base::Value(chooser_object.Clone()),
        /*chooser_type=*/kUsbChooserGroupName,
        /*chooser_exception_details=*/exception_details, profile());
    ExpectValidChooserExceptionObject(
        exception,
        /*expected_chooser_type=*/kUsbChooserGroupName,
        /*expected_display_name=*/kObjectName, chooser_object);

    const auto& sites_list = exception.Find(kSites)->GetList();
    ASSERT_EQ(sites_list.size(), 1u);
    ExpectValidSiteExceptionObject(
        /*actual_site_object=*/sites_list[0],
        /*display_name=*/
        extension_origin.GetURL().DeprecatedGetOriginAsURL().spec(),
        /*origin=*/extension_origin.GetURL(),
        /*source=*/kPreferenceSource,
        /*incognito=*/false);
  }
}
#endif  // #if BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace site_settings
