// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/site_settings_helper.h"

#include "base/check_deref.h"
#include "base/containers/to_vector.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/permissions/features.h"
#include "components/permissions/object_permission_context_base.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/test/permission_test_util.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/webui_allowlist.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/test/test_extension_dir.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace site_settings {

namespace {

using PermissionStatus = blink::mojom::PermissionStatus;
using ProviderType = content_settings::ProviderType;

constexpr ContentSettingsType kContentType = ContentSettingsType::GEOLOCATION;
constexpr ContentSettingsType kContentTypeCookies =
    ContentSettingsType::COOKIES;
constexpr ContentSettingsType kContentTypeFileSystem =
    ContentSettingsType::FILE_SYSTEM_WRITE_GUARD;
constexpr ContentSettingsType kContentTypeNotifications =
    ContentSettingsType::NOTIFICATIONS;
constexpr ContentSettingsType kContentTypeTrackingProtection =
    ContentSettingsType::TRACKING_PROTECTION;
}  // namespace

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

  static base::Time GetReferenceTime() {
    base::Time time;
    EXPECT_TRUE(base::Time::FromString("Sat, 1 Sep 2018 11:00:00", &time));
    return time;
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
                                             /*web_ui=*/nullptr,
                                             /*incognito=*/false, &exceptions);

  // |exceptions| size should be 2. One blocked and one embargoed origins.
  ASSERT_EQ(2U, exceptions.size());

  // Get last added origin.
  std::optional<bool> is_embargoed =
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
    ASSERT_EQ(PermissionStatus::DENIED,
              auto_blocker
                  ->GetEmbargoResult(GURL(kOriginToEmbargo),
                                     kContentTypeNotifications)
                  ->status);
  }

  // Check there is 1 embargoed origin for a non-incognito profile.
  {
    base::Value::List exceptions;
    site_settings::GetExceptionsForContentType(
        kContentTypeNotifications, &profile,
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
    EXPECT_EQ(PermissionStatus::DENIED,
              incognito_auto_blocker
                  ->GetEmbargoResult(GURL(kOriginToEmbargoIncognito),
                                     kContentTypeNotifications)
                  ->status);
  }

  // Check there are 2 blocked or embargoed origins for an incognito profile.
  {
    base::Value::List exceptions;
    site_settings::GetExceptionsForContentType(kContentTypeNotifications,
                                               incognito_profile,
                                               /*web_ui=*/nullptr,
                                               /*incognito=*/true, &exceptions);
    ASSERT_EQ(2U, exceptions.size());
  }
}

TEST_F(SiteSettingsHelperTest, ExceptionListFiltersIncognitoPolicyExceptions) {
  std::string test_url = "http://[*.]test.com";

  // Add a policy exception to the regular profile
  TestingProfile profile;
  auto* map = HostContentSettingsMapFactory::GetForProfile(&profile);
  auto policy_provider = std::make_unique<content_settings::MockProvider>();
  policy_provider->SetWebsiteSetting(
      ContentSettingsPattern::FromString(test_url),
      ContentSettingsPattern::Wildcard(), kContentTypeCookies,
      base::Value(CONTENT_SETTING_ALLOW), /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  policy_provider->set_read_only(true);
  content_settings::TestUtils::OverrideProvider(map, std::move(policy_provider),
                                                ProviderType::kPolicyProvider);

  // Check that the exception does not get filtered.
  base::Value::List exceptions;
  site_settings::GetExceptionsForContentType(kContentTypeCookies, &profile,
                                             /*web_ui=*/nullptr,
                                             /*incognito=*/true, &exceptions);
  ASSERT_EQ(1U, exceptions.size());

  // Add a policy exception to the incognito profile
  TestingProfile* incognito_profile =
      TestingProfile::Builder().BuildIncognito(&profile);
  auto* incognito_map =
      HostContentSettingsMapFactory::GetForProfile(incognito_profile);
  auto incognito_policy_provider =
      std::make_unique<content_settings::MockProvider>();
  incognito_policy_provider->SetWebsiteSetting(
      ContentSettingsPattern::FromString(test_url),
      ContentSettingsPattern::Wildcard(), kContentTypeCookies,
      base::Value(CONTENT_SETTING_ALLOW), /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  incognito_policy_provider->set_read_only(true);
  content_settings::TestUtils::OverrideProvider(
      incognito_map, std::move(incognito_policy_provider),
      ProviderType::kPolicyProvider);

  // Check that the exception gets filtered.
  base::Value::List incognito_exceptions;
  site_settings::GetExceptionsForContentType(
      kContentTypeCookies, incognito_profile,
      /*web_ui=*/nullptr,
      /*incognito=*/true, &incognito_exceptions);
  ASSERT_EQ(0U, incognito_exceptions.size());
}

TEST_F(SiteSettingsHelperTest, ExceptionListShowsEmbargoed) {
  TestingProfile profile;
  constexpr char kOriginToBlock[] = "https://www.blocked.com:443";
  constexpr char kOriginToEmbargo[] = "https://embargoed.co.uk:443";

  // Check there is no blocked origins.
  {
    base::Value::List exceptions;
    site_settings::GetExceptionsForContentType(
        kContentTypeNotifications, &profile,
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
        kContentTypeNotifications, &profile,
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
  EXPECT_EQ(PermissionStatus::DENIED,
            auto_blocker
                ->GetEmbargoResult(origin_to_embargo, kContentTypeNotifications)
                ->status);

  // Check there are 2 blocked origins.
  {
    base::Value::List exceptions;
    site_settings::GetExceptionsForContentType(
        kContentTypeNotifications, &profile,
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
    site_settings::GetExceptionsForContentType(kContentTypeCookies, &profile,
                                               /*web_ui=*/nullptr,
                                               /*incognito=*/false,
                                               &exceptions);
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
      /*web_ui=*/nullptr,
      /*incognito=*/false, &exceptions);

  // |exceptions| should have an exception for the embargoed origin.
  ASSERT_EQ(1U, exceptions.size());

  std::optional<bool> is_embargoed =
      exceptions[0].GetDict().FindBool(site_settings::kIsEmbargoed);
  ASSERT_TRUE(is_embargoed.has_value());
  EXPECT_TRUE(*is_embargoed);
  const std::string* primary_pattern =
      exceptions[0].GetDict().FindString(site_settings::kOrigin);
  ASSERT_TRUE(primary_pattern);
  EXPECT_EQ(kOriginToEmbargo, *primary_pattern);
}

TEST_F(SiteSettingsHelperTest, ExceptionListIgnoresWebUIAllowlist) {
  TestingProfile profile;
  auto* allowlist = WebUIAllowlist::GetOrCreate(&profile);

  // Confirm that WebUI allowlist entries are excluded from the exception list.
  allowlist->RegisterAutoGrantedPermission(
      url::Origin::Create(GURL("chrome://example.com")),
      ContentSettingsType::COOKIES);

  // Secondary patterns should also be ignored.
  allowlist->RegisterAutoGrantedThirdPartyCookies(
      url::Origin::Create(GURL("chrome-untrusted://another-example.com")),
      {
          ContentSettingsPattern::FromURL(GURL("https://embedded-1.com")),
          ContentSettingsPattern::FromURL(GURL("https://embedded-2.com")),
      });

  base::Value::List exceptions;
  site_settings::GetExceptionsForContentType(ContentSettingsType::COOKIES,
                                             &profile,
                                             /*web_ui=*/nullptr,
                                             /*incognito=*/false, &exceptions);
  ASSERT_EQ(0U, exceptions.size());

  // Exceptions from other sources that use a WebUI scheme should however be
  // displayed.
  auto* map = HostContentSettingsMapFactory::GetForProfile(&profile);
  map->SetContentSettingDefaultScope(
      GURL("chrome://example"), GURL("chrome-untrusted://another-example"),
      ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);

  site_settings::GetExceptionsForContentType(ContentSettingsType::COOKIES,
                                             &profile,
                                             /*web_ui=*/nullptr,
                                             /*incognito=*/false, &exceptions);
  ASSERT_EQ(1U, exceptions.size());
}

TEST_F(SiteSettingsHelperTest, CheckExceptionOrder) {
  TestingProfile profile;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  base::Value::List exceptions;
  // Check that the initial state of the map is empty.
  GetExceptionsForContentType(kContentType, &profile,
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
      base::Value(CONTENT_SETTING_BLOCK), /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  policy_provider->set_read_only(true);
  content_settings::TestUtils::OverrideProvider(map, std::move(policy_provider),
                                                ProviderType::kPolicyProvider);

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
      base::Value(CONTENT_SETTING_ASK), /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  extension_provider->set_read_only(true);
  content_settings::TestUtils::OverrideProvider(
      map, std::move(extension_provider),
      ProviderType::kCustomExtensionProvider);

  exceptions.clear();
  GetExceptionsForContentType(kContentType, &profile,
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
  SiteSettingSource source;
  ContentSetting content_setting;

  // Built in Chrome default.
  content_setting =
      GetContentSettingForOrigin(&profile, map, origin, kContentType, &source);
  EXPECT_EQ(SiteSettingSource::kDefault, source);
  EXPECT_EQ(CONTENT_SETTING_ASK, content_setting);

  // User-set global default.
  map->SetDefaultContentSetting(kContentType, CONTENT_SETTING_ALLOW);
  content_setting =
      GetContentSettingForOrigin(&profile, map, origin, kContentType, &source);
  EXPECT_EQ(SiteSettingSource::kDefault, source);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, content_setting);

  // User-set pattern.
  AddSetting(map, "https://*", CONTENT_SETTING_BLOCK);
  content_setting =
      GetContentSettingForOrigin(&profile, map, origin, kContentType, &source);
  EXPECT_EQ(SiteSettingSource::kPreference, source);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, content_setting);

  // User-set origin setting.
  map->SetContentSettingDefaultScope(origin, origin, kContentType,
                                     CONTENT_SETTING_ALLOW);
  content_setting =
      GetContentSettingForOrigin(&profile, map, origin, kContentType, &source);
  EXPECT_EQ(SiteSettingSource::kPreference, source);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, content_setting);

  // Extension.
  auto extension_provider = std::make_unique<content_settings::MockProvider>();
  extension_provider->SetWebsiteSetting(
      ContentSettingsPattern::FromURL(origin),
      ContentSettingsPattern::FromURL(origin), kContentType,
      base::Value(CONTENT_SETTING_BLOCK), /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  extension_provider->set_read_only(true);
  content_settings::TestUtils::OverrideProvider(
      map, std::move(extension_provider),
      ProviderType::kCustomExtensionProvider);
  content_setting =
      GetContentSettingForOrigin(&profile, map, origin, kContentType, &source);
  EXPECT_EQ(SiteSettingSource::kExtension, source);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, content_setting);

  // Enterprise policy.
  auto policy_provider = std::make_unique<content_settings::MockProvider>();
  policy_provider->SetWebsiteSetting(
      ContentSettingsPattern::FromURL(origin),
      ContentSettingsPattern::FromURL(origin), kContentType,
      base::Value(CONTENT_SETTING_ALLOW), /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  policy_provider->set_read_only(true);
  content_settings::TestUtils::OverrideProvider(map, std::move(policy_provider),
                                                ProviderType::kPolicyProvider);
  content_setting =
      GetContentSettingForOrigin(&profile, map, origin, kContentType, &source);
  EXPECT_EQ(SiteSettingSource::kPolicy, source);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, content_setting);

  // Insecure origins.
  content_setting = GetContentSettingForOrigin(
      &profile, map, GURL("http://www.insecure_http_site.com/"), kContentType,
      &source);
  EXPECT_EQ(SiteSettingSource::kInsecureOrigin, source);
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

  base::Value::List exceptions;
  site_settings::GetExceptionsForContentType(kContentTypeCookies, &profile,
                                             /*web_ui=*/nullptr,
                                             /*incognito=*/false, &exceptions);

  // Convert the test cases, and the returned dictionary, into tuples for
  // unordered comparison, as the order of exception is not relevant.
  std::vector<std::tuple<std::string, std::string, std::string>> expected =
      base::ToVector(test_cases, [&](const auto& test_case) {
        // make_tuple as we've some temporary rvalues.
        return std::make_tuple(
            test_case.primary_pattern,
            test_case.secondary_pattern ==
                    ContentSettingsPattern::Wildcard().ToString()
                ? ""
                : test_case.secondary_pattern,
            content_settings::ContentSettingToString(
                test_case.updated_setting));
      });

  std::vector<std::tuple<std::string, std::string, std::string>> actual =
      base::ToVector(exceptions, [](const auto& exception) {
        const base::Value::Dict& dict = exception.GetDict();
        return std::make_tuple(*dict.FindString(kOrigin),
                               *dict.FindString(kEmbeddingOrigin),
                               *dict.FindString(kSetting));
      });

  EXPECT_THAT(actual, testing::UnorderedElementsAreArray(expected));
}

TEST_F(SiteSettingsHelperTest,
       TrackingProtectionExceptionsListIncludes3pcExceptions) {
  TestingProfile profile;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  // Add Tracking Protection exception
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString("some-site.com"),
      kContentTypeTrackingProtection, CONTENT_SETTING_ALLOW);
  // Add 3PC exception
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString("third-party-cookies.com"),
      kContentTypeCookies, CONTENT_SETTING_ALLOW);
  // Add 1PC exception
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("first-party-cookies.com"),
      ContentSettingsPattern::Wildcard(), kContentTypeCookies,
      CONTENT_SETTING_ALLOW);

  // Check that cookies list has two exceptions.
  base::Value::List cookie_exceptions;
  site_settings::GetExceptionsForContentType(kContentTypeCookies, &profile,
                                             /*web_ui=*/nullptr,
                                             /*incognito=*/false,
                                             &cookie_exceptions);
  ASSERT_EQ(2U, cookie_exceptions.size());

  // Check that Tracking Protection list has two exceptions.
  base::Value::List tp_exceptions;
  site_settings::GetExceptionsForContentType(
      kContentTypeTrackingProtection, &profile,
      /*web_ui=*/nullptr,
      /*incognito=*/false, &tp_exceptions);
  ASSERT_EQ(2U, tp_exceptions.size());

  // Verify the TP exception
  ASSERT_TRUE(tp_exceptions[0].GetDict().contains(kType));
  EXPECT_EQ(ContentSettingsTypeFromGroupName(
                *tp_exceptions[0].GetDict().FindString(kType)),
            kContentTypeTrackingProtection);
  ASSERT_TRUE(tp_exceptions[0].GetDict().contains(kEmbeddingOrigin));
  EXPECT_EQ(*tp_exceptions[0].GetDict().FindString(kEmbeddingOrigin),
            "some-site.com");
  EXPECT_FALSE(tp_exceptions[0].GetDict().contains(kDescription));
  // Verify the 3PC exception
  ASSERT_TRUE(tp_exceptions[1].GetDict().contains(kType));
  EXPECT_EQ(ContentSettingsTypeFromGroupName(
                *tp_exceptions[1].GetDict().FindString(kType)),
            kContentTypeCookies);
  ASSERT_TRUE(tp_exceptions[1].GetDict().contains(kEmbeddingOrigin));
  EXPECT_EQ(*tp_exceptions[1].GetDict().FindString(kEmbeddingOrigin),
            "third-party-cookies.com");
  ASSERT_TRUE(tp_exceptions[1].GetDict().contains(kDescription));
  EXPECT_EQ(
      base::UTF8ToUTF16(*tp_exceptions[1].GetDict().FindString(kDescription)),
      l10n_util::GetStringUTF16(
          IDS_SETTINGS_THIRD_PARTY_COOKIES_ONLY_EXCEPTION_LABEL));
}

TEST_F(SiteSettingsHelperTest,
       TrackingProtectionExceptionsListIncludes3pcExceptionsWithSamePattern) {
  TestingProfile profile;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  // Add Tracking Protection exception
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString("some-site.com"),
      kContentTypeTrackingProtection, CONTENT_SETTING_ALLOW);
  // Add 3PC exception for same pattern
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString("some-site.com"), kContentTypeCookies,
      CONTENT_SETTING_ALLOW);

  // Check that Tracking Protection list has two exceptions.
  base::Value::List tp_exceptions;
  site_settings::GetExceptionsForContentType(
      kContentTypeTrackingProtection, &profile,
      /*web_ui=*/nullptr,
      /*incognito=*/false, &tp_exceptions);
  ASSERT_EQ(2U, tp_exceptions.size());

  // Verify the TP exception
  ASSERT_TRUE(tp_exceptions[0].GetDict().contains(kType));
  EXPECT_EQ(ContentSettingsTypeFromGroupName(
                *tp_exceptions[0].GetDict().FindString(kType)),
            kContentTypeTrackingProtection);
  ASSERT_TRUE(tp_exceptions[0].GetDict().contains(kEmbeddingOrigin));
  EXPECT_EQ(*tp_exceptions[0].GetDict().FindString(kEmbeddingOrigin),
            "some-site.com");
  EXPECT_FALSE(tp_exceptions[0].GetDict().contains(kDescription));
  // Verify the 3PC exception, which will have the same embedding origin
  ASSERT_TRUE(tp_exceptions[1].GetDict().contains(kType));
  EXPECT_EQ(ContentSettingsTypeFromGroupName(
                *tp_exceptions[1].GetDict().FindString(kType)),
            kContentTypeCookies);
  ASSERT_TRUE(tp_exceptions[1].GetDict().contains(kEmbeddingOrigin));
  EXPECT_EQ(*tp_exceptions[1].GetDict().FindString(kEmbeddingOrigin),
            "some-site.com");
  ASSERT_TRUE(tp_exceptions[1].GetDict().contains(kDescription));
  EXPECT_EQ(
      base::UTF8ToUTF16(*tp_exceptions[1].GetDict().FindString(kDescription)),
      l10n_util::GetStringUTF16(
          IDS_SETTINGS_THIRD_PARTY_COOKIES_ONLY_EXCEPTION_LABEL));
}

TEST_F(SiteSettingsHelperTest, GetExpirationDescription) {
  base::subtle::ScopedTimeClockOverrides time_override(
      &SiteSettingsHelperTest::GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  auto description =
      GetExpirationDescription(GetReferenceTime() + base::Days(0));

  EXPECT_EQ(description, l10n_util::GetPluralStringFUTF16(
                             IDS_SETTINGS_EXPIRES_AFTER_TIME_LABEL, 0));
}

TEST_F(SiteSettingsHelperTest, GetExpirationDescription_Tomorrow) {
  base::subtle::ScopedTimeClockOverrides time_override(
      &SiteSettingsHelperTest::GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  auto description =
      GetExpirationDescription(GetReferenceTime() + base::Days(1));

  EXPECT_EQ(description, l10n_util::GetPluralStringFUTF16(
                             IDS_SETTINGS_EXPIRES_AFTER_TIME_LABEL, 1));
}

TEST_F(SiteSettingsHelperTest,
       GetExpirationDescription_Tomorrow_LessThan24_AfterMidnight) {
  base::subtle::ScopedTimeClockOverrides time_override(
      &SiteSettingsHelperTest::GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  auto description =
      GetExpirationDescription(GetReferenceTime() + base::Hours(14));

  EXPECT_EQ(description, l10n_util::GetPluralStringFUTF16(
                             IDS_SETTINGS_EXPIRES_AFTER_TIME_LABEL, 1));
}

TEST_F(SiteSettingsHelperTest,
       GetExpirationDescription_Tomorrow_LessThan24_BeforeMidnight) {
  base::subtle::ScopedTimeClockOverrides time_override(
      &SiteSettingsHelperTest::GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  auto description =
      GetExpirationDescription(GetReferenceTime() + base::Hours(12));
  EXPECT_EQ(description, l10n_util::GetPluralStringFUTF16(
                             IDS_SETTINGS_EXPIRES_AFTER_TIME_LABEL, 0));
}

TEST_F(SiteSettingsHelperTest, GetExpirationDescription_Expired) {
  base::subtle::ScopedTimeClockOverrides time_override(
      &SiteSettingsHelperTest::GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  auto description =
      GetExpirationDescription(GetReferenceTime() - base::Days(4));
  EXPECT_EQ(description, l10n_util::GetPluralStringFUTF16(
                             IDS_SETTINGS_EXPIRES_AFTER_TIME_LABEL, 0));
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
                                    const SiteSettingSource source,
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
  EXPECT_EQ(*source_value, SiteSettingSourceToString(source));

  std::optional<bool> incognito_value = actual_site_dict.FindBool(kIncognito);
  ASSERT_TRUE(incognito_value.has_value());
  EXPECT_EQ(*incognito_value, incognito);
}

}  // namespace

TEST_F(SiteSettingsHelperTest, CreateChooserExceptionObject) {
  const std::string kUsbChooserGroupName(
      ContentSettingsTypeToGroupName(ContentSettingsType::USB_CHOOSER_DATA));
  auto kPolicySource = SiteSettingSource::kPolicy;
  auto kPreferenceSource = SiteSettingSource::kPreference;
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

TEST_F(SiteSettingsHelperTest, ShowAutograntedRWSPermissions) {
  TestingProfile profile;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      permissions::features::kShowRelatedWebsiteSetsPermissionGrants);
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  content_settings::ContentSettingConstraints constraint;
  constraint.set_session_model(
      content_settings::mojom::SessionModel::NON_RESTORABLE_USER_SESSION);
  constexpr char kToplevelURL[] = "https://firstparty.com";
  constexpr char kEmbeddedURL[] = "https://embedded.com";
  map->SetContentSettingDefaultScope(GURL(kEmbeddedURL), GURL(kToplevelURL),
                                     ContentSettingsType::STORAGE_ACCESS,
                                     CONTENT_SETTING_BLOCK, constraint);

  base::Value::List exceptions;
  site_settings::GetExceptionsForContentType(
      ContentSettingsType::STORAGE_ACCESS, &profile,
      /*web_ui=*/nullptr,
      /*incognito=*/false, &exceptions);
  EXPECT_EQ(1U, exceptions.size());
  EXPECT_EQ(exceptions[0].GetDict().Find("setting")->GetString(), "block");
  EXPECT_EQ(exceptions[0].GetDict().Find("origin")->GetString(),
            "https://[*.]embedded.com");
  EXPECT_EQ(exceptions[0].GetDict().Find("embeddingOrigin")->GetString(),
            "https://[*.]firstparty.com");
}

TEST_F(SiteSettingsHelperTest, HideAutograntedRWSPermissions) {
  TestingProfile profile;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      permissions::features::kShowRelatedWebsiteSetsPermissionGrants);
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  content_settings::ContentSettingConstraints constraint;
  constraint.set_session_model(
      content_settings::mojom::SessionModel::NON_RESTORABLE_USER_SESSION);
  constexpr char kToplevelURL[] = "https://firstparty.com";
  constexpr char kEmbeddedURL[] = "https://embedded.com";
  map->SetContentSettingDefaultScope(GURL(kEmbeddedURL), GURL(kToplevelURL),
                                     ContentSettingsType::STORAGE_ACCESS,
                                     CONTENT_SETTING_BLOCK, constraint);

  base::Value::List exceptions;
  site_settings::GetExceptionsForContentType(
      ContentSettingsType::STORAGE_ACCESS, &profile,
      /*web_ui=*/nullptr,
      /*incognito=*/false, &exceptions);
  EXPECT_TRUE(exceptions.empty());
}

TEST_F(SiteSettingsHelperTest, AutomaticFullscreenVisibility) {
  TestingProfile profile;
  profile.SetPermissionControllerDelegate(
      permissions::GetPermissionControllerDelegate(&profile));
  base::test::ScopedFeatureList feature_list{
      features::kAutomaticFullscreenContentSetting};
  const ContentSettingsType type = ContentSettingsType::AUTOMATIC_FULLSCREEN;

  // Automatic Fullscreen is visible for non-origin-specific lists.
  auto types = GetVisiblePermissionCategories();
  EXPECT_TRUE(base::Contains(types, type));

  constexpr char kDefault[] = "https://www.default.com:443";
  constexpr char kAllowed[] = "https://www.allowed.com:443";

  // Automatic Fullscreen is not visible for sites with the default BLOCK value.
  SiteSettingSource source;
  auto* map = HostContentSettingsMapFactory::GetForProfile(&profile);
  ContentSetting content_setting =
      GetContentSettingForOrigin(&profile, map, GURL(kDefault), type, &source);
  EXPECT_EQ(SiteSettingSource::kDefault, source);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, content_setting);
  types = GetVisiblePermissionCategories(kDefault, &profile);
  EXPECT_FALSE(base::Contains(types, type));

  // Simulate allowing Automatic Fullscreen through enterprise policy.
  auto policy_provider = std::make_unique<content_settings::MockProvider>();
  policy_provider->SetWebsiteSetting(
      ContentSettingsPattern::FromString(kAllowed),
      ContentSettingsPattern::FromString(kAllowed), type,
      base::Value(CONTENT_SETTING_ALLOW), /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  policy_provider->set_read_only(true);
  content_settings::TestUtils::OverrideProvider(map, std::move(policy_provider),
                                                ProviderType::kPolicyProvider);

  // Automatic Fullscreen is visible for origins with non-default values.
  content_setting =
      GetContentSettingForOrigin(&profile, map, GURL(kAllowed), type, &source);
  EXPECT_EQ(SiteSettingSource::kPolicy, source);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, content_setting);
  types = GetVisiblePermissionCategories(kAllowed, &profile);
  EXPECT_TRUE(base::Contains(types, type));
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(SiteSettingsHelperTest, WebPrintingVisibility) {
  TestingProfile profile;
  profile.SetPermissionControllerDelegate(
      permissions::GetPermissionControllerDelegate(&profile));
  base::test::ScopedFeatureList feature_list{blink::features::kWebPrinting};
  const ContentSettingsType type = ContentSettingsType::WEB_PRINTING;

  // Web Printing is visible for non-origin-specific lists.
  EXPECT_TRUE(base::Contains(GetVisiblePermissionCategories(), type));

  constexpr char kDefault[] = "https://www.default.com:443";
  constexpr char kAllowed[] = "https://www.allowed.com:443";
  constexpr char kIwa[] =
      "isolated-app://aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";

  // Web Printing is not visible for sites with the default source.
  EXPECT_FALSE(
      base::Contains(GetVisiblePermissionCategories(kDefault, &profile), type));

  // Web Printing is always visible for IWA origins.
  EXPECT_TRUE(
      base::Contains(GetVisiblePermissionCategories(kIwa, &profile), type));

  // Simulate allowing Web Printing through enterprise policy.
  auto policy_provider = std::make_unique<content_settings::MockProvider>();
  policy_provider->SetWebsiteSetting(
      ContentSettingsPattern::FromString(kAllowed),
      ContentSettingsPattern::FromString(kAllowed), type,
      base::Value(CONTENT_SETTING_ALLOW), /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  content_settings::TestUtils::OverrideProvider(
      HostContentSettingsMapFactory::GetForProfile(&profile),
      std::move(policy_provider), ProviderType::kPolicyProvider);

  // Web Printing is visible for origins with non-default sources.
  EXPECT_TRUE(
      base::Contains(GetVisiblePermissionCategories(kAllowed, &profile), type));
}
#endif

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
  auto kPolicySource = SiteSettingSource::kPolicy;
  auto kPreferenceSource = SiteSettingSource::kPreference;

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

// TODO(crbug.com/40101962): Remove usage of this testing class when the feature
// flag for Persistent Permissions is removed.
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
  auto empty_grants =
      context->ConvertObjectsToGrants(context->GetGrantedObjects(kTestOrigin));
  EXPECT_TRUE(empty_grants.file_write_grants.empty());

  context->SetOriginHasExtendedPermissionForTesting(kTestOrigin);

  auto file_write_grant = context->GetWritePermissionGrant(
      kTestOrigin, content::PathInfo(kTestPath),
      ChromeFileSystemAccessPermissionContext::HandleType::kFile,
      ChromeFileSystemAccessPermissionContext::UserAction::kSave);
  auto file_read_grant = context->GetWritePermissionGrant(
      kTestOrigin, content::PathInfo(kTestPath2),
      ChromeFileSystemAccessPermissionContext::HandleType::kFile,
      ChromeFileSystemAccessPermissionContext::UserAction::kSave);

  auto populated_grants =
      context->ConvertObjectsToGrants(context->GetGrantedObjects(kTestOrigin));
  EXPECT_FALSE(populated_grants.file_write_grants.empty());

  base::Value::List exceptions;
  site_settings::GetExceptionsForContentType(kContentTypeFileSystem, &profile,
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
  auto kPreferenceSource = SiteSettingSource::kPreference;
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

TEST_F(SiteSettingsHelperExtensionTest,
       ExceptionsUseExtensionNameAsDisplayName) {
  const std::string extension_name = "Test Extension";
  auto extension = LoadExtension(extension_name);

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  GURL extension_origin = extension->origin().GetURL();
  map->SetContentSettingDefaultScope(extension_origin, extension_origin,
                                     kContentTypeNotifications,
                                     CONTENT_SETTING_BLOCK);

  base::Value::List exceptions;
  site_settings::GetExceptionsForContentType(kContentTypeNotifications,
                                             profile(),
                                             /*web_ui=*/nullptr,
                                             /*incognito=*/false, &exceptions);

  ASSERT_EQ(exceptions.size(), 1u);
  const base::Value::Dict& exception = exceptions[0].GetDict();
  EXPECT_EQ(CHECK_DEREF(exception.FindString(kOrigin)), extension_origin);
  EXPECT_EQ(CHECK_DEREF(exception.FindString(kDisplayName)), extension_name);
}
#endif  // #if BUILDFLAG(ENABLE_EXTENSIONS)

class SiteSettingsHelperIsolatedWebAppTest : public testing::Test {
 protected:
  void InstallIsolatedWebApp(const GURL& url, const std::string& name) {
    web_app::test::AwaitStartWebAppProviderAndSubsystems(&testing_profile_);
    web_app::AddDummyIsolatedAppToRegistry(&testing_profile_, url, name);
  }

  Profile* profile() { return &testing_profile_; }

  const GURL kAppUrl{
      "isolated-app://"
      "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic"};
  const std::string kAppName = "test IWA Name";

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  web_app::test::ScopedSkipMainProfileCheck skip_main_profile_check_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

TEST_F(SiteSettingsHelperIsolatedWebAppTest,
       IsolatedWebAppsUseAppNameAsDisplayName) {
  const std::string kUsbChooserGroupName(
      ContentSettingsTypeToGroupName(ContentSettingsType::USB_CHOOSER_DATA));
  auto kPreferenceSource = SiteSettingSource::kPreference;
  const std::u16string& kObjectName = u"Gadget";

  InstallIsolatedWebApp(kAppUrl, kAppName);

  // Create a chooser object for testing.
  base::Value::Dict chooser_object;
  chooser_object.Set("name", kObjectName);

  // Add a user permission for an origin of |kAppUrl|.
  ChooserExceptionDetails exception_details;
  exception_details.insert({kAppUrl.DeprecatedGetOriginAsURL(),
                            kPreferenceSource, /*incognito=*/false});
  {
    auto exception = CreateChooserExceptionObject(
        /*display_name=*/kObjectName,
        /*object=*/base::Value(chooser_object.Clone()),
        /*chooser_type=*/kUsbChooserGroupName,
        /*chooser_exception_details=*/exception_details,
        /*profile=*/profile());
    ExpectValidChooserExceptionObject(
        exception, /*chooser_type=*/kUsbChooserGroupName,
        /*display_name=*/kObjectName, chooser_object);

    const auto& sites_list = exception.Find(kSites)->GetList();
    ExpectValidSiteExceptionObject(
        /*actual_site_object=*/sites_list[0],
        /*display_name=*/kAppName,
        /*origin=*/kAppUrl,
        /*source=*/kPreferenceSource,
        /*incognito=*/false);
  }
}

TEST_F(SiteSettingsHelperIsolatedWebAppTest, AutomaticFullscreenVisibility) {
  base::test::ScopedFeatureList feature_list{
      features::kAutomaticFullscreenContentSetting};
  const ContentSettingsType type = ContentSettingsType::AUTOMATIC_FULLSCREEN;
  InstallIsolatedWebApp(kAppUrl, kAppName);

  // Automatic Fullscreen is visible for IWAs, even with default BLOCK values.
  SiteSettingSource source;
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  ContentSetting content_setting =
      GetContentSettingForOrigin(profile(), map, kAppUrl, type, &source);
  EXPECT_EQ(SiteSettingSource::kDefault, source);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, content_setting);
  const auto types = GetVisiblePermissionCategories(kAppUrl.spec(), profile());
  EXPECT_TRUE(base::ranges::any_of(types, [](auto& t) { return t == type; }));
}

}  // namespace site_settings
