// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/site_settings_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context_factory.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"
#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/permissions/system/mock_platform_handle.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/privacy_sandbox/mock_privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/browsing_data/content/browsing_data_model.h"
#include "components/browsing_data/content/browsing_data_model_test_util.h"
#include "components/browsing_data/content/fake_browsing_data_model.h"
#include "components/browsing_data/content/mock_cookie_helper.h"
#include "components/browsing_data/content/mock_local_storage_helper.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/browsing_topics/test_util.h"
#include "components/client_hints/common/client_hints.h"
#include "components/content_settings/core/browser/content_settings_uma_util.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/history/core/browser/history_service.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/permissions/contexts/bluetooth_chooser_context.h"
#include "components/permissions/object_permission_context_base.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/test/object_permission_context_base_mock_permission_observer.h"
#include "components/permissions/test/permission_test_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/device/public/cpp/test/fake_hid_manager.h"
#include "services/device/public/cpp/test/fake_serial_port_manager.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/webui/webui_allowlist.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#endif

namespace {

using ::base::test::ParseJson;
using ::base::test::RunClosure;
using ::base::test::TestFuture;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

using GroupingKey = settings::SiteSettingsHandler::GroupingKey;
using PermissionStatus = blink::mojom::PermissionStatus;

constexpr char kCallbackId[] = "test-callback-id";
constexpr char kSetting[] = "setting";
constexpr char kSource[] = "source";
constexpr char kExtensionName[] = "Test Extension";
constexpr char kTestUserEmail[] = "user@example.com";

const struct PatternContentTypeTestCase {
  struct {
    const char* const pattern;
    const char* const content_type;
  } arguments;
  struct {
    const bool validity;
    const char* const reason;
  } expected;
} kPatternsAndContentTypeTestCases[]{
    {{"https://google.com", "cookies"}, {true, ""}},
    {{";", "cookies"}, {true, ""}},
    {{"^", "cookies"}, {false, "Not a valid web address"}},
    {{"*", "cookies"}, {false, "Not a valid web address"}},
    {{"chrome://test", "popups"}, {false, "Not a valid web address"}},
    {{"chrome-untrusted://test", "popups"}, {false, "Not a valid web address"}},
    {{"devtools://devtools", "popups"}, {false, "Not a valid web address"}},
    {{"chrome-search://search", "popups"}, {false, "Not a valid web address"}},
    {{"http://google.com", "location"}, {false, "Origin must be secure"}},
    {{"http://127.0.0.1", "location"}, {true, ""}},  // Localhost is secure.
    {{"http://[::1]", "location"}, {true, ""}}};

struct EmbeddingStorageAccessException {
  const std::string embedding_origin;
  const std::string embedding_display_name;
  const bool incognito;
  const bool embargoed = false;
  const int lifetime_in_days = 0;
};

// Matchers to make verifying GroupingKey contents easier.
MATCHER_P(IsEtldPlus1, etld_plus1, "") {
  return arg == std::string("etld:") + etld_plus1;
}

MATCHER_P(IsOrigin, origin, "") {
  return arg == std::string("origin:") + origin.spec();
}

// Converts |etld_plus1| into an HTTPS SchemefulSite.
net::SchemefulSite ConvertEtldToSchemefulSite(const std::string etld_plus1) {
  return net::SchemefulSite(GURL(std::string(url::kHttpsScheme) +
                                 url::kStandardSchemeSeparator + etld_plus1 +
                                 "/"));
}

// Validates that the list of sites are aligned with the related website sets
// mapping.
void ValidateSitesWithRws(
    const base::Value::List& storage_and_cookie_list,
    base::flat_map<net::SchemefulSite, net::SchemefulSite>&
        related_website_sets) {
  for (const base::Value& site_group_value : storage_and_cookie_list) {
    const base::Value::Dict& site_group = site_group_value.GetDict();
    GroupingKey grouping_key = GroupingKey::Deserialize(
        CHECK_DEREF(site_group.FindString("groupingKey")));
    if (!grouping_key.GetEtldPlusOne().has_value()) {
      return;
    }
    std::string etld_plus1 = *grouping_key.GetEtldPlusOne();
    auto schemeful_site = ConvertEtldToSchemefulSite(etld_plus1);

    if (related_website_sets.count(schemeful_site)) {
      // Ensure that the `rwsOwner` is set correctly and aligned with
      // |related_website_sets| mapping of site group owners.
      std::string owner_etldplus1 =
          related_website_sets[schemeful_site].GetURL().host();
      ASSERT_EQ(owner_etldplus1, *site_group.FindString("rwsOwner"));
      if (owner_etldplus1 == "google.com") {
        ASSERT_EQ(2, *site_group.FindInt("rwsNumMembers"));
        ASSERT_EQ(false, *site_group.FindBool("rwsEnterpriseManaged"));
      } else if (owner_etldplus1 == "example.com") {
        ASSERT_EQ(1, *site_group.FindInt("rwsNumMembers"));
        ASSERT_EQ(true, *site_group.FindBool("rwsEnterpriseManaged"));
      }
    } else {
      // The site is not part of a RWS therefore doesn't have `rwsOwner` or
      // `rwsNumMembers` set. `FindString` and `FindInt` should return null.
      ASSERT_FALSE(site_group.FindString("rwsOwner"));
      ASSERT_FALSE(site_group.FindInt("rwsNumMembers"));
      ASSERT_FALSE(site_group.FindBool("rwsEnterpriseManaged"));
    }
  }
}

apps::AppPtr MakeApp(const std::string& app_id,
                     apps::AppType app_type,
                     const std::string& publisher_id,
                     apps::Readiness readiness,
                     apps::InstallReason install_reason) {
  apps::AppPtr app = std::make_unique<apps::App>(app_type, app_id);
  app->publisher_id = publisher_id;
  app->readiness = readiness;
  app->install_reason = install_reason;
  return app;
}

void RegisterWebApp(Profile* profile, apps::AppPtr app) {
  std::vector<apps::AppPtr> deltas;
  deltas.push_back(std::move(app));
  apps::AppServiceProxyFactory::GetForProfile(profile)->OnApps(
      std::move(deltas), apps::AppType::kWeb,
      /*should_notify_initialized=*/true);
}

std::unique_ptr<net::CanonicalCookie> CreateCookieKey(
    const GURL& url,
    const std::string& cookie_line,
    std::optional<net::CookiePartitionKey> cookie_partition_key =
        std::nullopt) {
  return net::CanonicalCookie::CreateForTesting(
      url, cookie_line, base::Time::Now(), std::nullopt /* server_time */,
      cookie_partition_key);
}

void RemoveModelEntries(
    std::vector<browsing_data_model_test_util::BrowsingDataEntry>&
        model_entries,
    const std::vector<browsing_data_model_test_util::BrowsingDataEntry>&
        entries_to_remove) {
  for (const auto& entry : entries_to_remove) {
    model_entries.erase(
        std::remove(model_entries.begin(), model_entries.end(), entry),
        model_entries.end());
  }
}

struct TestModel {
  const raw_ref<FakeBrowsingDataModel> browsing_data_model;
};

}  // namespace

namespace settings {

// Helper class for setting ContentSettings via different sources.
class ContentSettingSourceSetter {
 public:
  ContentSettingSourceSetter(TestingProfile* profile,
                             ContentSettingsType content_type)
      : prefs_(profile->GetTestingPrefService()), content_type_(content_type) {}
  ContentSettingSourceSetter(const ContentSettingSourceSetter&) = delete;
  ContentSettingSourceSetter& operator=(const ContentSettingSourceSetter&) =
      delete;

  void SetPolicyDefault(ContentSetting setting) {
    prefs_->SetManagedPref(GetPrefNameForDefaultPermissionSetting(),
                           std::make_unique<base::Value>(setting));
  }

  const char* GetPrefNameForDefaultPermissionSetting() {
    switch (content_type_) {
      case ContentSettingsType::NOTIFICATIONS:
        return prefs::kManagedDefaultNotificationsSetting;
      default:
        // Add support as needed.
        NOTREACHED_IN_MIGRATION();
        return "";
    }
  }

 private:
  raw_ptr<sync_preferences::TestingPrefServiceSyncable> prefs_;
  ContentSettingsType content_type_;
};

class SiteSettingsHandlerBaseTest : public testing::Test {
 public:
  SiteSettingsHandlerBaseTest() {
    // Fully initialize |profile_| in the constructor since some children
    // classes need it right away for SetUp().
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    EXPECT_TRUE(testing_profile_manager_->SetUp());
    profile_ = testing_profile_manager_->CreateTestingProfile(
        kTestUserEmail,
        {TestingProfile::TestingFactory{
            HistoryServiceFactory::GetInstance(),
            HistoryServiceFactory::GetDefaultFactory()}},
        /*is_main_profile=*/true);
    EXPECT_TRUE(profile_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
    SetUpUserManager(profile_.get());
#endif
  }

  void SetUp() override {
    browsing_topics::BrowsingTopicsServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            profile(),
            base::BindLambdaForTesting([this](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
              auto mock_browsing_topics_service = std::make_unique<
                  browsing_topics::MockBrowsingTopicsService>();
              mock_browsing_topics_service_ =
                  mock_browsing_topics_service.get();
              return mock_browsing_topics_service;
            }));

    mock_privacy_sandbox_service_ = static_cast<MockPrivacySandboxService*>(
        PrivacySandboxServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockPrivacySandboxService)));

    profile()->SetPermissionControllerDelegate(
        permissions::GetPermissionControllerDelegate(profile()));

    handler_ = std::make_unique<SiteSettingsHandler>(profile());
    handler()->set_web_ui(web_ui());
    handler()->AllowJavascript();
    // AllowJavascript() adds a callback to create leveldb_env::ChromiumEnv
    // which reads the FeatureList. Wait for the callback to be finished so that
    // we won't destruct |feature_list_| before the callback is executed.
    base::RunLoop().RunUntilIdle();
    web_ui()->ClearTrackedCalls();
  }

  void TearDown() override {
    if (profile_) {
      auto* partition = profile_->GetDefaultStoragePartition();
      if (partition)
        partition->WaitForDeletionTasksForTesting();
    }
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetUpUserManager(TestingProfile* profile) {
    // On ChromeOS a user account is needed in order to check whether the user
    // account is affiliated with the device owner for the purposes of applying
    // enterprise policy.
    constexpr char kTestUserGaiaId[] = "1111111111";
    auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
    auto* fake_user_manager_ptr = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    auto account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
    fake_user_manager_ptr->AddUserWithAffiliation(account_id,
                                                  /*is_affiliated=*/true);
    fake_user_manager_ptr->LoginUser(account_id);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  TestingProfile* profile() { return profile_.get(); }
  Profile* incognito_profile() { return incognito_profile_; }
  content::TestWebUI* web_ui() { return &web_ui_; }
  SiteSettingsHandler* handler() { return handler_.get(); }
  browsing_topics::MockBrowsingTopicsService* mock_browsing_topics_service() {
    return mock_browsing_topics_service_;
  }
  MockPrivacySandboxService* mock_privacy_sandbox_service() {
    return mock_privacy_sandbox_service_.get();
  }

  void ValidateBlockAutoplay(bool expected_value, bool expected_enabled) {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());

    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ("onBlockAutoplayStatusChanged", data.arg1()->GetString());

    const base::Value* event_data = data.arg2();
    ASSERT_TRUE(event_data->is_dict());

    std::optional<bool> enabled = event_data->GetDict().FindBool("enabled");
    ASSERT_TRUE(enabled.has_value());
    EXPECT_EQ(expected_enabled, *enabled);

    const base::Value::Dict* pref_data = event_data->GetDict().FindDict("pref");
    ASSERT_TRUE(pref_data);

    std::optional<bool> value = pref_data->FindBool("value");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(expected_value, *value);
  }

  void SetSoundContentSettingDefault(ContentSetting value) {
    HostContentSettingsMap* content_settings =
        HostContentSettingsMapFactory::GetForProfile(profile());
    content_settings->SetDefaultContentSetting(ContentSettingsType::SOUND,
                                               value);
  }

  void ValidateDefault(const ContentSetting expected_setting,
                       const std::string expected_source,
                       size_t expected_total_calls) {
    EXPECT_EQ(expected_total_calls, web_ui()->call_data().size());

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());

    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());

    ASSERT_TRUE(data.arg2()->is_bool());
    ASSERT_TRUE(data.arg2()->GetBool());

    const base::Value::Dict& default_value = data.arg3()->GetDict();
    const std::string* setting = default_value.FindString(kSetting);
    ASSERT_TRUE(setting);
    EXPECT_EQ(*setting,
              content_settings::ContentSettingToString(expected_setting));
    const std::string* source = default_value.FindString(kSource);
    if (source) {
      EXPECT_EQ(*source, expected_source);
    } else {
      EXPECT_TRUE(expected_source.empty());
    }
  }

  void ValidateOrigin(const std::string& expected_origin,
                      const std::string& expected_embedding,
                      const std::string& expected_display_name,
                      const ContentSetting expected_setting,
                      const site_settings::SiteSettingSource expected_source,
                      size_t expected_total_calls) {
    EXPECT_EQ(expected_total_calls, web_ui()->call_data().size());

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());

    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->is_bool());
    ASSERT_TRUE(data.arg2()->GetBool());

    ASSERT_TRUE(data.arg3()->is_list());
    EXPECT_EQ(1U, data.arg3()->GetList().size());

    const base::Value::Dict& exception = data.arg3()->GetList()[0].GetDict();

    const std::string* origin = exception.FindString(site_settings::kOrigin);
    ASSERT_TRUE(origin);
    ASSERT_EQ(expected_origin, *origin);

    const std::string* display_name =
        exception.FindString(site_settings::kDisplayName);
    ASSERT_TRUE(display_name);
    ASSERT_EQ(expected_display_name, *display_name);

    const std::string* embedding_origin =
        exception.FindString(site_settings::kEmbeddingOrigin);
    ASSERT_TRUE(embedding_origin);
    ASSERT_EQ(expected_embedding, *embedding_origin);

    const std::string* setting = exception.FindString(site_settings::kSetting);
    ASSERT_TRUE(setting);
    ASSERT_EQ(content_settings::ContentSettingToString(expected_setting),
              *setting);

    const std::string* source = exception.FindString(site_settings::kSource);
    ASSERT_TRUE(source);
    ASSERT_EQ(site_settings::SiteSettingSourceToString(expected_source),
              *source);
  }

  void SetContentSettingCustomScope(
      std::string primary_pattern,
      std::string secondary_pattern,
      ContentSettingsType content_setting_type,
      ContentSetting content_setting,
      size_t expected_total_calls = 1U,
      bool is_incognito = false,
      base::TimeDelta lifetime = base::TimeDelta(),
      bool is_auto_granted = false) {
    HostContentSettingsMap* map = HostContentSettingsMapFactory::GetForProfile(
        is_incognito ? incognito_profile() : profile());

    content_settings::ContentSettingConstraints constraints;
    constraints.set_lifetime(lifetime);
    if (is_auto_granted) {
      constraints.set_session_model(
          content_settings::mojom::SessionModel::NON_RESTORABLE_USER_SESSION);
    }

    map->SetContentSettingCustomScope(
        ContentSettingsPattern::FromString(primary_pattern),
        secondary_pattern.empty()
            ? ContentSettingsPattern::Wildcard()
            : ContentSettingsPattern::FromString(secondary_pattern),
        content_setting_type, content_setting, constraints);
    EXPECT_EQ(expected_total_calls, web_ui()->call_data().size());
    ASSERT_EQ("contentSettingSitePermissionChanged",
              web_ui()->call_data().back()->arg1()->GetString());
  }

  void ValidateStorageAccessList(size_t expected_total_calls,
                                 size_t expected_num_groups) {
    EXPECT_EQ(expected_total_calls, web_ui()->call_data().size());

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());

    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->is_bool());
    ASSERT_TRUE(data.arg2()->GetBool());

    ASSERT_TRUE(data.arg3()->is_list());
    EXPECT_EQ(expected_num_groups, data.arg3()->GetList().size());
  }

  void ValidateStorageAccessException(
      const std::string& expected_origin,
      const std::string& expected_display_name,
      const ContentSetting expected_setting,
      const std::vector<EmbeddingStorageAccessException>
          expected_embedding_exceptions,
      size_t index = 0) {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();

    const base::Value::Dict& exception =
        data.arg3()->GetList()[index].GetDict();

    const auto* origin = exception.FindString(site_settings::kOrigin);
    ASSERT_TRUE(origin);
    ASSERT_EQ(expected_origin, *origin);

    const auto* display_name =
        exception.FindString(site_settings::kDisplayName);
    ASSERT_TRUE(display_name);
    ASSERT_EQ(expected_display_name, *display_name);

    // Simple description and incognito should only be present for static
    // exceptions.
    const auto* description = exception.FindString(site_settings::kDescription);
    ASSERT_FALSE(description);
    std::optional<bool> incognito =
        exception.FindBool(site_settings::kIncognito);
    ASSERT_FALSE(incognito.has_value());

    const auto expected_close_description = l10n_util::GetPluralStringFUTF8(
        IDS_DEL_SITE_SETTINGS_COUNTER, expected_embedding_exceptions.size());
    const auto* close_description =
        exception.FindString(site_settings::kCloseDescription);
    ASSERT_TRUE(close_description);
    ASSERT_EQ(expected_close_description, *close_description);

    const auto expected_open_description = l10n_util::GetStringUTF8(
        (expected_setting == ContentSetting::CONTENT_SETTING_ALLOW)
            ? IDS_SETTINGS_STORAGE_ACCESS_ALLOWED_SITE_LABEL
            : IDS_SETTINGS_STORAGE_ACCESS_BLOCKED_SITE_LABEL);
    const auto* open_description =
        exception.FindString(site_settings::kOpenDescription);
    ASSERT_TRUE(open_description);
    ASSERT_EQ(expected_open_description, *open_description);

    const auto* setting = exception.FindString(site_settings::kSetting);
    ASSERT_TRUE(setting);
    ASSERT_EQ(content_settings::ContentSettingToString(expected_setting),
              *setting);

    const auto* exceptions_list =
        exception.FindList(site_settings::kExceptions);
    ASSERT_TRUE(exceptions_list);
    ASSERT_EQ(expected_embedding_exceptions.size(), exceptions_list->size());

    for (size_t i = 0; i < expected_embedding_exceptions.size(); i++) {
      ValidateStorageAccessEmbeddingException(expected_embedding_exceptions[i],
                                              (*exceptions_list)[i].GetDict());
    }
  }

  void ValidateStorageAccessEmbeddingException(
      EmbeddingStorageAccessException expected_embedding_exception,
      const base::Value::Dict& embedding_exception) {
    const auto* embedding_origin =
        embedding_exception.FindString(site_settings::kEmbeddingOrigin);
    ASSERT_TRUE(embedding_origin);
    ASSERT_EQ(expected_embedding_exception.embedding_origin, *embedding_origin);

    const auto* embedding_display_name =
        embedding_exception.FindString(site_settings::kEmbeddingDisplayName);
    ASSERT_TRUE(embedding_display_name);
    ASSERT_EQ(expected_embedding_exception.embedding_display_name,
              *embedding_display_name);

    std::optional<bool> incognito =
        embedding_exception.FindBool(site_settings::kIncognito);
    ASSERT_TRUE(incognito.has_value());
    EXPECT_EQ(expected_embedding_exception.incognito, *incognito);

    const std::string* description =
        embedding_exception.FindString(site_settings::kDescription);

    if (expected_embedding_exception.lifetime_in_days == 0 &&
        !expected_embedding_exception.embargoed) {
      ASSERT_FALSE(description);
      return;
    }

    std::string expected_description =
        expected_embedding_exception.embargoed
            ? l10n_util::GetStringUTF8(
                  IDS_PAGE_INFO_PERMISSION_AUTOMATICALLY_BLOCKED)
            : l10n_util::GetPluralStringFUTF8(
                  IDS_SETTINGS_EXPIRES_AFTER_TIME_LABEL,
                  expected_embedding_exception.lifetime_in_days);
    ASSERT_TRUE(description);
    ASSERT_EQ(expected_description, *description);
  }

  void ValidateStaticStorageAccessException(
      const std::string& expected_origin,
      const std::string& expected_display_name,
      const ContentSetting expected_setting,
      bool expected_incognito,
      size_t index = 0) {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();

    const base::Value::Dict& exception =
        data.arg3()->GetList()[index].GetDict();

    const auto* origin = exception.FindString(site_settings::kOrigin);
    ASSERT_TRUE(origin);
    ASSERT_EQ(expected_origin, *origin);

    const auto* display_name =
        exception.FindString(site_settings::kDisplayName);
    ASSERT_TRUE(display_name);
    ASSERT_EQ(expected_display_name, *display_name);

    // Close and open description should only be present for non-static
    // exceptions.
    const auto* close_description =
        exception.FindString(site_settings::kCloseDescription);
    ASSERT_FALSE(close_description);
    const auto* open_description =
        exception.FindString(site_settings::kOpenDescription);
    ASSERT_FALSE(open_description);

    std::string expected_description = l10n_util::GetStringUTF8(
        IDS_PAGE_INFO_PERMISSION_AUTOMATICALLY_BLOCKED);
    const auto* description = exception.FindString(site_settings::kDescription);
    ASSERT_TRUE(description);
    ASSERT_EQ(expected_description, *description);

    const auto* setting = exception.FindString(site_settings::kSetting);
    ASSERT_TRUE(setting);
    ASSERT_EQ(content_settings::ContentSettingToString(expected_setting),
              *setting);

    std::optional<bool> incognito =
        exception.FindBool(site_settings::kIncognito);
    ASSERT_TRUE(incognito.has_value());
    EXPECT_EQ(expected_incognito, *incognito);

    const auto* exceptions_list =
        exception.FindList(site_settings::kExceptions);
    ASSERT_TRUE(exceptions_list);
    ASSERT_EQ(0U, exceptions_list->size());
  }

  void ValidateNoOrigin(size_t expected_total_calls) {
    EXPECT_EQ(expected_total_calls, web_ui()->call_data().size());

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());

    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());

    ASSERT_TRUE(data.arg2()->is_bool());
    ASSERT_TRUE(data.arg2()->GetBool());

    const base::Value& exceptions = *data.arg3();
    ASSERT_TRUE(exceptions.is_list());
    EXPECT_TRUE(exceptions.GetList().empty());
  }

  void ValidatePattern(bool expected_validity,
                       size_t expected_total_calls,
                       std::string expected_reason) {
    EXPECT_EQ(expected_total_calls, web_ui()->call_data().size());

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());

    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());

    ASSERT_TRUE(data.arg2()->is_bool());
    ASSERT_TRUE(data.arg2()->GetBool());

    const base::Value* result = data.arg3();
    ASSERT_TRUE(result->is_dict());

    std::optional<bool> valid = result->GetDict().FindBool("isValid");
    ASSERT_TRUE(valid.has_value());
    EXPECT_EQ(expected_validity, *valid);

    const std::string* reason = result->GetDict().FindString("reason");
    ASSERT_TRUE(reason);
    EXPECT_EQ(expected_reason, *reason);
  }

  void ValidateIncognitoExists(bool expected_incognito,
                               size_t expected_total_calls) {
    EXPECT_EQ(expected_total_calls, web_ui()->call_data().size());

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());

    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ("onIncognitoStatusChanged", data.arg1()->GetString());

    ASSERT_TRUE(data.arg2()->is_bool());
    EXPECT_EQ(expected_incognito, data.arg2()->GetBool());
  }

  struct ZoomLevel {
    std::string host_or_spec;
    std::string display_name;
    std::string zoom;
  };

  void ValidateZoom(const std::vector<ZoomLevel>& zoom_levels,
                    size_t expected_total_calls) {
    EXPECT_EQ(expected_total_calls, web_ui()->call_data().size());

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());

    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ("onZoomLevelsChanged", data.arg1()->GetString());

    ASSERT_TRUE(data.arg2()->is_list());
    const base::Value::List& exceptions = data.arg2()->GetList();
    ASSERT_EQ(zoom_levels.size(), exceptions.size());
    for (size_t i = 0; i < zoom_levels.size(); i++) {
      const ZoomLevel& zoom_level = zoom_levels[i];
      const base::Value::Dict& exception = exceptions[i].GetDict();

      const std::string* host_or_spec = exception.FindString("hostOrSpec");
      ASSERT_TRUE(host_or_spec);
      ASSERT_EQ(zoom_level.host_or_spec, *host_or_spec);

      const std::string* display_name = exception.FindString("displayName");
      ASSERT_TRUE(display_name);
      ASSERT_EQ(zoom_level.display_name, *display_name);

      const std::string* zoom = exception.FindString("zoom");
      ASSERT_TRUE(zoom);
      ASSERT_EQ(zoom_level.zoom, *zoom);
    }
  }

  void ValidateCookieSettingUpdate(const std::string expected_string,
                                   const int expected_call_index) {
    const content::TestWebUI::CallData& data =
        *web_ui()->call_data()[expected_call_index];

    ASSERT_EQ("cr.webUIListenerCallback", data.function_name());
    ASSERT_EQ("cookieSettingDescriptionChanged", data.arg1()->GetString());
    ASSERT_EQ(expected_string, data.arg2()->GetString());
  }

  void ValidateUsageInfo(const std::string& expected_usage_origin,
                         const std::string& expected_usage_string,
                         const std::string& expected_cookie_string,
                         const std::string& expected_rws_member_count_string,
                         const bool expected_rws_policy) {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());

    ASSERT_TRUE(data.arg_nth(0)->is_string());
    EXPECT_EQ("usage-total-changed", data.arg_nth(0)->GetString());

    ASSERT_TRUE(data.arg_nth(1)->is_string());
    EXPECT_EQ(expected_usage_origin, data.arg_nth(1)->GetString());

    ASSERT_TRUE(data.arg_nth(2)->is_string());
    EXPECT_EQ(expected_usage_string, data.arg_nth(2)->GetString());

    ASSERT_TRUE(data.arg_nth(3)->is_string());
    EXPECT_EQ(expected_cookie_string, data.arg_nth(3)->GetString());

    ASSERT_TRUE(data.arg_nth(4)->is_string());
    EXPECT_EQ(expected_rws_member_count_string, data.arg_nth(4)->GetString());

    ASSERT_TRUE(data.arg_nth(5)->is_bool());
    EXPECT_EQ(expected_rws_policy, data.arg_nth(5)->GetBool());
  }

  void CreateIncognitoProfile() {
    incognito_profile_ = profile_->GetOffTheRecordProfile(
        Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  }

  virtual void DestroyIncognitoProfile() {
    if (incognito_profile_) {
      profile_->DestroyOffTheRecordProfile(incognito_profile_);
      incognito_profile_ = nullptr;
    }
  }

  void SetupModel(base::OnceCallback<void(const TestModel& model)> setup) {
    auto fake_browsing_data_model = std::make_unique<FakeBrowsingDataModel>(
        ChromeBrowsingDataModelDelegate::CreateForProfile(profile()));

    std::move(setup).Run({ToRawRef(*fake_browsing_data_model)});

    handler()->SetModelForTesting(std::move(fake_browsing_data_model));
  }

  void SetupModel() {
    SetupModel(base::BindLambdaForTesting([this](const TestModel& model) {
      std::vector<browsing_data_model_test_util::BrowsingDataEntry>
          browsing_data_model_entries = {
              kGoogleUnpartitionedEntry,
              kExampleUnpartitionedEntry,
              kGoogleOnExampleEntry,
              kExampleOnGoogleSecureEntry,
              kExampleOnGoogleInsecureEntry,
              kExampleLocalStorage,
              kHttpExampleCookie,
              kHttpsWwwExampleCookie,
              kPartitionedHttpsWwwExampleOnGoogleAuCookie,
              kPartitionedHttpsWwwExampleOnGoogleCookie,
              kHttpAbcExampleCookie,
              kHttpGoogleCookieA,
              kHttpGoogleCookieB,
              kHttpGoogleAuCookie,
              kPartitionedHttpsGoogleAu1PCookie,
              kPartitionedHttpsWwwAnotherExampleOnGoogleAuCookie,
              kUngroupedHttpCookie,
          };

      for (const auto& entry : browsing_data_model_entries) {
        model.browsing_data_model->AddBrowsingData(
            entry.data_key, *(entry.data_details.storage_types.begin()),
            entry.data_details.storage_size, entry.data_details.cookie_count);
      }
    }));
  }

  void SetupModelWithIsolatedWebAppData(
      const std::vector<std::pair<std::string, int64_t>>& iwa_url_and_usage) {
    SetupModel(base::BindLambdaForTesting([&](const TestModel& model) {
      for (const auto& url_and_usage : iwa_url_and_usage) {
        model.browsing_data_model->AddBrowsingData(
            url::Origin::Create(GURL(url_and_usage.first)),
            static_cast<BrowsingDataModel::StorageType>(
                ChromeBrowsingDataModelDelegate::StorageType::kIsolatedWebApp),
            url_and_usage.second);
      }
    }));
  }

  base::Value::List GetOnStorageFetchedSentList() {
    handler()->ClearAllSitesMapForTesting();

    auto get_all_sites_args = base::Value::List().Append(kCallbackId);
    handler()->HandleGetAllSites(get_all_sites_args);
    handler()->ServicePendingRequests();

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    return data.arg2()->GetList().Clone();
  }

  void SetupDefaultRelatedWebsiteSets(MockPrivacySandboxService* mock_service) {
    EXPECT_CALL(*mock_service, GetFirstPartySetOwner(_))
        .WillRepeatedly(
            [&](const GURL& url) -> std::optional<net::SchemefulSite> {
              auto related_website_sets = GetTestRelatedWebsiteSets();
              if (related_website_sets.count(net::SchemefulSite(url))) {
                return related_website_sets[net::SchemefulSite(url)];
              }

              return std::nullopt;
            });
  }

  base::flat_map<net::SchemefulSite, net::SchemefulSite>
  GetTestRelatedWebsiteSets() {
    base::flat_map<net::SchemefulSite, net::SchemefulSite>
        related_website_sets = {{ConvertEtldToSchemefulSite("google.com"),
                                 ConvertEtldToSchemefulSite("google.com")},
                                {ConvertEtldToSchemefulSite("google.com.au"),
                                 ConvertEtldToSchemefulSite("google.com")},
                                {ConvertEtldToSchemefulSite("example.com"),
                                 ConvertEtldToSchemefulSite("example.com")},
                                {ConvertEtldToSchemefulSite("unrelated.com"),
                                 ConvertEtldToSchemefulSite("unrelated.com")}};

    return related_website_sets;
  }

  scoped_refptr<const extensions::Extension> LoadExtension(
      const std::string& extension_name) {
    auto extension = extensions::ExtensionBuilder()
                         .SetManifest(base::Value::Dict()
                                          .Set("name", kExtensionName)
                                          .Set("version", "1.0.0")
                                          .Set("manifest_version", 3))
                         .Build();

    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile()));
    extensions::ExtensionService* extension_service =
        extension_system->CreateExtensionService(
            base::CommandLine::ForCurrentProcess(), base::FilePath(),
            /*autoupdate_enabled=*/false);
    extension_service->AddExtension(extension.get());
    return extension;
  }

  void UnloadExtension(std::string extension_id) {
    auto* extension_service =
        extensions::ExtensionSystem::Get(profile())->extension_service();
    ASSERT_TRUE(extension_service);
    extension_service->UnloadExtension(
        extension_id, extensions::UnloadedExtensionReason::DISABLE);
  }

  void ValidateCallbacksForNotificationPermission(int index) {
    // When a notification permission is set or reset, there are two consecutive
    // callbacks. The first one is to notify content setting observers, and
    // the second one is to update safety check notification permission review.
    ASSERT_EQ("contentSettingSitePermissionChanged",
              web_ui()->call_data()[index]->arg1()->GetString());
    ASSERT_EQ("notification-permission-review-list-maybe-changed",
              web_ui()->call_data()[index + 1]->arg1()->GetString());
  }

  // Content setting group name for the relevant ContentSettingsType.
  const std::string_view kNotifications =
      site_settings::ContentSettingsTypeToGroupName(
          ContentSettingsType::NOTIFICATIONS);
  const std::string_view kCookies =
      site_settings::ContentSettingsTypeToGroupName(
          ContentSettingsType::COOKIES);
  const std::string_view kTrackingProtection =
      site_settings::ContentSettingsTypeToGroupName(
          ContentSettingsType::TRACKING_PROTECTION);
  const std::string_view kStorageAccess =
      site_settings::ContentSettingsTypeToGroupName(
          ContentSettingsType::STORAGE_ACCESS);

  const ContentSettingsType kPermissionNotifications =
      ContentSettingsType::NOTIFICATIONS;
  const ContentSettingsType kPermissionStorageAccess =
      ContentSettingsType::STORAGE_ACCESS;

  // The number of listeners that are expected to fire when notification content
  // setting is changed.
  const size_t kNumberNotificationsContentSettingListeners = 2;

  // The number of listeners that are expected to fire when cookies content
  // setting is changed.
  const size_t kNumberCookiesContentSettingListeners = 1;

  // Browsing data model constants. Here, instead of in the anon namespace, to
  // avoid static GURL creation.
  const browsing_data_model_test_util::BrowsingDataEntry
      kGoogleUnpartitionedEntry{
          "www.google.com",
          url::Origin::Create(GURL("https://www.google.com")),
          {{BrowsingDataModel::StorageType::kTrustTokens}, 50000000000, 0}};
  const browsing_data_model_test_util::BrowsingDataEntry
      kExampleUnpartitionedEntry{
          "www.example.com",
          url::Origin::Create(GURL("https://www.example.com")),
          {{BrowsingDataModel::StorageType::kTrustTokens}, 100, 0}};
  const browsing_data_model_test_util::BrowsingDataEntry kGoogleOnExampleEntry{
      "google.com",
      blink::StorageKey::Create(
          url::Origin::Create(GURL("https://google.com/")),
          net::SchemefulSite(
              url::Origin::Create(GURL("https://www.example.com/"))),
          blink::mojom::AncestorChainBit::kCrossSite,
          /*third_party_partitioning_allowed=*/true),
      {{BrowsingDataModel::StorageType::kQuotaStorage}, 100, 0}};
  const browsing_data_model_test_util::BrowsingDataEntry
      kExampleOnGoogleSecureEntry{
          "www.example.com",
          blink::StorageKey::Create(
              url::Origin::Create(GURL("https://www.example.com/")),
              net::SchemefulSite(
                  url::Origin::Create(GURL("https://www.google.com/"))),
              blink::mojom::AncestorChainBit::kCrossSite,
              /*third_party_partitioning_allowed=*/true),
          {{BrowsingDataModel::StorageType::kQuotaStorage}, 100, 0}};
  const browsing_data_model_test_util::BrowsingDataEntry
      kExampleOnGoogleInsecureEntry{
          "www.example.com",
          blink::StorageKey::Create(
              url::Origin::Create(GURL("https://www.example.com/")),
              net::SchemefulSite(
                  url::Origin::Create(GURL("http://www.google.com/"))),
              blink::mojom::AncestorChainBit::kCrossSite,
              /*third_party_partitioning_allowed=*/true),
          {{BrowsingDataModel::StorageType::kQuotaStorage}, 100, 0}};

  const browsing_data_model_test_util::BrowsingDataEntry kHttpExampleCookie{
      "example.com",
      *(CreateCookieKey(GURL("http://example.com"), "A=1")),
      {{BrowsingDataModel::StorageType::kCookie},
       /*storage_size=*/0,
       /*cookie_count=*/1}};

  const browsing_data_model_test_util::BrowsingDataEntry kExampleLocalStorage{
      "www.example.com",
      blink::StorageKey::CreateFromStringForTesting("https://www.example.com/"),
      {{BrowsingDataModel::StorageType::kLocalStorage},
       /*storage_size=*/2,
       /*cookie_count=*/0}};

  const browsing_data_model_test_util::BrowsingDataEntry kHttpsWwwExampleCookie{
      "www.example.com",
      *(CreateCookieKey(GURL("https://www.example.com"), "B=1")),
      {{BrowsingDataModel::StorageType::kCookie},
       /*storage_size=*/0,
       /*cookie_count=*/1}};

  const browsing_data_model_test_util::BrowsingDataEntry
      kPartitionedHttpsWwwExampleOnGoogleAuCookie{
          "www.example.com",
          *(CreateCookieKey(GURL("https://www.example.com"),
                            "__Host-A=1; Path=/; Partitioned; Secure;",
                            net::CookiePartitionKey::FromURLForTesting(
                                GURL("https://google.com.au")))),
          {{BrowsingDataModel::StorageType::kCookie},
           /*storage_size=*/0,
           /*cookie_count=*/1}};

  const browsing_data_model_test_util::BrowsingDataEntry
      kPartitionedHttpsWwwExampleOnGoogleCookie{
          "www.example.com",
          *(CreateCookieKey(GURL("https://www.example.com"),
                            "__Host-A=1; Path=/; Partitioned; Secure;",
                            net::CookiePartitionKey::FromURLForTesting(
                                GURL("https://google.com")))),
          {{BrowsingDataModel::StorageType::kCookie},
           /*storage_size=*/0,
           /*cookie_count=*/1}};

  const browsing_data_model_test_util::BrowsingDataEntry kHttpAbcExampleCookie{
      "abc.example.com",
      *(CreateCookieKey(GURL("http://abc.example.com"), "C=1")),
      {{BrowsingDataModel::StorageType::kCookie},
       /*storage_size=*/0,
       /*cookie_count=*/1}};

  const browsing_data_model_test_util::BrowsingDataEntry kHttpGoogleCookieA{
      "google.com",
      *(CreateCookieKey(GURL("http://google.com"), "A=1")),
      {{BrowsingDataModel::StorageType::kCookie},
       /*storage_size=*/0,
       /*cookie_count=*/1}};

  const browsing_data_model_test_util::BrowsingDataEntry kHttpGoogleCookieB{
      "google.com",
      *(CreateCookieKey(GURL("http://google.com"), "B=1")),
      {{BrowsingDataModel::StorageType::kCookie},
       /*storage_size=*/0,
       /*cookie_count=*/1}};

  const browsing_data_model_test_util::BrowsingDataEntry kHttpGoogleAuCookie{
      "google.com.au",
      *(CreateCookieKey(GURL("http://google.com.au"), "A=1")),
      {{BrowsingDataModel::StorageType::kCookie},
       /*storage_size=*/0,
       /*cookie_count=*/1}};

  const browsing_data_model_test_util::BrowsingDataEntry
      kPartitionedHttpsGoogleAu1PCookie{
          "google.com.au",
          *(CreateCookieKey(GURL("https://google.com.au"),
                            "__Host-A=1; Path=/; Partitioned; Secure;",
                            net::CookiePartitionKey::FromURLForTesting(
                                GURL("https://google.com.au")))),
          {{BrowsingDataModel::StorageType::kCookie},
           /*storage_size=*/0,
           /*cookie_count=*/1}};

  const browsing_data_model_test_util::BrowsingDataEntry
      kPartitionedHttpsWwwAnotherExampleOnGoogleAuCookie{
          "www.another-example.com",
          *(CreateCookieKey(GURL("https://www.another-example.com"),
                            "__Host-A=1; Path=/; Partitioned; Secure;",
                            net::CookiePartitionKey::FromURLForTesting(
                                GURL("https://google.com.au")))),
          {{BrowsingDataModel::StorageType::kCookie},
           /*storage_size=*/0,
           /*cookie_count=*/1}};

  const browsing_data_model_test_util::BrowsingDataEntry kUngroupedHttpCookie{
      "ungrouped.com",
      *(CreateCookieKey(GURL("http://ungrouped.com"), "A=1")),
      {{BrowsingDataModel::StorageType::kCookie},
       /*storage_size=*/0,
       /*cookie_count=*/1}};

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  raw_ptr<Profile, DanglingUntriaged> incognito_profile_ = nullptr;
  content::TestWebUI web_ui_;
  std::unique_ptr<SiteSettingsHandler> handler_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
#endif
  raw_ptr<browsing_topics::MockBrowsingTopicsService>
      mock_browsing_topics_service_;
  raw_ptr<MockPrivacySandboxService> mock_privacy_sandbox_service_;
};

class SiteSettingsHandlerSchemeTest : public SiteSettingsHandlerBaseTest,
                                      public testing::WithParamInterface<bool> {
 protected:
  bool IsHttps() { return GetParam(); }
};

// True if testing for handle clear unpartitioned usage with
// HTTPS scheme URL. When set to true, the tests use HTTPS scheme as origin.
// When set to false, the tests use HTTP scheme as origin.
INSTANTIATE_TEST_SUITE_P(All, SiteSettingsHandlerSchemeTest, testing::Bool());

TEST_P(SiteSettingsHandlerSchemeTest, StorageAccessExceptions_Description_All) {
  const std::string kOrigin("google.com");
  const std::string kEmbeddingOrigin("example.com");

  const ContentSetting content_setting =
      IsHttps() ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;

  SetContentSettingCustomScope(kOrigin, kEmbeddingOrigin,
                               kPermissionStorageAccess, content_setting);

  base::Value::List get_exception_list_args;
  get_exception_list_args.Append(kCallbackId);
  get_exception_list_args.Append(
      content_settings::ContentSettingToString(content_setting));
  handler()->HandleGetStorageAccessExceptionList(get_exception_list_args);

  // Verify that the grouped exception is correct.
  ValidateStorageAccessList(/*expected_total_calls=*/2U,
                            /*expected_num_groups=*/1U);

  ValidateStorageAccessException(
      kOrigin, kOrigin, content_setting,
      {{kEmbeddingOrigin, kEmbeddingOrigin, /*incognito=*/false}});
}

TEST_P(SiteSettingsHandlerSchemeTest, HandleClearUnpartitionedUsage) {
  SetupModel();
  std::vector<browsing_data_model_test_util::BrowsingDataEntry>
      expected_browsing_data_model_entries = {
          kGoogleUnpartitionedEntry,
          kExampleUnpartitionedEntry,
          kGoogleOnExampleEntry,
          kExampleOnGoogleSecureEntry,
          kExampleOnGoogleInsecureEntry,
          kExampleLocalStorage,
          kHttpExampleCookie,
          kHttpsWwwExampleCookie,
          kPartitionedHttpsWwwExampleOnGoogleAuCookie,
          kPartitionedHttpsWwwExampleOnGoogleCookie,
          kHttpAbcExampleCookie,
          kHttpGoogleCookieA,
          kHttpGoogleCookieB,
          kHttpGoogleAuCookie,
          kPartitionedHttpsGoogleAu1PCookie,
          kPartitionedHttpsWwwAnotherExampleOnGoogleAuCookie,
          kUngroupedHttpCookie,
      };
  browsing_data_model_test_util::ValidateBrowsingDataEntries(
      handler()->GetBrowsingDataModelForTesting(),
      expected_browsing_data_model_entries);

  base::Value::List args;
  args.Append(IsHttps() ? "https://www.example.com/"
                        : "http://www.example.com/");
  handler()->HandleClearUnpartitionedUsage(args);

  // Confirm that only the unpartitioned items for example.com have been
  // cleared.
  RemoveModelEntries(expected_browsing_data_model_entries,
                     {
                         kExampleUnpartitionedEntry,
                         kExampleLocalStorage,
                         kHttpsWwwExampleCookie,
                     });
  browsing_data_model_test_util::ValidateBrowsingDataEntries(
      handler()->GetBrowsingDataModelForTesting(),
      expected_browsing_data_model_entries);

  args = base::Value::List();
  args.Append("https://google.com.au/");
  handler()->HandleClearUnpartitionedUsage(args);

  // First-party partitioned storage should be cleared.
  RemoveModelEntries(expected_browsing_data_model_entries,
                     {
                         kHttpGoogleAuCookie,
                         kPartitionedHttpsGoogleAu1PCookie,
                     });
  browsing_data_model_test_util::ValidateBrowsingDataEntries(
      handler()->GetBrowsingDataModelForTesting(),
      expected_browsing_data_model_entries);

  args = base::Value::List();
  args.Append("https://www.google.com/");
  handler()->HandleClearUnpartitionedUsage(args);

  // The unpartitioned Google and Example entries should be removed.
  RemoveModelEntries(expected_browsing_data_model_entries,
                     {
                         kGoogleUnpartitionedEntry,
                     });
  browsing_data_model_test_util::ValidateBrowsingDataEntries(
      handler()->GetBrowsingDataModelForTesting(),
      expected_browsing_data_model_entries);
}

class SiteSettingsHandlerTest : public SiteSettingsHandlerBaseTest {};

TEST_F(SiteSettingsHandlerTest, GetAndSetDefault) {
  // Test the JS -> C++ -> JS callback path for getting and setting defaults.
  base::Value::List get_args;
  get_args.Append(kCallbackId);
  get_args.Append(kNotifications);
  handler()->HandleGetDefaultValueForContentType(get_args);
  ValidateDefault(CONTENT_SETTING_ASK, "", 1U);

  // Set the default to 'Blocked'.
  base::Value::List set_args;
  set_args.Append(kNotifications);
  set_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  handler()->HandleSetDefaultValueForContentType(set_args);

  EXPECT_EQ(2U, web_ui()->call_data().size());

  // Verify that the default has been set to 'Blocked'.
  handler()->HandleGetDefaultValueForContentType(get_args);
  ValidateDefault(CONTENT_SETTING_BLOCK, "", 3U);
}

TEST_F(SiteSettingsHandlerTest, GetEnforcedDefault) {
  ContentSettingSourceSetter source_setter(profile(),
                                           ContentSettingsType::NOTIFICATIONS);
  source_setter.SetPolicyDefault(CONTENT_SETTING_ALLOW);

  base::Value::List get_args;
  get_args.Append(kCallbackId);
  get_args.Append(kNotifications);
  handler()->HandleGetDefaultValueForContentType(get_args);
  ValidateDefault(CONTENT_SETTING_ALLOW, "policy", 1U);
}

// Flaky on CrOS and Linux. https://crbug.com/930481
TEST_F(SiteSettingsHandlerTest, GetAllSites) {
  SetupModel();

  base::Value::List get_all_sites_args;
  get_all_sites_args.Append(kCallbackId);

  // Test all sites is empty when there are no preferences.
  handler()->HandleGetAllSites(get_all_sites_args);
  EXPECT_EQ(1U, web_ui()->call_data().size());

  {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->GetBool());

    const base::Value::List& site_groups = data.arg3()->GetList();
    EXPECT_TRUE(site_groups.empty());
  }

  // Add a couple of exceptions and check they appear in all sites.
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  const GURL url1("http://example.com");
  const GURL url2("https://other.example.com");
  map->SetContentSettingDefaultScope(
      url1, url1, ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_BLOCK);
  map->SetContentSettingDefaultScope(
      url2, url2, ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  handler()->HandleGetAllSites(get_all_sites_args);

  {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->GetBool());

    const base::Value::List& site_groups = data.arg3()->GetList();
    EXPECT_EQ(1UL, site_groups.size());
    for (const base::Value& site_group_val : site_groups) {
      const base::Value::Dict& site_group = site_group_val.GetDict();
      const base::Value::List& origin_list =
          CHECK_DEREF(site_group.FindList("origins"));
      EXPECT_THAT(CHECK_DEREF(site_group.FindString("groupingKey")),
                  IsEtldPlus1("example.com"));
      EXPECT_EQ(2UL, origin_list.size());
      const base::Value::Dict& first_origin = origin_list[0].GetDict();
      const base::Value::Dict& second_origin = origin_list[1].GetDict();
      EXPECT_EQ(url1.spec(), CHECK_DEREF(first_origin.FindString("origin")));
      EXPECT_EQ(0, first_origin.FindDouble("engagement"));
      EXPECT_EQ(url2.spec(), CHECK_DEREF(second_origin.FindString("origin")));
      EXPECT_EQ(0, second_origin.FindDouble("engagement"));
    }
  }

  // Add an additional exception belonging to a different eTLD+1.
  const GURL url3("https://example2.net");
  map->SetContentSettingDefaultScope(
      url3, url3, ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_BLOCK);
  handler()->HandleGetAllSites(get_all_sites_args);

  {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());

    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->GetBool());

    const base::Value::List& site_groups = data.arg3()->GetList();
    EXPECT_EQ(2UL, site_groups.size());
    for (const base::Value& site_group_val : site_groups) {
      const base::Value::Dict& site_group = site_group_val.GetDict();
      const std::string& grouping_key_string =
          CHECK_DEREF(site_group.FindString("groupingKey"));
      auto grouping_key = GroupingKey::Deserialize(grouping_key_string);
      const base::Value::List& origin_list =
          CHECK_DEREF(site_group.FindList("origins"));
      if (grouping_key.GetEtldPlusOne() == "example2.net") {
        EXPECT_EQ(1UL, origin_list.size());
        const base::Value::Dict& first_origin = origin_list[0].GetDict();
        EXPECT_EQ(url3.spec(), CHECK_DEREF(first_origin.FindString("origin")));
      } else {
        EXPECT_THAT(grouping_key_string, IsEtldPlus1("example.com"));
      }
    }
  }

  // Test embargoed settings also appear.
  permissions::PermissionDecisionAutoBlocker* auto_blocker =
      PermissionDecisionAutoBlockerFactory::GetForProfile(profile());
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  auto_blocker->SetClockForTesting(&clock);
  const GURL url4("https://example2.co.uk");
  for (int i = 0; i < 3; ++i) {
    auto_blocker->RecordDismissAndEmbargo(
        url4, ContentSettingsType::NOTIFICATIONS, false);
  }
  EXPECT_EQ(
      PermissionStatus::DENIED,
      auto_blocker->GetEmbargoResult(url4, ContentSettingsType::NOTIFICATIONS)
          ->status);
  handler()->HandleGetAllSites(get_all_sites_args);

  {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->GetBool());

    EXPECT_EQ(3UL, data.arg3()->GetList().size());
  }

  // Check |url4| disappears from the list when its embargo expires.
  clock.Advance(base::Days(8));
  handler()->HandleGetAllSites(get_all_sites_args);

  {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->GetBool());

    const base::Value::List& site_groups = data.arg3()->GetList();
    EXPECT_EQ(2UL, site_groups.size());
    EXPECT_THAT(CHECK_DEREF(site_groups[0].GetDict().FindString("groupingKey")),
                IsEtldPlus1("example.com"));
    EXPECT_THAT(CHECK_DEREF(site_groups[1].GetDict().FindString("groupingKey")),
                IsEtldPlus1("example2.net"));
  }

  // Add an expired embargo setting to an existing eTLD+1 group and make sure it
  // still appears.
  for (int i = 0; i < 3; ++i) {
    auto_blocker->RecordDismissAndEmbargo(
        url3, ContentSettingsType::NOTIFICATIONS, false);
  }
  EXPECT_EQ(
      PermissionStatus::DENIED,
      auto_blocker->GetEmbargoResult(url3, ContentSettingsType::NOTIFICATIONS)
          ->status);
  clock.Advance(base::Days(8));
  EXPECT_FALSE(
      auto_blocker->GetEmbargoResult(url3, ContentSettingsType::NOTIFICATIONS)
          .has_value());

  handler()->HandleGetAllSites(get_all_sites_args);
  {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->GetBool());

    const base::Value::List& site_groups = data.arg3()->GetList();
    EXPECT_EQ(2UL, site_groups.size());
    EXPECT_THAT(CHECK_DEREF(site_groups[0].GetDict().FindString("groupingKey")),
                IsEtldPlus1("example.com"));
    EXPECT_THAT(CHECK_DEREF(site_groups[1].GetDict().FindString("groupingKey")),
                IsEtldPlus1("example2.net"));
  }

  // Add an expired embargo to a new eTLD+1 and make sure it doesn't appear.
  const GURL url5("http://test.example5.com");
  for (int i = 0; i < 3; ++i) {
    auto_blocker->RecordDismissAndEmbargo(
        url5, ContentSettingsType::NOTIFICATIONS, false);
  }
  EXPECT_EQ(
      PermissionStatus::DENIED,
      auto_blocker->GetEmbargoResult(url5, ContentSettingsType::NOTIFICATIONS)
          ->status);
  clock.Advance(base::Days(8));
  EXPECT_FALSE(
      auto_blocker->GetEmbargoResult(url5, ContentSettingsType::NOTIFICATIONS)
          .has_value());

  handler()->HandleGetAllSites(get_all_sites_args);
  {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->GetBool());

    const base::Value::List& site_groups = data.arg3()->GetList();
    EXPECT_EQ(2UL, site_groups.size());
    EXPECT_THAT(CHECK_DEREF(site_groups[0].GetDict().FindString("groupingKey")),
                IsEtldPlus1("example.com"));
    EXPECT_THAT(CHECK_DEREF(site_groups[1].GetDict().FindString("groupingKey")),
                IsEtldPlus1("example2.net"));
  }

  // Same extension url from different content setting types shows only one
  // extension site group.
  auto extension = LoadExtension(kExtensionName);
  map->SetContentSettingDefaultScope(extension->url(), extension->url(),
                                     ContentSettingsType::NOTIFICATIONS,
                                     CONTENT_SETTING_BLOCK);
  map->SetContentSettingDefaultScope(extension->url(), extension->url(),
                                     ContentSettingsType::GEOLOCATION,
                                     CONTENT_SETTING_BLOCK);
  handler()->HandleGetAllSites(get_all_sites_args);
  {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->GetBool());

    const base::Value::List& site_groups = data.arg3()->GetList();
    EXPECT_EQ(3UL, site_groups.size());
    // Extension groupingKey will be its origin with the pattern
    // "chrome-extension://<extension_id>" so it is before other site groups in
    // the list.
    EXPECT_THAT(CHECK_DEREF(site_groups[0].GetDict().FindString("groupingKey")),
                IsOrigin(extension->url()));
    EXPECT_EQ(nullptr, site_groups[0].GetDict().FindString("etldPlus1"));
    EXPECT_THAT(CHECK_DEREF(site_groups[1].GetDict().FindString("groupingKey")),
                IsEtldPlus1("example.com"));
    EXPECT_EQ("example.com",
              CHECK_DEREF(site_groups[1].GetDict().FindString("etldPlus1")));
    EXPECT_THAT(CHECK_DEREF(site_groups[2].GetDict().FindString("groupingKey")),
                IsEtldPlus1("example2.net"));
    EXPECT_EQ("example2.net",
              CHECK_DEREF(site_groups[2].GetDict().FindString("etldPlus1")));
  }

  // Each call to HandleGetAllSites() above added a callback to the profile's
  // browsing_data::LocalStorageHelper, so make sure these aren't stuck waiting
  // to run at the end of the test.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

TEST_F(SiteSettingsHandlerTest, GetAllSitesIncludesStorage) {
  SetupModel();

  base::Value::List get_all_sites_args;
  get_all_sites_args.Append(kCallbackId);

  // Test all sites is empty when there are no preferences.
  handler()->HandleGetAllSites(get_all_sites_args);
  EXPECT_EQ(1U, web_ui()->call_data().size());

  // Add a couple of double-keyed exceptions and check that they appear for the
  // primary (embedded) site using a representative URL.
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  const GURL embedded_url("https://altostrat.com");
  const GURL embedding_url1("https://examplepetstore.com");
  const GURL embedding_url2("https://cymbalgroup.com");
  map->SetContentSettingDefaultScope(embedded_url, embedding_url1,
                                     ContentSettingsType::STORAGE_ACCESS,
                                     CONTENT_SETTING_ALLOW);
  map->SetContentSettingDefaultScope(embedded_url, embedding_url2,
                                     ContentSettingsType::STORAGE_ACCESS,
                                     CONTENT_SETTING_ALLOW);
  handler()->HandleGetAllSites(get_all_sites_args);

  {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->GetBool());
    const base::Value::List& site_groups = data.arg3()->GetList();
    EXPECT_EQ(1UL, site_groups.size());
    for (const base::Value& site_group_val : site_groups) {
      const base::Value::Dict& site_group = site_group_val.GetDict();
      const base::Value::List& origin_list =
          CHECK_DEREF(site_group.FindList("origins"));
      EXPECT_THAT(CHECK_DEREF(site_group.FindString("groupingKey")),
                  IsEtldPlus1("altostrat.com"));
      EXPECT_EQ(1UL, origin_list.size());
    }
  }

  // Now remove the double-keyed exceptions using SetOriginPermissions and
  // verify that the list is empty.
  base::Value::List reset_args;
  reset_args.Append(embedded_url.spec());
  reset_args.Append(std::move(kStorageAccess));
  reset_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_DEFAULT));

  handler()->HandleSetOriginPermissions(reset_args);

  handler()->HandleGetAllSites(get_all_sites_args);

  {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->GetBool());
    const base::Value::List& site_groups = data.arg3()->GetList();
    EXPECT_EQ(0UL, site_groups.size());
  }

  // Each call to HandleGetAllSites() above added a callback to the profile's
  // browsing_data::LocalStorageHelper, so make sure these aren't stuck waiting
  // to run at the end of the test.
  base::RunLoop().RunUntilIdle();
}

TEST_F(SiteSettingsHandlerTest, Cookies) {
  base::Value::List get_all_sites_args;
  get_all_sites_args.Append(kCallbackId);

  // Tests that a cookie eTLD+1 origin, which should use a placeholder in
  // AllSitesMap, returns the correct origin in GetAllSites.
  // This corresponds to case 1 in InsertOriginIntoGroup.
  {
    SetupModel(base::BindLambdaForTesting([](const TestModel& model) {
      model.browsing_data_model->AddBrowsingData(
          *(CreateCookieKey(GURL("http://c1.com"), "A=1")),
          BrowsingDataModel::StorageType::kCookie,
          /*storage_size=*/0,
          /*cookie_count=*/1);
    }));

    base::Value::List site_groups = GetOnStorageFetchedSentList();

    ASSERT_EQ(1UL, site_groups.size());
    const base::Value::Dict& first_group = site_groups[0].GetDict();
    EXPECT_THAT(CHECK_DEREF(first_group.FindString("groupingKey")),
                IsEtldPlus1("c1.com"));
    EXPECT_EQ(1, *first_group.FindInt("numCookies"));
    const base::Value::List& first_group_origins =
        CHECK_DEREF(first_group.FindList("origins"));
    ASSERT_EQ(1UL, first_group_origins.size());
    EXPECT_EQ(
        "http://c1.com/",
        CHECK_DEREF(first_group_origins[0].GetDict().FindString("origin")));
    EXPECT_EQ(1, first_group_origins[0].GetDict().FindInt("numCookies"));
  }

  // Tests that multiple cookie eTLD+1 origins result in a single origin being
  // returned in GetAllSites.
  // This corresponds to case 2 in InsertOriginIntoGroup.
  {
    SetupModel(base::BindLambdaForTesting([](const TestModel& model) {
      model.browsing_data_model->AddBrowsingData(
          *(CreateCookieKey(GURL("https://c2.com"), "A=1")),
          BrowsingDataModel::StorageType::kCookie,
          /*storage_size=*/0,
          /*cookie_count=*/1);
      model.browsing_data_model->AddBrowsingData(
          *(CreateCookieKey(GURL("https://c2.com"), "B=1")),
          BrowsingDataModel::StorageType::kCookie,
          /*storage_size=*/0,
          /*cookie_count=*/1);
    }));

    base::Value::List site_groups = GetOnStorageFetchedSentList();

    ASSERT_EQ(1UL, site_groups.size());
    const base::Value::Dict& first_group = site_groups[0].GetDict();
    EXPECT_THAT(CHECK_DEREF(first_group.FindString("groupingKey")),
                IsEtldPlus1("c2.com"));
    EXPECT_EQ(2, *first_group.FindInt("numCookies"));
    const base::Value::List& first_group_origins =
        CHECK_DEREF(first_group.FindList("origins"));
    ASSERT_EQ(1UL, first_group_origins.size());
    EXPECT_EQ(
        "http://c2.com/",
        CHECK_DEREF(first_group_origins[0].GetDict().FindString("origin")));
    EXPECT_EQ(2, first_group_origins[0].GetDict().FindInt("numCookies"));
  }

  // Tests that an HTTP cookie origin will reuse an equivalent HTTPS origin if
  // one exists.
  // This corresponds to case 3 in InsertOriginIntoGroup.
  {
    SetupModel(base::BindLambdaForTesting([](const TestModel& model) {
      model.browsing_data_model->AddBrowsingData(
          url::Origin::Create(GURL("https://w.c3.com")),
          BrowsingDataModel::StorageType::kTrustTokens, 50);
      model.browsing_data_model->AddBrowsingData(
          *(CreateCookieKey(GURL("http://w.c3.com"), "A=1")),
          BrowsingDataModel::StorageType::kCookie,
          /*storage_size=*/0,
          /*cookie_count=*/1);
    }));

    base::Value::List site_groups = GetOnStorageFetchedSentList();

    ASSERT_EQ(1UL, site_groups.size());
    const base::Value::Dict& first_group = site_groups[0].GetDict();
    EXPECT_THAT(CHECK_DEREF(first_group.FindString("groupingKey")),
                IsEtldPlus1("c3.com"));
    EXPECT_EQ(1, *first_group.FindInt("numCookies"));
    const base::Value::List& first_group_origins =
        CHECK_DEREF(first_group.FindList("origins"));
    ASSERT_EQ(1UL, first_group_origins.size());
    EXPECT_EQ(
        "https://w.c3.com/",
        CHECK_DEREF(first_group_origins[0].GetDict().FindString("origin")));
    EXPECT_EQ(1, first_group_origins[0].GetDict().FindInt("numCookies"));
  }

  // Tests that placeholder cookie eTLD+1 origins get removed from AllSitesMap
  // when a more specific origin is added later.
  {
    SetupModel(base::BindLambdaForTesting([](const TestModel& model) {
      model.browsing_data_model->AddBrowsingData(
          *(CreateCookieKey(GURL("https://c4.com"), "B=1")),
          BrowsingDataModel::StorageType::kCookie,
          /*storage_size=*/0,
          /*cookie_count=*/1);
      model.browsing_data_model->AddBrowsingData(
          *(CreateCookieKey(GURL("https://w.c4.com"), "A=1")),
          BrowsingDataModel::StorageType::kCookie,
          /*storage_size=*/0,
          /*cookie_count=*/1);
    }));

    base::Value::List site_groups = GetOnStorageFetchedSentList();

    ASSERT_EQ(1UL, site_groups.size());
    const base::Value::Dict& first_group = site_groups[0].GetDict();
    EXPECT_THAT(CHECK_DEREF(first_group.FindString("groupingKey")),
                IsEtldPlus1("c4.com"));
    EXPECT_EQ(2, *first_group.FindInt("numCookies"));
    const base::Value::List& first_group_origins =
        CHECK_DEREF(first_group.FindList("origins"));
    ASSERT_EQ(1UL, first_group_origins.size());
    EXPECT_EQ(
        "https://w.c4.com/",
        CHECK_DEREF(first_group_origins[0].GetDict().FindString("origin")));
    EXPECT_EQ(1, first_group_origins[0].GetDict().FindInt("numCookies"));
  }
}

TEST_F(SiteSettingsHandlerTest, GetRecentSitePermissions) {
  // Constants used only in this test.
  std::string kAllowed =
      content_settings::ContentSettingToString(CONTENT_SETTING_ALLOW);
  std::string kBlocked = content_settings::ContentSettingToString(
      ContentSetting::CONTENT_SETTING_BLOCK);
  std::string kEmbargo =
      SiteSettingSourceToString(site_settings::SiteSettingSource::kEmbargo);
  std::string kPreference =
      SiteSettingSourceToString(site_settings::SiteSettingSource::kPreference);

  base::Value::List get_recent_permissions_args;
  get_recent_permissions_args.Append(kCallbackId);
  get_recent_permissions_args.Append(3);

  // Configure prefs and auto blocker with a controllable clock.
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetClockForTesting(&clock);
  permissions::PermissionDecisionAutoBlocker* auto_blocker =
      PermissionDecisionAutoBlockerFactory::GetForProfile(profile());
  auto_blocker->SetClockForTesting(&clock);
  clock.Advance(base::Hours(1));

  // Test recent permissions is empty when there are no preferences.
  handler()->HandleGetRecentSitePermissions(get_recent_permissions_args);
  EXPECT_EQ(1U, web_ui()->call_data().size());

  {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->GetBool());

    const base::Value::List& recent_permissions = data.arg3()->GetList();
    EXPECT_TRUE(recent_permissions.empty());
  }

  // Add numerous permissions from different sources and confirm that the recent
  // permissions are correctly transformed for usage by JS.
  const GURL url1("https://example.com");
  const GURL url2("http://example.com");
  for (int i = 0; i < 3; ++i)
    auto_blocker->RecordDismissAndEmbargo(
        url1, ContentSettingsType::NOTIFICATIONS, false);

  clock.Advance(base::Hours(2));
  clock.Advance(base::Hours(1));
  CreateIncognitoProfile();
  HostContentSettingsMap* incognito_map =
      HostContentSettingsMapFactory::GetForProfile(incognito_profile());
  incognito_map->SetClockForTesting(&clock);

  clock.Advance(base::Hours(1));
  permissions::PermissionDecisionAutoBlocker* incognito_auto_blocker =
      PermissionDecisionAutoBlockerFactory::GetForProfile(incognito_profile());
  incognito_auto_blocker->SetClockForTesting(&clock);
  for (int i = 0; i < 3; ++i)
    incognito_auto_blocker->RecordDismissAndEmbargo(
        url1, ContentSettingsType::NOTIFICATIONS, false);

  handler()->HandleGetRecentSitePermissions(get_recent_permissions_args);
  {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->GetBool());

    const base::Value::List& recent_permissions = data.arg3()->GetList();
    EXPECT_EQ(2UL, recent_permissions.size());
    const base::Value::Dict& first_permission = recent_permissions[0].GetDict();
    const base::Value::Dict& second_permission =
        recent_permissions[1].GetDict();

    EXPECT_EQ(url1.spec(), CHECK_DEREF(second_permission.FindString("origin")));
    EXPECT_EQ(url1.spec(), CHECK_DEREF(first_permission.FindString("origin")));

    EXPECT_TRUE(first_permission.FindBool("incognito").value_or(false));
    EXPECT_FALSE(second_permission.FindBool("incognito").value_or(true));

    const base::Value::List& incognito_url1_permissions =
        CHECK_DEREF(first_permission.FindList("recentPermissions"));
    const base::Value::List& url1_permissions =
        CHECK_DEREF(second_permission.FindList("recentPermissions"));

    EXPECT_EQ(1UL, incognito_url1_permissions.size());
    const base::Value::Dict& first_incognito_permission =
        incognito_url1_permissions[0].GetDict();

    EXPECT_EQ(kNotifications,
              CHECK_DEREF(first_incognito_permission.FindString("type")));
    EXPECT_EQ(kBlocked,
              CHECK_DEREF(first_incognito_permission.FindString("setting")));
    EXPECT_EQ(kEmbargo,
              CHECK_DEREF(first_incognito_permission.FindString("source")));

    const base::Value::Dict& first_url_permission =
        url1_permissions[0].GetDict();
    EXPECT_EQ(kNotifications,
              CHECK_DEREF(first_url_permission.FindString("type")));
    EXPECT_EQ(kBlocked,
              CHECK_DEREF(first_url_permission.FindString("setting")));
    EXPECT_EQ(kEmbargo, CHECK_DEREF(first_url_permission.FindString("source")));
  }
}

TEST_F(SiteSettingsHandlerTest, OnStorageFetched) {
  SetupModel();

  handler()->ClearAllSitesMapForTesting();
  handler()->OnStorageFetched();

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data.function_name());

  ASSERT_TRUE(data.arg1()->is_string());
  EXPECT_EQ("onStorageListFetched", data.arg1()->GetString());

  ASSERT_TRUE(data.arg2()->is_list());
  const base::Value::List& storage_and_cookie_list = data.arg2()->GetList();
  EXPECT_EQ(4U, storage_and_cookie_list.size());

  {
    const base::Value& site_group_val = storage_and_cookie_list[0];
    ASSERT_TRUE(site_group_val.is_dict());
    const base::Value::Dict& site_group = site_group_val.GetDict();

    ASSERT_THAT(CHECK_DEREF(site_group.FindString("groupingKey")),
                IsEtldPlus1("example.com"));

    EXPECT_EQ(3, site_group.FindDouble("numCookies"));

    const base::Value::List* origin_list = site_group.FindList("origins");
    ASSERT_TRUE(origin_list);
    // There will be 3 origins in this case. Cookie node with url
    // http://www.example.com/ will be treat as https://www.example.com/ because
    // this url existed in the storage nodes.
    EXPECT_EQ(3U, origin_list->size());

    const base::Value::Dict& origin_info_0 = (*origin_list)[0].GetDict();

    EXPECT_EQ("http://abc.example.com/",
              CHECK_DEREF(origin_info_0.FindString("origin")));
    EXPECT_EQ(0, origin_info_0.FindDouble("engagement"));
    EXPECT_EQ(0, origin_info_0.FindDouble("usage"));
    EXPECT_EQ(1, origin_info_0.FindDouble("numCookies"));
    EXPECT_FALSE(origin_info_0.FindBool("isPartitioned").value_or(false));

    const base::Value::Dict& origin_info_1 = (*origin_list)[1].GetDict();

    EXPECT_EQ("https://google.com/",
              CHECK_DEREF(origin_info_1.FindString("origin")));
    EXPECT_EQ(0, origin_info_1.FindDouble("engagement"));
    EXPECT_EQ(100, origin_info_1.FindDouble("usage"));
    EXPECT_EQ(0, origin_info_1.FindDouble("numCookies"));
    EXPECT_TRUE(origin_info_1.FindBool("isPartitioned").value_or(false));

    const base::Value::Dict& origin_info_2 = (*origin_list)[2].GetDict();

    // Even though in the cookies the scheme is http, it still stored as https
    // because there is https data stored.
    EXPECT_EQ("https://www.example.com/",
              CHECK_DEREF(origin_info_2.FindString("origin")));
    EXPECT_EQ(0, origin_info_2.FindDouble("engagement"));
    EXPECT_EQ(302, origin_info_2.FindDouble("usage"));
    EXPECT_EQ(1, origin_info_2.FindDouble("numCookies"));
    EXPECT_FALSE(origin_info_2.FindBool("isPartitioned").value_or(false));
  }

  {
    const base::Value::Dict& site_group = storage_and_cookie_list[1].GetDict();

    ASSERT_THAT(CHECK_DEREF(site_group.FindString("groupingKey")),
                IsEtldPlus1("google.com"));

    EXPECT_EQ(3, site_group.FindDouble("numCookies"));

    const base::Value::List* origin_list = site_group.FindList("origins");
    ASSERT_TRUE(origin_list);

    EXPECT_EQ(2U, origin_list->size());

    const base::Value::Dict& partitioned_origin_info =
        (*origin_list)[0].GetDict();

    EXPECT_EQ("https://www.example.com/",
              CHECK_DEREF(partitioned_origin_info.FindString("origin")));
    EXPECT_EQ(0, partitioned_origin_info.FindDouble("engagement"));
    EXPECT_EQ(302, partitioned_origin_info.FindDouble("usage"));
    EXPECT_EQ(1, partitioned_origin_info.FindDouble("numCookies"));
    EXPECT_TRUE(
        partitioned_origin_info.FindBool("isPartitioned").value_or(false));

    const base::Value::Dict& unpartitioned_origin_info =
        (*origin_list)[1].GetDict();

    EXPECT_EQ("https://www.google.com/",
              CHECK_DEREF(unpartitioned_origin_info.FindString("origin")));
    EXPECT_EQ(0, unpartitioned_origin_info.FindDouble("engagement"));
    EXPECT_EQ(50000000000, unpartitioned_origin_info.FindDouble("usage"));
    EXPECT_EQ(0, unpartitioned_origin_info.FindDouble("numCookies"));
    EXPECT_FALSE(
        unpartitioned_origin_info.FindBool("isPartitioned").value_or(true));
  }

  {
    const base::Value& site_group_val = storage_and_cookie_list[2];
    ASSERT_TRUE(site_group_val.is_dict());
    const base::Value::Dict& site_group = site_group_val.GetDict();

    ASSERT_THAT(CHECK_DEREF(site_group.FindString("groupingKey")),
                IsEtldPlus1("google.com.au"));

    EXPECT_EQ(4, site_group.FindDouble("numCookies"));

    const base::Value::List* origin_list = site_group.FindList("origins");
    ASSERT_TRUE(origin_list);

    // The unpartitioned cookie set for google.com.au should be associated with
    // the eTLD+1, and thus won't have an origin entry as other origin entries
    // exist for the unpartitioned storage. The partitioned cookie for
    // google.com.au, partitioned by google.com.au should have also created an
    // entry.
    EXPECT_EQ(3U, origin_list->size());

    const base::Value::Dict& partitioned_origin_one_info =
        (*origin_list)[0].GetDict();

    EXPECT_EQ("https://google.com.au/",
              CHECK_DEREF(partitioned_origin_one_info.FindString("origin")));
    EXPECT_EQ(0, partitioned_origin_one_info.FindDouble("engagement"));
    EXPECT_EQ(0, partitioned_origin_one_info.FindDouble("usage"));
    EXPECT_EQ(1, partitioned_origin_one_info.FindDouble("numCookies"));
    EXPECT_TRUE(
        partitioned_origin_one_info.FindBool("isPartitioned").value_or(false));

    const base::Value::Dict& partitioned_origin_two_info =
        (*origin_list)[1].GetDict();
    EXPECT_EQ("https://www.another-example.com/",
              CHECK_DEREF(partitioned_origin_two_info.FindString("origin")));
    EXPECT_EQ(0, partitioned_origin_two_info.FindDouble("engagement"));
    EXPECT_EQ(0, partitioned_origin_two_info.FindDouble("usage"));
    EXPECT_EQ(1, partitioned_origin_two_info.FindDouble("numCookies"));
    EXPECT_TRUE(
        partitioned_origin_two_info.FindBool("isPartitioned").value_or(false));

    const base::Value::Dict& partitioned_origin_three_info =
        (*origin_list)[2].GetDict();

    EXPECT_EQ("https://www.example.com/",
              CHECK_DEREF(partitioned_origin_three_info.FindString("origin")));
    EXPECT_EQ(0, partitioned_origin_three_info.FindDouble("engagement"));
    EXPECT_EQ(302, partitioned_origin_three_info.FindDouble("usage"));
    EXPECT_EQ(1, partitioned_origin_three_info.FindDouble("numCookies"));
    EXPECT_TRUE(partitioned_origin_three_info.FindBool("isPartitioned")
                    .value_or(false));
  }

  {
    const base::Value& site_group_val = storage_and_cookie_list[3];
    ASSERT_TRUE(site_group_val.is_dict());
    const base::Value::Dict& site_group = site_group_val.GetDict();

    ASSERT_THAT(CHECK_DEREF(site_group.FindString("groupingKey")),
                IsEtldPlus1("ungrouped.com"));

    EXPECT_EQ(1, site_group.FindDouble("numCookies"));

    const base::Value::List* origin_list = site_group.FindList("origins");
    ASSERT_TRUE(origin_list);
    EXPECT_EQ(1U, origin_list->size());

    const base::Value::Dict& origin_info = (*origin_list)[0].GetDict();

    EXPECT_EQ("http://ungrouped.com/",
              CHECK_DEREF(origin_info.FindString("origin")));
    EXPECT_EQ(0, origin_info.FindDouble("engagement"));
    EXPECT_EQ(0, origin_info.FindDouble("usage"));
    EXPECT_EQ(1, origin_info.FindDouble("numCookies"));
  }
}

TEST_F(SiteSettingsHandlerTest, InstalledApps) {
  GURL start_url("http://abc.example.com/path");
  RegisterWebApp(
      profile(),
      MakeApp(web_app::GenerateAppId(/*manifest_id=*/std::nullopt, start_url),
              apps::AppType::kWeb, start_url.spec(), apps::Readiness::kReady,
              apps::InstallReason::kSync));

  SetupModel();

  base::Value::List storage_and_cookie_list = GetOnStorageFetchedSentList();
  EXPECT_EQ(4U, storage_and_cookie_list.size());

  {
    const base::Value& site_group_val = storage_and_cookie_list[0];
    ASSERT_TRUE(site_group_val.is_dict());
    const base::Value::Dict& site_group = site_group_val.GetDict();

    ASSERT_THAT(CHECK_DEREF(site_group.FindString("groupingKey")),
                IsEtldPlus1("example.com"));

    ASSERT_TRUE(site_group.FindBool("hasInstalledPWA").value_or(false));

    const base::Value::List* origin_list = site_group.FindList("origins");
    ASSERT_TRUE(origin_list);

    const base::Value::Dict& origin_info = (*origin_list)[0].GetDict();

    EXPECT_EQ("http://abc.example.com/",
              CHECK_DEREF(origin_info.FindString("origin")));
    EXPECT_TRUE(origin_info.FindBool("isInstalled").value_or(false));
  }

  // Verify that installed booleans are false for other siteGroups/origins
  {
    const base::Value& site_group_val = storage_and_cookie_list[1];
    ASSERT_TRUE(site_group_val.is_dict());
    const base::Value::Dict& site_group = site_group_val.GetDict();

    ASSERT_THAT(CHECK_DEREF(site_group.FindString("groupingKey")),
                IsEtldPlus1("google.com"));
    EXPECT_FALSE(site_group.FindBool("hasInstalledPWA").value_or(true));

    const base::Value::List* origin_list = site_group.FindList("origins");
    ASSERT_TRUE(origin_list);

    for (const auto& origin_info : *origin_list) {
      ASSERT_TRUE(origin_info.is_dict());
      EXPECT_FALSE(
          origin_info.GetDict().FindBool("isInstalled").value_or(true));
    }
  }
}

TEST_F(SiteSettingsHandlerTest, IncognitoExceptions) {
  constexpr char kOriginToBlock[] = "https://www.blocked.com:443";

  auto validate_exception = [&kOriginToBlock](const base::Value& exception) {
    ASSERT_TRUE(exception.is_dict());

    ASSERT_TRUE(exception.GetDict().FindString(site_settings::kOrigin));
    ASSERT_EQ(kOriginToBlock,
              *exception.GetDict().FindString(site_settings::kOrigin));
  };

  CreateIncognitoProfile();

  {
    SetContentSettingCustomScope(kOriginToBlock, std::string(),
                                 kPermissionNotifications,
                                 CONTENT_SETTING_BLOCK, 2U,
                                 /*is_incognito=*/true);

    base::Value::List get_exception_list_args;
    get_exception_list_args.Append(kCallbackId);
    get_exception_list_args.Append(kNotifications);
    handler()->HandleGetExceptionList(get_exception_list_args);

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();

    ASSERT_TRUE(data.arg3()->is_list());
    const base::Value::List& exceptions = data.arg3()->GetList();
    ASSERT_EQ(1U, exceptions.size());

    validate_exception(exceptions[0]);
  }

  {
    SetContentSettingCustomScope(kOriginToBlock, std::string(),
                                 kPermissionNotifications,
                                 CONTENT_SETTING_BLOCK, 5U,
                                 /*is_incognito=*/false);

    base::Value::List get_exception_list_args;
    get_exception_list_args.Append(kCallbackId);
    get_exception_list_args.Append(kNotifications);
    handler()->HandleGetExceptionList(get_exception_list_args);

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();

    ASSERT_TRUE(data.arg3()->is_list());
    const base::Value::List& exceptions = data.arg3()->GetList();
    ASSERT_EQ(2U, exceptions.size());

    validate_exception(exceptions[0]);
    validate_exception(exceptions[1]);
  }

  DestroyIncognitoProfile();
}

TEST_F(SiteSettingsHandlerTest, ResetCategoryPermissionForEmbargoedOrigins) {
  constexpr char kOriginToBlock[] = "https://www.blocked.com:443";
  constexpr char kOriginToEmbargo[] = "https://embargoed.co.uk";

  // Add and test 1 blocked origin
  {
    base::Value::List set_args;
    set_args.Append(kOriginToBlock);  // Primary pattern.
    set_args.Append(std::string());   // Secondary pattern.
    set_args.Append(kNotifications);
    set_args.Append(
        content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
    set_args.Append(false);  // Incognito.

    handler()->HandleSetCategoryPermissionForPattern(set_args);
    ASSERT_EQ(2U, web_ui()->call_data().size());
    // When HandleSetCategoryPermissionForPattern is called for a notification
    // permission, there are two callbacks that make call_data size increase
    // by 2 instead of 1.
    ValidateCallbacksForNotificationPermission(0);
  }

  // Add and test 1 embargoed origin.
  {
    auto* auto_blocker =
        PermissionDecisionAutoBlockerFactory::GetForProfile(profile());
    for (size_t i = 0; i < 3; ++i) {
      auto_blocker->RecordDismissAndEmbargo(GURL(kOriginToEmbargo),
                                            kPermissionNotifications, false);
    }
    // Check that origin is under embargo.
    EXPECT_EQ(
        PermissionStatus::DENIED,
        auto_blocker
            ->GetEmbargoResult(GURL(kOriginToEmbargo), kPermissionNotifications)
            ->status);
  }

  // Check there are 2 blocked origins.
  {
    base::Value::List exceptions;
    site_settings::GetExceptionsForContentType(
        kPermissionNotifications, profile(), web_ui(),
        /*incognito=*/false, &exceptions);

    // The size should be 2, 1st is blocked origin, 2nd is embargoed origin.
    ASSERT_EQ(2U, exceptions.size());
  }

  {
    // Reset blocked origin.
    base::Value::List reset_args;
    reset_args.Append(kOriginToBlock);
    reset_args.Append(std::string());
    reset_args.Append(kNotifications);
    reset_args.Append(false);  // Incognito.
    handler()->HandleResetCategoryPermissionForPattern(reset_args);

    // Check there is 1 blocked origin.
    base::Value::List exceptions;
    site_settings::GetExceptionsForContentType(
        kPermissionNotifications, profile(), web_ui(),
        /*incognito=*/false, &exceptions);
    ASSERT_EQ(1U, exceptions.size());
  }

  {
    // Reset embargoed origin.
    base::Value::List reset_args;
    reset_args.Append(kOriginToEmbargo);
    reset_args.Append(std::string());
    reset_args.Append(kNotifications);
    reset_args.Append(false);  // Incognito.
    handler()->HandleResetCategoryPermissionForPattern(reset_args);

    // Check that there are no blocked or embargoed origins.
    base::Value::List exceptions;
    site_settings::GetExceptionsForContentType(
        kPermissionNotifications, profile(), web_ui(),
        /*incognito=*/false, &exceptions);
    ASSERT_TRUE(exceptions.empty());
  }
}

TEST_F(SiteSettingsHandlerTest, ResetCategoryPermissionForInvalidOrigins) {
  constexpr char kInvalidOrigin[] = "example.com";
  auto url = GURL(kInvalidOrigin);
  EXPECT_FALSE(url.is_valid());
  EXPECT_TRUE(url.is_empty());

  base::Value::List set_args;
  set_args.Append(kInvalidOrigin);  // Primary pattern.
  set_args.Append(std::string());   // Secondary pattern.
  set_args.Append(kNotifications);
  set_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  set_args.Append(false);  // Incognito.

  handler()->HandleSetCategoryPermissionForPattern(set_args);
  ASSERT_EQ(2U, web_ui()->call_data().size());
  // When HandleSetCategoryPermissionForPattern is called for a notification
  // permission, there are two callbacks that make call_data size increase
  // by 2 instead of 1.
  ValidateCallbacksForNotificationPermission(0);

  // Reset blocked origin.
  base::Value::List reset_args;
  reset_args.Append(kInvalidOrigin);
  reset_args.Append(std::string());
  reset_args.Append(kNotifications);
  reset_args.Append(false);  // Incognito.
  // Check that this method is not crashing for an invalid origin.
  handler()->HandleResetCategoryPermissionForPattern(reset_args);
}

TEST_F(SiteSettingsHandlerTest, SetCategory_GetException_ResetCategory) {
  const std::string google("https://www.google.com:443");
  {
    // Test the JS -> C++ -> JS callback path for configuring origins, by
    // setting Google.com to blocked.
    base::Value::List set_args;
    set_args.Append(google);         // Primary pattern.
    set_args.Append(std::string());  // Secondary pattern.
    set_args.Append(kNotifications);
    set_args.Append(
        content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
    set_args.Append(false);  // Incognito.
    handler()->HandleSetCategoryPermissionForPattern(set_args);
    EXPECT_EQ(2U, web_ui()->call_data().size());
    // When HandleSetCategoryPermissionForPattern is called for a notification
    // permission, there are two callbacks that make call_data size increase
    // by 2 instead of 1.
    ValidateCallbacksForNotificationPermission(0);
  }

  base::Value::List get_exception_list_args;
  get_exception_list_args.Append(kCallbackId);
  get_exception_list_args.Append(kNotifications);
  handler()->HandleGetExceptionList(get_exception_list_args);
  ValidateOrigin(google, "", google, CONTENT_SETTING_BLOCK,
                 site_settings::SiteSettingSource::kPreference, 3U);

  {
    // Reset things back to how they were.
    base::Value::List reset_args;
    reset_args.Append(google);
    reset_args.Append(std::string());
    reset_args.Append(kNotifications);
    reset_args.Append(false);  // Incognito.
    handler()->HandleResetCategoryPermissionForPattern(reset_args);
    EXPECT_EQ(5U, web_ui()->call_data().size());
    // When HandleResetCategoryPermissionForPattern is called for a notification
    // permission, there are two callbacks that make call_data size increase
    // by 2 instead of 1.
    ValidateCallbacksForNotificationPermission(3);
  }

  // Verify the reset was successful.
  handler()->HandleGetExceptionList(get_exception_list_args);
  ValidateNoOrigin(6U);
}

TEST_F(SiteSettingsHandlerTest, NotificationPermissionRevokeUkm) {
  const std::string google("https://www.google.com");
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  {
    base::Value::List set_notification_origin_args;
    set_notification_origin_args.Append(google);
    set_notification_origin_args.Append("");
    set_notification_origin_args.Append(kNotifications);
    set_notification_origin_args.Append(
        content_settings::ContentSettingToString(CONTENT_SETTING_ALLOW));
    set_notification_origin_args.Append(false /* incognito */);
    handler()->HandleSetCategoryPermissionForPattern(
        set_notification_origin_args);
  }

  {
    base::Value::List set_notification_origin_args;
    set_notification_origin_args.Append(google);
    set_notification_origin_args.Append("");
    set_notification_origin_args.Append(kNotifications);
    set_notification_origin_args.Append(
        content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
    set_notification_origin_args.Append(false /* incognito */);
    handler()->HandleSetCategoryPermissionForPattern(
        set_notification_origin_args);
  }

  auto entries = ukm_recorder.GetEntriesByName("Permission");
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries.front().get();

  ukm_recorder.ExpectEntrySourceHasUrl(entry, GURL(google));
  EXPECT_EQ(
      *ukm_recorder.GetEntryMetric(entry, "Source"),
      static_cast<int64_t>(permissions::PermissionSourceUI::SITE_SETTINGS));

  EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "PermissionType"),
            content_settings_uma_util::ContentSettingTypeToHistogramValue(
                ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "Action"),
            static_cast<int64_t>(permissions::PermissionAction::REVOKED));
}

TEST_F(SiteSettingsHandlerTest, IncrementsTrackingProtectionMetrics) {
  constexpr char kOrigin[] = "https://www.test.com:443";
  base::UserActionTester user_actions;

  base::Value::List set_args;
  set_args.Append(kOrigin);        // Primary pattern.
  set_args.Append(std::string());  // Secondary pattern.
  set_args.Append(kTrackingProtection);
  set_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_ALLOW));
  set_args.Append(false);  // Incognito
  handler()->HandleSetCategoryPermissionForPattern(set_args);
  EXPECT_EQ(user_actions.GetActionCount(
                "Settings.TrackingProtection.SiteExceptionAdded"),
            1);

  base::Value::List reset_args;
  reset_args.Append(kOrigin);        // Primary pattern.
  reset_args.Append(std::string());  // Secondary pattern.
  reset_args.Append(kTrackingProtection);
  reset_args.Append(false);  // Incognito
  handler()->HandleResetCategoryPermissionForPattern(reset_args);
  EXPECT_EQ(user_actions.GetActionCount(
                "Settings.TrackingProtection.SiteExceptionRemoved"),
            1);
}

class Reset3pcCategoryPermissionTest
    : public SiteSettingsHandlerBaseTest,
      public testing::WithParamInterface<bool> {
 public:
  Reset3pcCategoryPermissionTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          privacy_sandbox::kTrackingProtectionContentSettingInSettings);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, Reset3pcCategoryPermissionTest, testing::Bool());

TEST_P(Reset3pcCategoryPermissionTest,
       RemovesTrackingProtectionExceptionsWhenFeatureIsOff) {
  constexpr char kOrigin[] = "https://www.test.com:443";
  base::Value::List set_args;
  set_args.Append("*");        // Primary pattern.
  set_args.Append(kOrigin);  // Secondary pattern.
  set_args.Append(kTrackingProtection);
  set_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_ALLOW));
  set_args.Append(false);  // Incognito
  handler()->HandleSetCategoryPermissionForPattern(set_args);
  // We should have 1 Tracking Protection exception
  base::Value::List initial_exceptions;
  site_settings::GetExceptionsForContentType(
      ContentSettingsType::TRACKING_PROTECTION, profile(), web_ui(),
      /*incognito=*/false, &initial_exceptions);
  EXPECT_EQ(initial_exceptions.size(), 1U);

  base::Value::List reset_args;
  reset_args.Append("*");      // Primary pattern.
  reset_args.Append(kOrigin);  // Secondary pattern.
  reset_args.Append(kCookies);
  reset_args.Append(false);  // Incognito
  handler()->HandleResetCategoryPermissionForPattern(reset_args);
  base::Value::List actual_exceptions;
  site_settings::GetExceptionsForContentType(
      ContentSettingsType::TRACKING_PROTECTION, profile(), web_ui(),
      /*incognito=*/false, &actual_exceptions);
  // The exception should only have been removed if the feature is off.
  EXPECT_EQ(actual_exceptions.size(), GetParam() ? 1U : 0U);
}

// TODO(crbug.com/40688152): Test flakes on TSAN and ASAN.
#if defined(THREAD_SANITIZER) || defined(ADDRESS_SANITIZER)
#define MAYBE_DefaultSettingSource DISABLED_DefaultSettingSource
#else
#define MAYBE_DefaultSettingSource DefaultSettingSource
#endif
TEST_F(SiteSettingsHandlerTest, MAYBE_DefaultSettingSource) {
  // Use a non-default port to verify the display name does not strip this
  // off.
  const std::string google("https://www.google.com:183");
  const std::string expected_display_name("www.google.com:183");

  ContentSettingSourceSetter source_setter(profile(),
                                           ContentSettingsType::NOTIFICATIONS);

  base::Value::List get_origin_permissions_args;
  get_origin_permissions_args.Append(kCallbackId);
  get_origin_permissions_args.Append(google);
  base::Value::List category_list;
  category_list.Append(kNotifications);
  get_origin_permissions_args.Append(std::move(category_list));

  // Test Chrome built-in defaults are marked as default.
  handler()->HandleGetOriginPermissions(get_origin_permissions_args);
  ValidateOrigin(google, google, expected_display_name, CONTENT_SETTING_ASK,
                 site_settings::SiteSettingSource::kDefault, 1U);

  base::Value::List default_value_args;
  default_value_args.Append(kNotifications);
  default_value_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  handler()->HandleSetDefaultValueForContentType(default_value_args);
  // A user-set global default should also show up as default.
  handler()->HandleGetOriginPermissions(get_origin_permissions_args);
  ValidateOrigin(google, google, expected_display_name, CONTENT_SETTING_BLOCK,
                 site_settings::SiteSettingSource::kDefault, 3U);

  base::Value::List set_notification_pattern_args;
  set_notification_pattern_args.Append("[*.]google.com");
  set_notification_pattern_args.Append("");
  set_notification_pattern_args.Append(kNotifications);
  set_notification_pattern_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_ALLOW));
  set_notification_pattern_args.Append(false);
  handler()->HandleSetCategoryPermissionForPattern(
      set_notification_pattern_args);
  ASSERT_EQ(5U, web_ui()->call_data().size());
  // When HandleSetCategoryPermissionForPattern is called for a notification
  // permission, there are two callbacks that make call_data size increase
  // by 2 instead of 1.
  ValidateCallbacksForNotificationPermission(3);
  // A user-set pattern should not show up as default.
  handler()->HandleGetOriginPermissions(get_origin_permissions_args);
  ValidateOrigin(google, google, expected_display_name, CONTENT_SETTING_ALLOW,
                 site_settings::SiteSettingSource::kPreference, 6U);

  base::Value::List set_notification_origin_args;
  set_notification_origin_args.Append(google);
  set_notification_origin_args.Append("");
  set_notification_origin_args.Append(kNotifications);
  set_notification_origin_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  set_notification_origin_args.Append(false);
  handler()->HandleSetCategoryPermissionForPattern(
      set_notification_origin_args);
  ASSERT_EQ(8U, web_ui()->call_data().size());
  // When HandleSetCategoryPermissionForPattern is called for a notification
  // permission, there are two callbacks that make call_data size increase
  // by 2 instead of 1.
  ValidateCallbacksForNotificationPermission(6);
  // A user-set per-origin permission should not show up as default.
  handler()->HandleGetOriginPermissions(get_origin_permissions_args);
  ValidateOrigin(google, google, expected_display_name, CONTENT_SETTING_BLOCK,
                 site_settings::SiteSettingSource::kPreference, 9U);

  // Enterprise-policy set defaults should not show up as default.
  source_setter.SetPolicyDefault(CONTENT_SETTING_ALLOW);
  handler()->HandleGetOriginPermissions(get_origin_permissions_args);
  ValidateOrigin(google, google, expected_display_name, CONTENT_SETTING_ALLOW,
                 site_settings::SiteSettingSource::kPolicy, 10U);
}

TEST_F(SiteSettingsHandlerTest, GetAndSetOriginPermissions) {
  const std::string origin_with_port("https://www.example.com:443");
  // The display name won't show the port if it's default for that scheme.
  const std::string origin("www.example.com");
  base::Value::List get_args;
  get_args.Append(kCallbackId);
  get_args.Append(origin_with_port);
  {
    base::Value::List category_list;
    category_list.Append(kNotifications);
    get_args.Append(std::move(category_list));
  }
  handler()->HandleGetOriginPermissions(get_args);
  ValidateOrigin(origin_with_port, origin_with_port, origin,
                 CONTENT_SETTING_ASK,
                 site_settings::SiteSettingSource::kDefault, 1U);

  // Block notifications.
  base::Value::List set_args;
  set_args.Append(origin_with_port);
  set_args.Append(kNotifications);
  set_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  handler()->HandleSetOriginPermissions(set_args);
  EXPECT_EQ(2U, web_ui()->call_data().size());

  // Reset things back to how they were.
  base::Value::List reset_args;
  reset_args.Append(origin_with_port);
  reset_args.Append(std::move(kNotifications));
  reset_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_DEFAULT));

  handler()->HandleSetOriginPermissions(reset_args);
  EXPECT_EQ(3U, web_ui()->call_data().size());

  // Verify the reset was successful.
  handler()->HandleGetOriginPermissions(get_args);
  ValidateOrigin(origin_with_port, origin_with_port, origin,
                 CONTENT_SETTING_ASK,
                 site_settings::SiteSettingSource::kDefault, 4U);
}

TEST_F(SiteSettingsHandlerTest, SetOriginPermissionsForStorageAccess) {
  const GURL origin_a_with_port("https://www.example.com:443");
  const GURL origin_b_with_port("https://www.examplepetstore.com:443");
  // The display name won't show the port if it's default for that scheme.
  const std::string origin_a_display_name("example.com");
  const std::string origin_b_display_name("examplepetstore.com");
  // The resulting pattern will be broadened to the scheme+eTLD+1 level.
  const std::string origin_a_pattern("https://[*.]example.com");
  const std::string origin_b_pattern("https://[*.]examplepetstore.com");

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  // Allow storage access for this origin/embedding pair, in both directions.
  // This is an unlikely scenario in practice but we want users to be able to
  // clear an embedding relationship between two sites from either site.
  map->SetContentSettingDefaultScope(origin_a_with_port, origin_b_with_port,
                                     ContentSettingsType::STORAGE_ACCESS,
                                     CONTENT_SETTING_ALLOW);
  map->SetContentSettingDefaultScope(origin_b_with_port, origin_a_with_port,
                                     ContentSettingsType::STORAGE_ACCESS,
                                     CONTENT_SETTING_ALLOW);

  base::Value::List get_exception_list_args;
  get_exception_list_args.Append(kCallbackId);
  get_exception_list_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_ALLOW));
  handler()->HandleGetStorageAccessExceptionList(get_exception_list_args);

  // Verify that the group exception was created correctly.
  ValidateStorageAccessList(/*expected_total_calls=*/3U,
                            /*expected_num_groups=*/2U);

  ValidateStorageAccessException(
      origin_a_pattern, origin_a_display_name, CONTENT_SETTING_ALLOW,
      {{origin_b_pattern, origin_b_display_name, /*incognito=*/false}},
      /*index=*/0U);

  ValidateStorageAccessException(
      origin_b_pattern, origin_b_display_name, CONTENT_SETTING_ALLOW,
      {{origin_a_pattern, origin_a_display_name, /*incognito=*/false}},
      /*index=*/1U);

  // Reset things to default for STORAGE_ACCESS on origin_a.
  base::Value::List reset_args;
  reset_args.Append(origin_a_with_port.spec());
  reset_args.Append(std::move(kStorageAccess));
  reset_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_DEFAULT));
  handler()->HandleSetOriginPermissions(reset_args);
  EXPECT_EQ(5U, web_ui()->call_data().size());

  // Verify that there are no storage access exceptions after resetting.
  handler()->HandleGetStorageAccessExceptionList(get_exception_list_args);
  ValidateStorageAccessList(/*expected_total_calls=*/6U,
                            /*expected_num_groups=*/0U);
}

TEST_F(SiteSettingsHandlerTest, GetAndSetForInvalidURLs) {
  const std::string origin("arbitrary string");
  EXPECT_FALSE(GURL(origin).is_valid());
  base::Value::List get_args;
  get_args.Append(kCallbackId);
  get_args.Append(origin);
  {
    base::Value::List category_list;
    category_list.Append(kNotifications);
    get_args.Append(std::move(category_list));
  }
  handler()->HandleGetOriginPermissions(get_args);
  // Verify that it'll return CONTENT_SETTING_BLOCK as |origin| is not a secure
  // context, a requirement for notifications. Note that the display string
  // will be blank since it's an invalid URL.
  ValidateOrigin(origin, origin, "", CONTENT_SETTING_BLOCK,
                 site_settings::SiteSettingSource::kInsecureOrigin, 1U);

  // Make sure setting a permission on an invalid origin doesn't crash.
  base::Value::List set_args;
  set_args.Append(origin);
  {
    base::Value::List category_list;
    category_list.Append(kNotifications);
    set_args.Append(std::move(category_list));
  }
  set_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_ALLOW));
  handler()->HandleSetOriginPermissions(set_args);

  // Also make sure the content setting for |origin| wasn't actually changed.
  handler()->HandleGetOriginPermissions(get_args);
  ValidateOrigin(origin, origin, "", CONTENT_SETTING_BLOCK,
                 site_settings::SiteSettingSource::kInsecureOrigin, 2U);
}

TEST_F(SiteSettingsHandlerTest, SetCategoryPermissionForPattern) {
  const std::string kOrigin = "https://www.example.com:443";

  base::Value::List set_args;
  set_args.Append(kOrigin);        // Primary pattern.
  set_args.Append(std::string());  // Secondary pattern.
  set_args.Append(kNotifications);
  set_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  set_args.Append(false);  // Incognito.

  handler()->HandleSetCategoryPermissionForPattern(set_args);
  EXPECT_EQ(kNumberNotificationsContentSettingListeners,
            web_ui()->call_data().size());

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  ASSERT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(GURL(kOrigin), GURL(std::string()),
                                   kPermissionNotifications));
}

TEST_F(SiteSettingsHandlerTest, SetCategoryPermissionForPattern_WildCard) {
  const std::string kWildcardOrigin = "[*.]example.com";
  const std::string kRealOrigin = "https://www.example.com";

  base::Value::List set_args;
  set_args.Append(kWildcardOrigin);  // Primary pattern.
  set_args.Append(std::string());    // Secondary pattern.
  set_args.Append(kNotifications);
  set_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  set_args.Append(false);  // Incognito.

  handler()->HandleSetCategoryPermissionForPattern(set_args);
  EXPECT_EQ(kNumberNotificationsContentSettingListeners,
            web_ui()->call_data().size());

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  ASSERT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(GURL(kRealOrigin), GURL(std::string()),
                                   kPermissionNotifications));
}

TEST_F(SiteSettingsHandlerTest,
       SetCategoryPermissionForPattern_SecondaryPattern) {
  const std::string kOrigin = "https://www.example.com:443";
  const std::string kSecondary = "https://www.secondary.com:443";

  base::Value::List set_args;
  set_args.Append(kOrigin);     // Primary pattern.
  set_args.Append(kSecondary);  // Secondary pattern.
  set_args.Append(site_settings::ContentSettingsTypeToGroupName(
      ContentSettingsType::STORAGE_ACCESS));
  set_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  set_args.Append(false);  // Incognito.

  handler()->HandleSetCategoryPermissionForPattern(set_args);
  EXPECT_EQ(1U, web_ui()->call_data().size());

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  ASSERT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(GURL(kOrigin), GURL(kSecondary),
                                   kPermissionStorageAccess));
}

TEST_F(SiteSettingsHandlerTest, SetCategoryPermissionForPattern_Incognito) {
  const std::string kOrigin = "https://www.example.com:443";
  CreateIncognitoProfile();

  HostContentSettingsMap* incognito_map =
      HostContentSettingsMapFactory::GetForProfile(incognito_profile());
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  {
    base::Value::List set_args;
    set_args.Append(kOrigin);        // Primary pattern.
    set_args.Append(std::string());  // Secondary pattern.
    set_args.Append(kNotifications);
    set_args.Append(
        content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
    set_args.Append(true);  // Incognito.

    handler()->HandleSetCategoryPermissionForPattern(set_args);
    EXPECT_EQ(3U, web_ui()->call_data().size());

    ASSERT_EQ(CONTENT_SETTING_BLOCK, incognito_map->GetContentSetting(
                                         GURL(kOrigin), GURL(std::string()),
                                         kPermissionNotifications));
    // Shouldn't change the setting for the normal profile.
    ASSERT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(GURL(kOrigin), GURL(std::string()),
                                     kPermissionNotifications));
  }

  {
    base::Value::List set_args;
    set_args.Append(kOrigin);        // Primary pattern.
    set_args.Append(std::string());  // Secondary pattern.
    set_args.Append(kNotifications);
    set_args.Append(
        content_settings::ContentSettingToString(CONTENT_SETTING_ALLOW));
    set_args.Append(false);  // Incognito.

    handler()->HandleSetCategoryPermissionForPattern(set_args);
    EXPECT_EQ(6U, web_ui()->call_data().size());

    ASSERT_EQ(CONTENT_SETTING_ALLOW,
              map->GetContentSetting(GURL(kOrigin), GURL(std::string()),
                                     kPermissionNotifications));
    // Shouldn't change the setting for the incognito profile.
    ASSERT_EQ(CONTENT_SETTING_BLOCK, incognito_map->GetContentSetting(
                                         GURL(kOrigin), GURL(std::string()),
                                         kPermissionNotifications));
  }

  DestroyIncognitoProfile();
}

TEST_F(SiteSettingsHandlerTest,
       SetCategoryPermissionForPattern_ExceptionHelpers) {
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]google.com");
  base::Value::Dict exception = site_settings::GetExceptionForPage(
      ContentSettingsType::NOTIFICATIONS, /*profile=*/nullptr, pattern,
      ContentSettingsPattern::Wildcard(), pattern.ToString(),
      CONTENT_SETTING_BLOCK, site_settings::SiteSettingSource::kPreference,
      /*expiration=*/base::Time::Now(), /*incognito=*/false);

  CHECK(exception.FindString(site_settings::kOrigin));
  CHECK(exception.FindString(site_settings::kDisplayName));
  CHECK(exception.FindString(site_settings::kEmbeddingOrigin));
  CHECK(exception.FindString(site_settings::kSetting));
  // Notifications should not have a description.
  CHECK(!exception.FindString(site_settings::kDescription));
  CHECK(exception.FindBool(site_settings::kIncognito).has_value());

  base::Value::List args;
  args.Append(*exception.FindString(site_settings::kOrigin));
  args.Append(*exception.FindString(site_settings::kEmbeddingOrigin));
  args.Append(kNotifications);  // Chosen arbitrarily.
  args.Append(*exception.FindString(site_settings::kSetting));
  args.Append(*exception.FindBool(site_settings::kIncognito));

  // We don't need to check the results. This is just to make sure it doesn't
  // crash on the input.
  handler()->HandleSetCategoryPermissionForPattern(args);

  scoped_refptr<const extensions::Extension> extension;
  extension = extensions::ExtensionBuilder()
                  .SetManifest(base::Value::Dict()
                                   .Set("name", kExtensionName)
                                   .Set("version", "1.0.0")
                                   .Set("manifest_version", 2))
                  .SetID("ahfgeienlihckogmohjhadlkjgocpleb")
                  .Build();

  base::Value::List exceptions;
  site_settings::AddExceptionForHostedApp("[*.]google.com", *extension.get(),
                                          &exceptions);

  const base::Value& dictionary_value = exceptions[0];
  CHECK(dictionary_value.is_dict());
  const base::Value::Dict& dictionary = dictionary_value.GetDict();
  CHECK(dictionary.FindString(site_settings::kOrigin));
  CHECK(dictionary.FindString(site_settings::kDisplayName));
  CHECK(dictionary.FindString(site_settings::kEmbeddingOrigin));
  CHECK(dictionary.FindString(site_settings::kSetting));
  CHECK(dictionary.FindBool(site_settings::kIncognito).has_value());

  // Again, don't need to check the results.
  handler()->HandleSetCategoryPermissionForPattern(args);
}

TEST_F(SiteSettingsHandlerTest, SetCategoryPermissionForPattern_SessionOnly) {
  const std::string kGoogleWithPort("https://www.google.com:443");
  base::Value::List set_args;
  set_args.Append(kGoogleWithPort);  // Primary pattern.
  set_args.Append(std::string());    // Secondary pattern.
  set_args.Append(kCookies);
  set_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_SESSION_ONLY));
  set_args.Append(false);  // Incognito.
  handler()->HandleSetCategoryPermissionForPattern(set_args);

  EXPECT_EQ(kNumberCookiesContentSettingListeners,
            web_ui()->call_data().size());
}

TEST_F(SiteSettingsHandlerTest, ExtensionDisplayName) {
  // When the extension is loaded, displayName is the extension's name and id.
  auto extension = LoadExtension(kExtensionName);
  auto extension_url = extension->url().spec();
  {
    base::Value::List get_origin_permissions_args;
    get_origin_permissions_args.Append(kCallbackId);
    get_origin_permissions_args.Append(extension_url);
    {
      base::Value::List category_list;
      category_list.Append(kNotifications);
      get_origin_permissions_args.Append(std::move(category_list));
    }
    handler()->HandleGetOriginPermissions(get_origin_permissions_args);
    std::string expected_display_name =
        base::StringPrintf("Test Extension (ID: %s)", extension->id().c_str());
    ValidateOrigin(extension_url, extension_url, expected_display_name,
                   CONTENT_SETTING_ASK,
                   site_settings::SiteSettingSource::kDefault, 1U);
  }

  // When the extension is unloaded, the displayName is the extension's origin.
  UnloadExtension(extension->id());
  {
    base::Value::List get_origin_permissions_args;
    get_origin_permissions_args.Append(kCallbackId);
    get_origin_permissions_args.Append(extension_url);
    {
      base::Value::List category_list;
      category_list.Append(kNotifications);
      get_origin_permissions_args.Append(std::move(category_list));
    }
    handler()->HandleGetOriginPermissions(get_origin_permissions_args);
    ValidateOrigin(
        extension_url, extension_url,
        base::StringPrintf("chrome-extension://%s", extension->id().c_str()),
        CONTENT_SETTING_ASK, site_settings::SiteSettingSource::kDefault, 2U);
  }
}

TEST_F(SiteSettingsHandlerTest, PatternsAndContentType) {
  unsigned counter = 1;
  for (const auto& test_case : kPatternsAndContentTypeTestCases) {
    base::Value::List args;
    args.Append(kCallbackId);
    args.Append(test_case.arguments.pattern);
    args.Append(test_case.arguments.content_type);
    handler()->HandleIsPatternValidForType(args);
    ValidatePattern(test_case.expected.validity, counter,
                    test_case.expected.reason);
    ++counter;
  }
}

TEST_F(SiteSettingsHandlerTest, Incognito) {
  base::Value::List args;
  handler()->HandleUpdateIncognitoStatus(args);
  ValidateIncognitoExists(false, 1U);

  CreateIncognitoProfile();
  ValidateIncognitoExists(true, 2U);

  DestroyIncognitoProfile();
  ValidateIncognitoExists(false, 3U);
}

TEST_F(SiteSettingsHandlerTest, ZoomLevels) {
  std::string http_host("www.google.com");
  std::string error_host("chromewebdata");
  std::string data_url("data:text/plain;base64,SGVsbG8sIFdvcmxkIQ==");
  double zoom_level = 1.1;

  content::HostZoomMap* host_zoom_map =
      content::HostZoomMap::GetDefaultForBrowserContext(profile());
  host_zoom_map->SetZoomLevelForHost(http_host, zoom_level);
  host_zoom_map->SetZoomLevelForHost(error_host, zoom_level);
  host_zoom_map->SetZoomLevelForHost(data_url, zoom_level);
  ValidateZoom({{error_host, "(Chrome error pages)", "122%"},
                {data_url, data_url, "122%"},
                {http_host, http_host, "122%"}},
               3U);

  base::Value::List args;
  handler()->HandleFetchZoomLevels(args);
  ValidateZoom({{error_host, "(Chrome error pages)", "122%"},
                {data_url, data_url, "122%"},
                {http_host, http_host, "122%"}},
               4U);

  args.Append(http_host);
  handler()->HandleRemoveZoomLevel(args);
  args.front() = base::Value(error_host);
  handler()->HandleRemoveZoomLevel(args);
  args.front() = base::Value(data_url);
  handler()->HandleRemoveZoomLevel(args);
  ValidateZoom({}, 7U);

  double default_level = host_zoom_map->GetDefaultZoomLevel();
  double level = host_zoom_map->GetZoomLevelForHostAndScheme("http", http_host);
  EXPECT_EQ(default_level, level);
}

TEST_F(SiteSettingsHandlerTest, TemporaryCookieExceptions) {
  // Set a temporary exception directly, instead of relying on any helpers that
  // have duration configurable via feature parameters.
  constexpr int kExpirationDurationInDays = 100;

  content_settings::ContentSettingConstraints constraints;
  constraints.set_lifetime(base::Days(kExpirationDurationInDays));
  constraints.set_session_model(content_settings::mojom::SessionModel::DURABLE);

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  host_content_settings_map->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURL(GURL("https://example.com")),
      ContentSettingsType::COOKIES, ContentSetting::CONTENT_SETTING_ALLOW,
      constraints);

  base::Value::List get_exception_list_args;
  get_exception_list_args.Append(kCallbackId);
  get_exception_list_args.Append(kCookies);

  handler()->HandleGetExceptionList(get_exception_list_args);

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  const base::Value::List& exception_list = data.arg3()->GetList();
  EXPECT_EQ(1UL, exception_list.size());

  // Mirror the logic in the helper to avoid flakes on time edges.
  auto time_diff = (base::Time::Now() + base::Days(kExpirationDurationInDays))
                       .LocalMidnight() -
                   base::Time::Now().LocalMidnight();

  EXPECT_EQ(l10n_util::GetPluralStringFUTF8(
                IDS_SETTINGS_EXPIRES_AFTER_TIME_LABEL, time_diff.InDays()),
            CHECK_DEREF(exception_list[0].GetDict().FindString("description")));
}

class SiteSettingsHandlerIsolatedWebAppTest
    : public SiteSettingsHandlerBaseTest {
 public:
  void SetUp() override {
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
    InstallIsolatedWebApp(iwa_url(), "IWA Name");

    SiteSettingsHandlerBaseTest::SetUp();
  }

 protected:
  GURL iwa_url() {
    return GURL(
        "isolated-app://"
        "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic");
  }

  webapps::AppId InstallIsolatedWebApp(const GURL& iwa_url,
                                       const std::string& name) {
    webapps::AppId app_id =
        web_app::AddDummyIsolatedAppToRegistry(profile(), iwa_url, name);
    RegisterWebApp(profile(), MakeApp(app_id, apps::AppType::kWeb,
                                      iwa_url.spec(), apps::Readiness::kReady,
                                      apps::InstallReason::kUser));
    return app_id;
  }

  content::HostZoomMap* GetIwaHostZoomMap(const GURL& url) {
    auto url_info = *web_app::IsolatedWebAppUrlInfo::Create(url);
    content::StoragePartition* iwa_partition = profile()->GetStoragePartition(
        url_info.storage_partition_config(profile()));
    return content::HostZoomMap::GetForStoragePartition(iwa_partition);
  }
};

TEST_F(SiteSettingsHandlerIsolatedWebAppTest, AllSitesDisplaysAppName) {
  GURL https_url("https://" + iwa_url().host());

  SetupModelWithIsolatedWebAppData({{iwa_url().spec(), 50}});
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(iwa_url(), iwa_url(),
                                     ContentSettingsType::NOTIFICATIONS,
                                     CONTENT_SETTING_BLOCK);
  map->SetContentSettingDefaultScope(https_url, https_url,
                                     ContentSettingsType::NOTIFICATIONS,
                                     CONTENT_SETTING_BLOCK);

  base::Value::List site_groups = GetOnStorageFetchedSentList();

  ASSERT_EQ(site_groups.size(), 2u);
  const base::Value::Dict& group1 = site_groups[0].GetDict();
  const base::Value::Dict& origin1 =
      CHECK_DEREF(group1.FindList("origins"))[0].GetDict();
  EXPECT_THAT(CHECK_DEREF(group1.FindString("groupingKey")),
              IsOrigin(iwa_url()));
  EXPECT_EQ(group1.FindString("etldPlus1"), nullptr);
  EXPECT_EQ(CHECK_DEREF(group1.FindString("displayName")), "IWA Name");
  EXPECT_EQ(CHECK_DEREF(origin1.FindString("origin")), iwa_url());
  EXPECT_EQ(origin1.FindDouble("usage").value(), 50.0);

  const base::Value::Dict& group2 = site_groups[1].GetDict();
  const base::Value::Dict& origin2 =
      CHECK_DEREF(group2.FindList("origins"))[0].GetDict();
  EXPECT_THAT(CHECK_DEREF(group2.FindString("groupingKey")),
              IsEtldPlus1(iwa_url().host()));
  EXPECT_EQ(CHECK_DEREF(group2.FindString("etldPlus1")), iwa_url().host());
  EXPECT_EQ(CHECK_DEREF(group2.FindString("displayName")), iwa_url().host());
  EXPECT_EQ(CHECK_DEREF(origin2.FindString("origin")), https_url);
  EXPECT_EQ(origin2.FindDouble("usage").value(), 0.0);
}

TEST_F(SiteSettingsHandlerIsolatedWebAppTest, ZoomLevel) {
  content::HostZoomMap* iwa_host_zoom_map = GetIwaHostZoomMap(iwa_url());

  std::string host_or_spec = url::Origin::Create(iwa_url()).Serialize();
  iwa_host_zoom_map->SetZoomLevelForHost(iwa_url().host(), 1.1);
  ValidateZoom({{host_or_spec, "IWA Name", "122%"}}, 1U);

  base::Value::List args;
  handler()->HandleFetchZoomLevels(args);
  ValidateZoom({{host_or_spec, "IWA Name", "122%"}}, 2U);

  args.Append(host_or_spec);
  handler()->HandleRemoveZoomLevel(args);
  ValidateZoom({}, 3U);

  double default_level = iwa_host_zoom_map->GetDefaultZoomLevel();
  double level = iwa_host_zoom_map->GetZoomLevelForHostAndScheme(
      "isolated-app", iwa_url().host());
  EXPECT_EQ(default_level, level);
}

TEST_F(SiteSettingsHandlerIsolatedWebAppTest, ZoomLevelsSortedByAppName) {
  GetIwaHostZoomMap(iwa_url())->SetZoomLevelForHost(iwa_url().host(), 1.1);

  // Install 3 more IWAs.
  GURL iwa3_url(
      "isolated-app://"
      "cerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic");
  InstallIsolatedWebApp(iwa3_url, "IWA Name 3");
  GetIwaHostZoomMap(iwa3_url)->SetZoomLevelForHost(iwa3_url.host(), 1.1);

  GURL iwa2_url(
      "isolated-app://"
      "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic");
  InstallIsolatedWebApp(iwa2_url, "IWA Name 2");
  GetIwaHostZoomMap(iwa2_url)->SetZoomLevelForHost(iwa2_url.host(), 1.1);

  // Don't set a zoom for this app to make sure it's not in the list.
  GURL iwa4_url(
      "isolated-app://"
      "derugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic");
  InstallIsolatedWebApp(iwa4_url, "IWA Name 4");

  base::Value::List args;
  handler()->HandleFetchZoomLevels(args);

  ValidateZoom(
      {{url::Origin::Create(iwa_url()).Serialize(), "IWA Name", "122%"},
       {url::Origin::Create(iwa2_url).Serialize(), "IWA Name 2", "122%"},
       {url::Origin::Create(iwa3_url).Serialize(), "IWA Name 3", "122%"}},
      2U);
}

class SiteSettingsHandlerInfobarTest : public BrowserWithTestWindowTest {
 public:
  SiteSettingsHandlerInfobarTest() = default;
  SiteSettingsHandlerInfobarTest(const SiteSettingsHandlerInfobarTest&) =
      delete;
  SiteSettingsHandlerInfobarTest& operator=(
      const SiteSettingsHandlerInfobarTest&) = delete;
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    handler_ = std::make_unique<SiteSettingsHandler>(profile());
    handler()->set_web_ui(web_ui());
    handler()->AllowJavascript();
    web_ui()->ClearTrackedCalls();

    window2_ = CreateBrowserWindow();
    browser2_ =
        CreateBrowser(profile(), browser()->type(), false, window2_.get());
    window3_ = CreateBrowserWindow();

    // Creates the second profile used by this test.
    TestingProfile* profile2_ = profile_manager()->CreateTestingProfile(
        "testing_profile2@test", nullptr, std::u16string(), 0,
        GetTestingFactories());

    browser3_ =
        CreateBrowser(profile2_, browser()->type(), false, window3_.get());

    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile()));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
  }

  void TearDown() override {
    // SiteSettingsHandler maintains a HostZoomMap::Subscription internally and
    // has a PrefChangeRegistrar that observes the profile's preference, so make
    // sure that it's cleared before profile destruction.
    handler_.reset();

    // Also destroy `browser2_` before the profile.
    browser2()->tab_strip_model()->CloseAllTabs();
    browser2_.reset();

    // Destroy `browser3_`.
    browser3()->tab_strip_model()->CloseAllTabs();
    browser3_.reset();

    // Browser()'s destruction is handled in
    // BrowserWithTestWindowTest::TearDown()
    BrowserWithTestWindowTest::TearDown();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On ChromeOS a user account is needed in order to check whether the user
  // account is affiliated with the device owner for the purposes of applying
  // enterprise policy.
  void LogIn(const std::string& email) override {
    const AccountId account_id = AccountId::FromUserEmail(email);
    user_manager()->AddUserWithAffiliation(account_id, /*is_affiliated=*/true);
    ash_test_helper()->test_session_controller_client()->AddUserSession(email);
    user_manager()->UserLoggedIn(
        account_id,
        user_manager::FakeUserManager::GetFakeUsernameHash(account_id),
        /*browser_restart=*/false,
        /*is_child=*/false);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  infobars::ContentInfoBarManager* GetInfoBarManagerForTab(Browser* browser,
                                                           int tab_index,
                                                           GURL* tab_url) {
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetWebContentsAt(tab_index);
    if (tab_url)
      *tab_url = web_contents->GetLastCommittedURL();
    return infobars::ContentInfoBarManager::FromWebContents(web_contents);
  }

  content::TestWebUI* web_ui() { return &web_ui_; }

  SiteSettingsHandler* handler() { return handler_.get(); }

  Browser* browser2() { return browser2_.get(); }

  // browser3 is from a different profile `profile2_` than
  // browser2 and browser() which are from profile()
  Browser* browser3() { return browser3_.get(); }

  const std::string_view kNotifications =
      site_settings::ContentSettingsTypeToGroupName(
          ContentSettingsType::NOTIFICATIONS);

 private:
  content::TestWebUI web_ui_;
  std::unique_ptr<SiteSettingsHandler> handler_;
  std::unique_ptr<BrowserWindow> window2_;
  std::unique_ptr<Browser> browser2_;
  std::unique_ptr<BrowserWindow> window3_;
  std::unique_ptr<Browser> browser3_;
};

TEST_F(SiteSettingsHandlerInfobarTest, SettingPermissionsTriggersInfobar) {
  // Note all GURLs starting with 'origin' below belong to the same origin.
  //               _____  _______________  ________  ________  ___________
  //   Window 1:  / foo \' origin_anchor \' chrome \' origin \' extension \
  // -------------       -----------------------------------------------------
  std::string origin_anchor_string =
      "https://www.example.com/with/path/blah#heading";
  const GURL foo("http://foo");
  const GURL origin_anchor(origin_anchor_string);
  const GURL chrome("chrome://about");
  const GURL origin("https://www.example.com/");
  const GURL extension(
      "chrome-extension://fooooooooooooooooooooooooooooooo/bar.html");

  // Make sure |extension|'s extension ID exists before navigating to it. This
  // fixes a test timeout that occurs with --enable-browser-side-navigation on.
  scoped_refptr<const extensions::Extension> test_extension =
      extensions::ExtensionBuilder("Test")
          .SetID("fooooooooooooooooooooooooooooooo")
          .Build();
  extensions::ExtensionSystem::Get(profile())
      ->extension_service()
      ->AddExtension(test_extension.get());

  //               __________  ______________  ___________________  _______
  //   Window 2:  / insecure '/ origin_query \' example_subdomain \' about \
  // -------------------------                --------------------------------
  const GURL insecure("http://www.example.com/");
  const GURL origin_query("https://www.example.com/?param=value");
  const GURL example_subdomain("https://subdomain.example.com/");
  const GURL about(url::kAboutBlankURL);

  // Set up. Note AddTab() adds tab at index 0, so add them in reverse order.
  AddTab(browser(), extension);
  AddTab(browser(), origin);
  AddTab(browser(), chrome);
  AddTab(browser(), origin_anchor);
  AddTab(browser(), foo);
  for (int i = 0; i < browser()->tab_strip_model()->count(); ++i) {
    EXPECT_EQ(
        0u, GetInfoBarManagerForTab(browser(), i, nullptr)->infobars().size());
  }

  AddTab(browser2(), about);
  AddTab(browser2(), example_subdomain);
  AddTab(browser2(), origin_query);
  AddTab(browser2(), insecure);
  for (int i = 0; i < browser2()->tab_strip_model()->count(); ++i) {
    EXPECT_EQ(
        0u, GetInfoBarManagerForTab(browser2(), i, nullptr)->infobars().size());
  }

  // Block notifications.
  base::Value::List set_args;
  set_args.Append(origin_anchor_string);
  set_args.Append(kNotifications);
  set_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  handler()->HandleSetOriginPermissions(set_args);

  // Make sure all tabs belonging to the same origin as |origin_anchor| have an
  // infobar shown.
  GURL tab_url;
  for (int i = 0; i < browser()->tab_strip_model()->count(); ++i) {
    if (i == /*origin_anchor=*/1 || i == /*origin=*/3) {
      EXPECT_EQ(
          1u,
          GetInfoBarManagerForTab(browser(), i, &tab_url)->infobars().size());
      EXPECT_TRUE(url::IsSameOriginWith(origin, tab_url));
    } else {
      EXPECT_EQ(
          0u,
          GetInfoBarManagerForTab(browser(), i, &tab_url)->infobars().size());
      EXPECT_FALSE(url::IsSameOriginWith(origin, tab_url));
    }
  }
  for (int i = 0; i < browser2()->tab_strip_model()->count(); ++i) {
    if (i == /*origin_query=*/1) {
      EXPECT_EQ(
          1u,
          GetInfoBarManagerForTab(browser2(), i, &tab_url)->infobars().size());
      EXPECT_TRUE(url::IsSameOriginWith(origin, tab_url));
    } else {
      EXPECT_EQ(
          0u,
          GetInfoBarManagerForTab(browser2(), i, &tab_url)->infobars().size());
      EXPECT_FALSE(url::IsSameOriginWith(origin, tab_url));
    }
  }

  // Navigate the |foo| tab to the same origin as |origin_anchor|, and the
  // |origin_query| tab to a different origin.
  const GURL origin_path("https://www.example.com/path/to/page.html");
  content::WebContents* foo_contents =
      browser()->tab_strip_model()->GetWebContentsAt(/*index=*/0);
  NavigateAndCommit(foo_contents, origin_path);

  const GURL example_without_www("https://example.com/");
  content::WebContents* origin_query_contents =
      browser2()->tab_strip_model()->GetWebContentsAt(/*index=*/1);
  NavigateAndCommit(origin_query_contents, example_without_www);

  // Reset all permissions.
  base::Value::List reset_args;
  reset_args.Append(origin_anchor_string);
  base::Value::List category_list;
  category_list.Append(kNotifications);
  reset_args.Append(std::move(category_list));
  reset_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_DEFAULT));
  handler()->HandleSetOriginPermissions(reset_args);

  // Check the same tabs (plus the tab navigated to |origin_path|) still have
  // infobars showing.
  for (int i = 0; i < browser()->tab_strip_model()->count(); ++i) {
    if (i == /*origin_path=*/0 || i == /*origin_anchor=*/1 ||
        i == /*origin=*/3) {
      EXPECT_EQ(
          1u,
          GetInfoBarManagerForTab(browser(), i, &tab_url)->infobars().size());
      EXPECT_TRUE(url::IsSameOriginWith(origin, tab_url));
    } else {
      EXPECT_EQ(
          0u,
          GetInfoBarManagerForTab(browser(), i, &tab_url)->infobars().size());
      EXPECT_FALSE(url::IsSameOriginWith(origin, tab_url));
    }
  }
  // The infobar on the original |origin_query| tab (which has now been
  // navigated to |example_without_www|) should disappear.
  for (int i = 0; i < browser2()->tab_strip_model()->count(); ++i) {
    EXPECT_EQ(
        0u,
        GetInfoBarManagerForTab(browser2(), i, &tab_url)->infobars().size());
    EXPECT_FALSE(url::IsSameOriginWith(origin, tab_url));
  }

  // Make sure it's the correct infobar that's being shown.
  EXPECT_EQ(infobars::InfoBarDelegate::PAGE_INFO_INFOBAR_DELEGATE,
            GetInfoBarManagerForTab(browser(), /*tab_index=*/0, &tab_url)
                ->infobars()[0]
                ->delegate()
                ->GetIdentifier());
  EXPECT_TRUE(url::IsSameOriginWith(origin, tab_url));
}

TEST_F(SiteSettingsHandlerInfobarTest,
       SettingPermissionsDoesNotTriggerInfobarOnDifferentProfile) {
  // Note all GURLs starting with 'origin' below belong to the same origin.
  //               _______________
  //   Window 1:  / origin_anchor \
  // -------------       -----------------------------------------------------
  const GURL origin("https://www.example.com/");
  std::string origin_anchor_string =
      "https://www.example.com/with/path/blah#heading";
  const GURL origin_anchor(origin_anchor_string);

  //   Different
  //   Profile (2) ______________
  //   Window 3:  / origin_query \
  // -------------------------------------------------------------------------
  const GURL origin_query("https://www.example.com/?param=value");

  // Set up. No info bars.
  AddTab(browser(), origin_anchor);
  EXPECT_EQ(0u,
            GetInfoBarManagerForTab(browser(), 0, nullptr)->infobars().size());

  AddTab(browser3(), origin_query);
  EXPECT_EQ(0u,
            GetInfoBarManagerForTab(browser3(), 0, nullptr)->infobars().size());

  // Block notifications.
  base::Value::List set_args;
  set_args.Append(origin_anchor_string);
  set_args.Append(kNotifications);
  set_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  handler()->HandleSetOriginPermissions(set_args);

  // Make sure all tabs within the same profile belonging to the same origin
  // as `origin_anchor` have an infobar shown.
  GURL tab_url;
  EXPECT_EQ(1u,
            GetInfoBarManagerForTab(browser(), 0, &tab_url)->infobars().size());
  EXPECT_TRUE(url::IsSameOriginWith(origin, tab_url));

  // Make sure all tabs with the same origin as `origin_anchor` that don't
  // belong to the same profile don't have an infobar shown
  EXPECT_EQ(
      0u, GetInfoBarManagerForTab(browser3(), 0, &tab_url)->infobars().size());
  EXPECT_TRUE(url::IsSameOriginWith(origin, tab_url));
}

TEST_F(SiteSettingsHandlerTest, BlockAutoplay_SendOnRequest) {
  base::Value::List args;
  handler()->HandleFetchBlockAutoplayStatus(args);

  // Check that we are checked and enabled.
  ValidateBlockAutoplay(true, true);
}

TEST_F(SiteSettingsHandlerTest, BlockAutoplay_SoundSettingUpdate) {
  SetSoundContentSettingDefault(CONTENT_SETTING_BLOCK);
  base::RunLoop().RunUntilIdle();

  // Check that we are not checked or enabled.
  ValidateBlockAutoplay(false, false);

  SetSoundContentSettingDefault(CONTENT_SETTING_ALLOW);
  base::RunLoop().RunUntilIdle();

  // Check that we are checked and enabled.
  ValidateBlockAutoplay(true, true);
}

TEST_F(SiteSettingsHandlerTest, BlockAutoplay_PrefUpdate) {
  profile()->GetPrefs()->SetBoolean(prefs::kBlockAutoplayEnabled, false);
  base::RunLoop().RunUntilIdle();

  // Check that we are not checked but are enabled.
  ValidateBlockAutoplay(false, true);

  profile()->GetPrefs()->SetBoolean(prefs::kBlockAutoplayEnabled, true);
  base::RunLoop().RunUntilIdle();

  // Check that we are checked and enabled.
  ValidateBlockAutoplay(true, true);
}

TEST_F(SiteSettingsHandlerTest, BlockAutoplay_Update) {
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kBlockAutoplayEnabled));

  base::Value::List data;
  data.Append(false);

  handler()->HandleSetBlockAutoplayEnabled(data);
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kBlockAutoplayEnabled));
}

TEST_F(SiteSettingsHandlerTest, ExcludeWebUISchemesInLists) {
  SetupModel();
  const ContentSettingsType content_settings_type =
      ContentSettingsType::NOTIFICATIONS;
  // Register WebUIAllowlist auto-granted permissions.
  const url::Origin kWebUIOrigins[] = {
      url::Origin::Create(GURL("chrome://test")),
      url::Origin::Create(GURL("chrome-untrusted://test")),
      url::Origin::Create(GURL("devtools://devtools")),
  };

  WebUIAllowlist* allowlist = WebUIAllowlist::GetOrCreate(profile());
  for (const url::Origin& origin : kWebUIOrigins)
    allowlist->RegisterAutoGrantedPermission(origin, content_settings_type);

  // Verify the auto-granted permissions are registered, and they are indeed
  // provided by WebUIAllowlist.
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  content_settings::SettingInfo info;
  base::Value value = map->GetWebsiteSetting(kWebUIOrigins[0].GetURL(),
                                             kWebUIOrigins[0].GetURL(),
                                             content_settings_type, &info);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, value.GetInt());
  EXPECT_EQ(content_settings::SettingSource::kAllowList, info.source);

  // Register an ordinary website permission.
  const GURL kWebUrl = GURL("https://example.com");
  map->SetContentSettingDefaultScope(kWebUrl, kWebUrl, content_settings_type,
                                     CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(kWebUrl, kWebUrl, content_settings_type));

  // GetAllSites() only returns website exceptions.
  {
    base::Value::List get_all_sites_args;
    get_all_sites_args.Append(kCallbackId);

    handler()->HandleGetAllSites(get_all_sites_args);

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    const base::Value::List& site_groups = data.arg3()->GetList();
    EXPECT_EQ(1UL, site_groups.size());
    const base::Value::Dict& first_site_group = site_groups[0].GetDict();

    EXPECT_THAT(CHECK_DEREF(first_site_group.FindString("groupingKey")),
                IsEtldPlus1("example.com"));
    const base::Value::List& origin_list =
        CHECK_DEREF(first_site_group.FindList("origins"));
    EXPECT_EQ(1UL, origin_list.size());
    EXPECT_EQ(kWebUrl.spec(),
              CHECK_DEREF(origin_list[0].GetDict().FindString("origin")));
  }

  // GetExceptionList() only returns website exceptions.
  {
    base::Value::List get_exception_list_args;
    get_exception_list_args.Append(kCallbackId);
    get_exception_list_args.Append(kNotifications);

    handler()->HandleGetExceptionList(get_exception_list_args);

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    const base::Value::List& exception_list = data.arg3()->GetList();
    EXPECT_EQ(1UL, exception_list.size());
    EXPECT_EQ("https://example.com:443",
              CHECK_DEREF(exception_list[0].GetDict().FindString("origin")));
  }

  // GetRecentSitePermissions() only returns website exceptions.
  {
    base::Value::List get_recent_permissions_args;
    get_recent_permissions_args.Append(kCallbackId);
    get_recent_permissions_args.Append(3);

    handler()->HandleGetRecentSitePermissions(get_recent_permissions_args);

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    const base::Value::List& recent_permission_list = data.arg3()->GetList();
    EXPECT_EQ(1UL, recent_permission_list.size());
    EXPECT_EQ(
        kWebUrl.spec(),
        CHECK_DEREF(recent_permission_list[0].GetDict().FindString("origin")));
  }
}

// GetOriginPermissions() returns the allowlisted exception. We explicitly
// return this, so developers can easily test things (e.g. by navigating to
// chrome://settings/content/siteDetails?site=chrome://example).
TEST_F(SiteSettingsHandlerTest, IncludeWebUISchemesInGetOriginPermissions) {
  const ContentSettingsType content_settings_type =
      ContentSettingsType::NOTIFICATIONS;

  // Register WebUIAllowlist auto-granted permissions.
  const url::Origin kWebUIOrigins[] = {
      url::Origin::Create(GURL("chrome://test")),
      url::Origin::Create(GURL("chrome-untrusted://test")),
      url::Origin::Create(GURL("devtools://devtools")),
  };

  WebUIAllowlist* allowlist = WebUIAllowlist::GetOrCreate(profile());
  for (const url::Origin& origin : kWebUIOrigins)
    allowlist->RegisterAutoGrantedPermission(origin, content_settings_type);

  for (const url::Origin& origin : kWebUIOrigins) {
    base::Value::List get_origin_permissions_args;
    get_origin_permissions_args.Append(kCallbackId);
    get_origin_permissions_args.Append(origin.GetURL().spec());
    base::Value::List category_list;
    category_list.Append(kNotifications);
    get_origin_permissions_args.Append(std::move(category_list));

    handler()->HandleGetOriginPermissions(get_origin_permissions_args);
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    const base::Value::List& exception_list = data.arg3()->GetList();
    EXPECT_EQ(1UL, exception_list.size());
    const base::Value::Dict& first_exception = exception_list[0].GetDict();

    EXPECT_EQ(origin.GetURL().spec(),
              CHECK_DEREF(first_exception.FindString("origin")));
    EXPECT_EQ("allowlist", CHECK_DEREF(first_exception.FindString("source")));
  }
}

TEST_F(SiteSettingsHandlerTest, StorageAccessExceptions_DiffPatterns) {
  const std::string kOrigin("https://[*.]google.com:443");
  const std::string kDisplayName("google.com");

  const std::string kOrigin2("https://[*.]google2.com:443");
  const std::string kDisplayName2("google2.com");

  const std::string kEmbeddingOrigin("https://[*.]example.com:443");
  const std::string kEmbeddingDisplayName("example.com");

  const std::string kEmbeddingOrigin2("https://[*.]example2.com:443");
  const std::string kEmbeddingDisplayName2("example2.com");

  SetContentSettingCustomScope(kOrigin, kEmbeddingOrigin,
                               kPermissionStorageAccess, CONTENT_SETTING_BLOCK);
  SetContentSettingCustomScope(kOrigin2, kEmbeddingOrigin2,
                               kPermissionStorageAccess, CONTENT_SETTING_BLOCK,
                               /*expected_total_calls=*/2U);

  base::Value::List get_exception_list_args;
  get_exception_list_args.Append(kCallbackId);
  get_exception_list_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  handler()->HandleGetStorageAccessExceptionList(get_exception_list_args);

  // Verify that the exception list is correct.
  ValidateStorageAccessList(/*expected_total_calls=*/3U,
                            /*expected_num_groups=*/2U);

  // Verify that the first group exception is correct.
  ValidateStorageAccessException(
      kOrigin, kDisplayName, CONTENT_SETTING_BLOCK,
      {{kEmbeddingOrigin, kEmbeddingDisplayName, /*incognito=*/false}},
      /*index=*/0U);

  // Verify that the second group exception is correct.
  ValidateStorageAccessException(
      kOrigin2, kDisplayName2, CONTENT_SETTING_BLOCK,
      {{kEmbeddingOrigin2, kEmbeddingDisplayName2, /*incognito=*/false}},
      /*index=*/1U);
}

TEST_F(SiteSettingsHandlerTest, StorageAccessExceptions_SamePrimaryPattern) {
  const std::string kOrigin("https://[*.]google.com:443");
  const std::string kDisplayName("google.com");

  const std::string kEmbeddingOrigin("https://[*.]example.com:443");
  const std::string kEmbeddingDisplayName("example.com");

  const std::string kEmbeddingOrigin2("https://[*.]example2.com:443");
  const std::string kEmbeddingDisplayName2("example2.com");

  SetContentSettingCustomScope(kOrigin, kEmbeddingOrigin,
                               kPermissionStorageAccess, CONTENT_SETTING_BLOCK);
  SetContentSettingCustomScope(kOrigin, kEmbeddingOrigin2,
                               kPermissionStorageAccess, CONTENT_SETTING_BLOCK,
                               /*expected_total_calls=*/2U);

  base::Value::List get_exception_list_args;
  get_exception_list_args.Append(kCallbackId);
  get_exception_list_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  handler()->HandleGetStorageAccessExceptionList(get_exception_list_args);

  // Verify that the group exception is correct.
  ValidateStorageAccessList(/*expected_total_calls=*/3U,
                            /*expected_num_groups=*/1U);

  ValidateStorageAccessException(
      kOrigin, kDisplayName, CONTENT_SETTING_BLOCK,
      {{kEmbeddingOrigin2, kEmbeddingDisplayName2, /*incognito=*/false},
       {kEmbeddingOrigin, kEmbeddingDisplayName, /*incognito=*/false}},
      /*index=*/0U);
}

TEST_F(SiteSettingsHandlerTest, StorageAccessExceptions_DiffType) {
  const std::string kOrigin("https://[*.]google.com:443");
  const std::string kEmbeddingOrigin("https://[*.]example.com:443");

  SetContentSettingCustomScope(kOrigin, kEmbeddingOrigin,
                               kPermissionStorageAccess, CONTENT_SETTING_BLOCK);

  base::Value::List get_exception_list_args;
  get_exception_list_args.Append(kCallbackId);
  get_exception_list_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_ALLOW));
  handler()->HandleGetStorageAccessExceptionList(get_exception_list_args);

  // Verify that no exception is returned.
  ValidateNoOrigin(2U);
}

TEST_F(SiteSettingsHandlerTest, StorageAccessExceptions_AutoGranted) {
  const std::string kOrigin("https://[*.]google.com:443");
  const std::string kEmbeddingOrigin("https://[*.]example.com:443");

  SetContentSettingCustomScope(
      kOrigin, kEmbeddingOrigin, kPermissionStorageAccess,
      CONTENT_SETTING_BLOCK, /*expected_total_calls=*/1U,
      /*is_incognito=*/false, /*lifetime=*/base::TimeDelta(),
      /*is_auto_granted=*/true);

  base::Value::List get_exception_list_args;
  get_exception_list_args.Append(kCallbackId);
  get_exception_list_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  handler()->HandleGetStorageAccessExceptionList(get_exception_list_args);

  // Verify that no exception is returned since auto granted exceptions should
  // not be returned.
  ValidateNoOrigin(2U);
}

TEST_F(SiteSettingsHandlerTest, StorageAccessExceptions_Incognito) {
  const std::string kOrigin("https://[*.]google.com:443");
  const std::string kDisplayName("google.com");

  const std::string kEmbeddingOrigin("https://[*.]example.com:443");
  const std::string kEmbeddingDisplayName("example.com");

  CreateIncognitoProfile();
  ValidateIncognitoExists(true, 1U);

  SetContentSettingCustomScope(kOrigin, kEmbeddingOrigin,
                               kPermissionStorageAccess, CONTENT_SETTING_BLOCK,
                               /*expected_total_calls=*/2U,
                               /*is_incognito=*/true);

  base::Value::List get_exception_list_args;
  get_exception_list_args.Append(kCallbackId);
  get_exception_list_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  handler()->HandleGetStorageAccessExceptionList(get_exception_list_args);

  // Verify that the group exception is correct.
  ValidateStorageAccessList(/*expected_total_calls=*/3U,
                            /*expected_num_groups=*/1U);
  ValidateStorageAccessException(
      kOrigin, kDisplayName, CONTENT_SETTING_BLOCK,
      {{kEmbeddingOrigin, kEmbeddingDisplayName, /*incognito=*/true}});
}

TEST_F(SiteSettingsHandlerTest, StorageAccessExceptions_NormalAndIncognito) {
  const std::string kOrigin("https://[*.]google.com:443");
  const std::string kDisplayName("google.com");

  const std::string kOrigin2("https://[*.]google2.com:443");
  const std::string kDisplayName2("google2.com");

  const std::string kEmbeddingOrigin("https://[*.]example.com:443");
  const std::string kEmbeddingDisplayName("example.com");

  const std::string kEmbeddingOrigin2("https://[*.]example2.com:443");
  const std::string kEmbeddingDisplayName2("example2.com");

  SetContentSettingCustomScope(kOrigin, kEmbeddingOrigin,
                               kPermissionStorageAccess, CONTENT_SETTING_BLOCK);

  CreateIncognitoProfile();
  ValidateIncognitoExists(true, 2U);

  SetContentSettingCustomScope(kOrigin2, kEmbeddingOrigin2,
                               kPermissionStorageAccess, CONTENT_SETTING_BLOCK,
                               /*expected_total_calls=*/3U,
                               /*is_incognito=*/true);

  base::Value::List get_exception_list_args;
  get_exception_list_args.Append(kCallbackId);
  get_exception_list_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  handler()->HandleGetStorageAccessExceptionList(get_exception_list_args);

  ValidateStorageAccessList(/*expected_total_calls=*/4U,
                            /*expected_num_groups=*/2U);

  // Verify that group exception for non-incognito is correct.
  ValidateStorageAccessException(
      kOrigin, kDisplayName, CONTENT_SETTING_BLOCK,
      {{kEmbeddingOrigin, kEmbeddingDisplayName, /*incognito=*/false}},
      /*index=*/0U);

  // Verify that group exception for incognito is correct.
  ValidateStorageAccessException(
      kOrigin2, kDisplayName2, CONTENT_SETTING_BLOCK,
      {{kEmbeddingOrigin2, kEmbeddingDisplayName2, /*incognito=*/true}},
      /*index=*/1U);

  DestroyIncognitoProfile();
  handler()->HandleGetStorageAccessExceptionList(get_exception_list_args);

  // Check that the incognito exception gets deleted if the incognito profile is
  // destroyed.
  ValidateStorageAccessList(/*expected_total_calls=*/6U,
                            /*expected_num_groups=*/1U);
  ValidateStorageAccessException(
      kOrigin, kDisplayName, CONTENT_SETTING_BLOCK,
      {{kEmbeddingOrigin, kEmbeddingDisplayName, /*incognito=*/false}},
      /*index=*/0U);
}

TEST_F(SiteSettingsHandlerTest,
       StorageAccessExceptions_NormalAndIncognito_SamePatterns) {
  const std::string kOrigin("https://[*.]google.com:443");
  const std::string kDisplayName("google.com");

  const std::string kEmbeddingOrigin("https://[*.]example.com:443");
  const std::string kEmbeddingDisplayName("example.com");

  SetContentSettingCustomScope(kOrigin, kEmbeddingOrigin,
                               kPermissionStorageAccess, CONTENT_SETTING_BLOCK);

  CreateIncognitoProfile();
  ValidateIncognitoExists(true, 2U);

  SetContentSettingCustomScope(kOrigin, kEmbeddingOrigin,
                               kPermissionStorageAccess, CONTENT_SETTING_BLOCK,
                               /*expected_total_calls=*/3U,
                               /*is_incognito=*/true);

  base::Value::List get_exception_list_args;
  get_exception_list_args.Append(kCallbackId);
  get_exception_list_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  handler()->HandleGetStorageAccessExceptionList(get_exception_list_args);

  // Verify that group exception for non-incognito and incognito is correct.
  ValidateStorageAccessList(/*expected_total_calls=*/4U,
                            /*expected_num_groups=*/1U);
  ValidateStorageAccessException(
      kOrigin, kDisplayName, CONTENT_SETTING_BLOCK,
      {{kEmbeddingOrigin, kEmbeddingDisplayName, /*incognito=*/false},
       {kEmbeddingOrigin, kEmbeddingDisplayName, /*incognito=*/true}});
}

TEST_F(SiteSettingsHandlerTest, StorageAccessExceptions_Extension) {
  auto extension = LoadExtension(kExtensionName);
  auto extension_url = extension->url().spec();

  const std::string kEmbeddingOrigin("https://[*.]example.com:443");
  const std::string kEmbeddingDisplayName("example.com");

  SetContentSettingCustomScope(extension_url, kEmbeddingOrigin,
                               kPermissionStorageAccess, CONTENT_SETTING_BLOCK);

  base::Value::List get_exception_list_args;
  get_exception_list_args.Append(kCallbackId);
  get_exception_list_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  handler()->HandleGetStorageAccessExceptionList(get_exception_list_args);

  // Verify that the grouped exception is correct.
  ValidateStorageAccessList(/*expected_total_calls=*/2U,
                            /*expected_num_groups=*/1U);
  ValidateStorageAccessException(
      extension_url, kExtensionName, CONTENT_SETTING_BLOCK,
      {{kEmbeddingOrigin, kEmbeddingDisplayName, /*incognito=*/false}});

  // When the extension is unloaded, the display name should be the plain url.
  UnloadExtension(extension->id());
  // Display name does not have a trailing '/'.
  const auto extensionDisplayName =
      extension_url.substr(0, extension_url.length() - 1);

  handler()->HandleGetStorageAccessExceptionList(get_exception_list_args);
  ValidateStorageAccessList(/*expected_total_calls=*/3U,
                            /*expected_num_groups=*/1U);

  ValidateStorageAccessException(
      extension_url, extensionDisplayName, CONTENT_SETTING_BLOCK,
      {{kEmbeddingOrigin, kEmbeddingDisplayName, /*incognito=*/false}});
}

TEST_F(SiteSettingsHandlerTest, StorageAccessExceptions_Description_Embargoed) {
  const std::string kOrigin("https://google.com:443");
  const std::string kDisplayName("google.com");

  // Set an embargoed setting.
  permissions::PermissionDecisionAutoBlocker* auto_blocker =
      PermissionDecisionAutoBlockerFactory::GetForProfile(profile());
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  auto_blocker->SetClockForTesting(&clock);
  for (int i = 0; i < 3; ++i) {
    auto_blocker->RecordDismissAndEmbargo(GURL(kOrigin),
                                          kPermissionStorageAccess, false);
  }
  EXPECT_EQ(
      PermissionStatus::DENIED,
      auto_blocker->GetEmbargoResult(GURL(kOrigin), kPermissionStorageAccess)
          ->status);

  base::Value::List get_exception_list_args;
  get_exception_list_args.Append(kCallbackId);
  get_exception_list_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  handler()->HandleGetStorageAccessExceptionList(get_exception_list_args);

  // Verify that group exception with an embargoed is correct.
  ValidateStorageAccessList(/*expected_total_calls=*/1U,
                            /*expected_num_groups=*/1U);
  ValidateStaticStorageAccessException(kOrigin, kDisplayName,
                                       CONTENT_SETTING_BLOCK,
                                       /*expected_incognito=*/false);
}

TEST_F(SiteSettingsHandlerTest,
       StorageAccessExceptions_Description_EmbargoedTwoProfiles) {
  const std::string kOrigin("https://google.com:443");
  const std::string kDisplayName("google.com");

  // Set an embargoed setting for regular and incognito profile
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());

  permissions::PermissionDecisionAutoBlocker* auto_blocker =
      PermissionDecisionAutoBlockerFactory::GetForProfile(profile());
  auto_blocker->SetClockForTesting(&clock);
  for (int i = 0; i < 3; ++i) {
    auto_blocker->RecordDismissAndEmbargo(GURL(kOrigin),
                                          kPermissionStorageAccess, false);
  }
  EXPECT_EQ(
      PermissionStatus::DENIED,
      auto_blocker->GetEmbargoResult(GURL(kOrigin), kPermissionStorageAccess)
          ->status);

  CreateIncognitoProfile();
  permissions::PermissionDecisionAutoBlocker* auto_blocker_incognito =
      PermissionDecisionAutoBlockerFactory::GetForProfile(incognito_profile());
  auto_blocker_incognito->SetClockForTesting(&clock);
  for (int i = 0; i < 3; ++i) {
    auto_blocker_incognito->RecordDismissAndEmbargo(
        GURL(kOrigin), kPermissionStorageAccess, false);
  }
  EXPECT_EQ(PermissionStatus::DENIED,
            auto_blocker_incognito
                ->GetEmbargoResult(GURL(kOrigin), kPermissionStorageAccess)
                ->status);

  base::Value::List get_exception_list_args;
  get_exception_list_args.Append(kCallbackId);
  get_exception_list_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  handler()->HandleGetStorageAccessExceptionList(get_exception_list_args);

  // Verify that group exception with an embargoed origin in two profiles is
  // correct.
  ValidateStorageAccessList(/*expected_total_calls=*/2U,
                            /*expected_num_groups=*/1U);
  ValidateStorageAccessException(kOrigin, kDisplayName, CONTENT_SETTING_BLOCK,
                                 {{std::string(), "*", /*incognito=*/false,
                                   /*embargoed=*/true, 0},
                                  {std::string(), "*", /*incognito=*/true,
                                   /*embargoed=*/true, 0}});
}

typedef std::pair<std::string_view, std::string_view> OriginStringParams;
class StorageAccessSiteSettingsHandlerTest
    : public SiteSettingsHandlerBaseTest,
      public testing::WithParamInterface<
          std::tuple<OriginStringParams, OriginStringParams>> {};

// Several pairs <origin, displayName> to test on both the embedded and
// embedding.
constexpr OriginStringParams kOrigins[] = {
    {"192.168.0.1", "192.168.0.1"},
    {"google.com", "google.com"},
    {"google.com:443", "google.com"},
    {"docs.google.com:443", "docs.google.com"},
    {"https://[*.]google.com", "google.com"},
    {"https://[*.]docs.example.com:443", "docs.example.com"},
    {"chrome-extension://peoadpeiejnhkmpaakpnompolbglelel/",
     "chrome-extension://peoadpeiejnhkmpaakpnompolbglelel"}};

INSTANTIATE_TEST_SUITE_P(All,
                         StorageAccessSiteSettingsHandlerTest,
                         testing::Combine(testing::ValuesIn(kOrigins),
                                          testing::ValuesIn(kOrigins)));

TEST_P(StorageAccessSiteSettingsHandlerTest, StorageAccessExceptions_Origins) {
  OriginStringParams embedded = std::get<0>(GetParam());
  const std::string kOrigin(std::get<0>(embedded));
  const std::string kDisplayName(std::get<1>(embedded));

  OriginStringParams embedding = std::get<1>(GetParam());
  const std::string kEmbeddingOrigin(std::get<0>(embedding));
  const std::string kEmbeddingDisplayName(std::get<1>(embedding));

  SetContentSettingCustomScope(kOrigin, kEmbeddingOrigin,
                               kPermissionStorageAccess, CONTENT_SETTING_BLOCK);

  base::Value::List get_exception_list_args;
  get_exception_list_args.Append(kCallbackId);
  get_exception_list_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  handler()->HandleGetStorageAccessExceptionList(get_exception_list_args);

  // Verify that the grouped exception is correct.
  ValidateStorageAccessList(/*expected_total_calls=*/2U,
                            /*expected_num_groups=*/1U);

  ValidateStorageAccessException(
      kOrigin, kDisplayName, CONTENT_SETTING_BLOCK,
      {{kEmbeddingOrigin, kEmbeddingDisplayName, /*incognito=*/false}});
}

class StorageAccessSiteSettingsHandlerLifetimeTest
    : public SiteSettingsHandlerBaseTest,
      public testing::WithParamInterface<int> {};

// Range for the lifetime in days of a Storage Access permission.
INSTANTIATE_TEST_SUITE_P(All,
                         StorageAccessSiteSettingsHandlerLifetimeTest,
                         testing::Range(0, 3));

TEST_P(StorageAccessSiteSettingsHandlerLifetimeTest,
       StorageAccessExceptions_Description) {
  const std::string kOrigin("https://[*.]google.com:443");
  const std::string kDisplayName("google.com");

  const std::string kEmbeddingOrigin("https://[*.]example.com:443");
  const std::string kEmbeddingDisplayName("example.com");

  const int kLifetimeInDays = GetParam();

  SetContentSettingCustomScope(kOrigin, kEmbeddingOrigin,
                               kPermissionStorageAccess, CONTENT_SETTING_BLOCK,
                               /*expected_total_calls=*/1U,
                               /*is_incognito=*/false,
                               base::Days(kLifetimeInDays));

  base::Value::List get_exception_list_args;
  get_exception_list_args.Append(kCallbackId);
  get_exception_list_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  handler()->HandleGetStorageAccessExceptionList(get_exception_list_args);

  // Verify that group exception description with expiration is correct.
  ValidateStorageAccessList(/*expected_total_calls=*/2U,
                            /*expected_num_groups=*/1U);
  ValidateStorageAccessException(
      kOrigin, kDisplayName, CONTENT_SETTING_BLOCK,
      {{kEmbeddingOrigin, kEmbeddingDisplayName, /*incognito=*/false,
        /*embargoed=*/false, kLifetimeInDays}});
}

class PersistentPermissionsSiteSettingsHandlerTest
    : public SiteSettingsHandlerBaseTest {
  void SetUp() override {
    SiteSettingsHandlerBaseTest::SetUp();
    handler_ = std::make_unique<SiteSettingsHandler>(&profile_);
    handler_->set_web_ui(web_ui());
    handler_->AllowJavascript();
    web_ui()->ClearTrackedCalls();
  }

  void TearDown() override { handler_->DisallowJavascript(); }

 public:
  PersistentPermissionsSiteSettingsHandlerTest() {
    feature_list_.InitAndEnableFeature(
        features::kFileSystemAccessPersistentPermissions);
  }

 protected:
  TestingProfile profile_;
  std::unique_ptr<SiteSettingsHandler> handler_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// GetFileSystemGrants() returns the allowed grants for a given origin
// based on the File System Access persistent permissions policy.
TEST_F(PersistentPermissionsSiteSettingsHandlerTest,
       HandleGetFileSystemGrants) {
  ChromeFileSystemAccessPermissionContext* context =
      FileSystemAccessPermissionContextFactory::GetForProfile(&profile_);

  auto kTestOrigin1 = url::Origin::Create(GURL("https://www.a.com"));
  auto kTestOrigin2 = url::Origin::Create(GURL("https://www.b.com"));

  const base::FilePath kTestPath = base::FilePath(FILE_PATH_LITERAL("/a/b"));
  const base::FilePath kTestPath2 = base::FilePath(FILE_PATH_LITERAL("/c/d"));
  const base::FilePath kTestPath3 = base::FilePath(FILE_PATH_LITERAL("/e/"));
  const base::FilePath kTestPath4 =
      base::FilePath(FILE_PATH_LITERAL("/f/g/h/"));

  context->SetOriginHasExtendedPermissionForTesting(kTestOrigin1);
  context->SetOriginHasExtendedPermissionForTesting(kTestOrigin2);

  // Populate the `grants` object with permissions.
  auto file_read_grant = context->GetExtendedReadPermissionGrantForTesting(
      kTestOrigin1, content::PathInfo(kTestPath),
      ChromeFileSystemAccessPermissionContext::HandleType::kFile);
  auto file_write_grant = context->GetExtendedWritePermissionGrantForTesting(
      kTestOrigin2, content::PathInfo(kTestPath2),
      ChromeFileSystemAccessPermissionContext::HandleType::kFile);
  auto directory_read_grant = context->GetExtendedReadPermissionGrantForTesting(
      kTestOrigin1, content::PathInfo(kTestPath3),
      ChromeFileSystemAccessPermissionContext::HandleType::kDirectory);
  auto directory_write_grant =
      context->GetExtendedWritePermissionGrantForTesting(
          kTestOrigin2, content::PathInfo(kTestPath4),
          ChromeFileSystemAccessPermissionContext::HandleType::kDirectory);

  auto kTestOrigin1Grants =
      context->ConvertObjectsToGrants(context->GetGrantedObjects(kTestOrigin1));
  auto kTestOrigin2Grants =
      context->ConvertObjectsToGrants(context->GetGrantedObjects(kTestOrigin2));
  EXPECT_EQ(kTestOrigin1Grants.file_read_grants.size(), 1UL);
  EXPECT_EQ(kTestOrigin2Grants.file_read_grants.size(), 0UL);
  EXPECT_EQ(kTestOrigin1Grants.file_write_grants.size(), 0UL);
  EXPECT_EQ(kTestOrigin2Grants.file_write_grants.size(), 1UL);
  EXPECT_EQ(kTestOrigin1Grants.directory_read_grants.size(), 1UL);
  EXPECT_EQ(kTestOrigin2Grants.directory_read_grants.size(), 0UL);
  EXPECT_EQ(kTestOrigin1Grants.directory_write_grants.size(), 0UL);
  EXPECT_EQ(kTestOrigin2Grants.directory_write_grants.size(), 1UL);

  base::Value::List get_file_system_permissions_args;
  get_file_system_permissions_args.Append(kCallbackId);

  handler_->HandleGetFileSystemGrants(get_file_system_permissions_args);
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  const base::Value::List& grants = data.arg3()->GetList();

  EXPECT_EQ(grants.size(), 2UL);
  const base::Value::Dict& first_grant = grants[0].GetDict();
  const base::Value::Dict& second_grant = grants[1].GetDict();
  EXPECT_EQ(CHECK_DEREF(first_grant.FindString(site_settings::kOrigin)),
            "https://www.a.com/");
  EXPECT_EQ(CHECK_DEREF(second_grant.FindString(site_settings::kOrigin)),
            "https://www.b.com/");

  const base::Value::List* kTestOrigin1ViewGrants =
      first_grant.FindList(site_settings::kFileSystemViewGrants);
  const base::Value::List* kTestOrigin1EditGrants =
      first_grant.FindList(site_settings::kFileSystemEditGrants);

  const base::Value::List* kTestOrigin2ViewGrants =
      second_grant.FindList(site_settings::kFileSystemViewGrants);
  const base::Value::List* kTestOrigin2EditGrants =
      second_grant.FindList(site_settings::kFileSystemEditGrants);

  // Checks that the grants for test origins are populated as expected.
  EXPECT_TRUE(CHECK_DEREF(kTestOrigin1ViewGrants)[0]
                  .GetDict()
                  .FindBool(site_settings::kFileSystemIsDirectory)
                  .value_or(false));
  EXPECT_EQ(
      CHECK_DEREF(CHECK_DEREF(kTestOrigin1ViewGrants)[1].GetDict().FindString(
          site_settings::kFileSystemFilePath)),
      "/a/b");
  ASSERT_TRUE(kTestOrigin1EditGrants != nullptr);
  EXPECT_TRUE(kTestOrigin1EditGrants->empty());

  // In the case of kTestOrigin2, check that when an origin has an
  // associated 'write' grant, that the grant is only recorded in the
  // respective write grants list, and is not recorded in the origin's
  // read grants list.
  ASSERT_TRUE(kTestOrigin2ViewGrants != nullptr);
  EXPECT_TRUE(kTestOrigin2ViewGrants->empty());
  EXPECT_EQ(
      CHECK_DEREF(CHECK_DEREF(kTestOrigin2EditGrants)[0].GetDict().FindString(
          site_settings::kDisplayName)),
      "/f/g/h/");
  EXPECT_FALSE(CHECK_DEREF(kTestOrigin2EditGrants)[1]
                   .GetDict()
                   .FindBool(site_settings::kFileSystemIsDirectory)
                   .value_or(true));
}

// RevokeGrant() revokes a single File System Access permission grant,
// for a given origin and file path.
TEST_F(PersistentPermissionsSiteSettingsHandlerTest,
       HandleRevokeFileSystemGrant) {
  ChromeFileSystemAccessPermissionContext* context =
      FileSystemAccessPermissionContextFactory::GetForProfile(&profile_);

  auto kTestOrigin1 = url::Origin::Create(GURL("https://www.a.com"));
  auto kTestOrigin2 = url::Origin::Create(GURL("https://www.b.com"));

  const base::FilePath kTestPath = base::FilePath(FILE_PATH_LITERAL("/a/b"));
  const base::FilePath kTestPath2 = base::FilePath(FILE_PATH_LITERAL("/c/d/"));
  const base::FilePath kTestPath3 = base::FilePath(FILE_PATH_LITERAL("/e/"));
  const base::FilePath kTestPath4 =
      base::FilePath(FILE_PATH_LITERAL("/f/g/h/"));

  context->SetOriginHasExtendedPermissionForTesting(kTestOrigin1);
  context->SetOriginHasExtendedPermissionForTesting(kTestOrigin2);

  // Populate the `grants` object with permissions.
  auto file_read_grant = context->GetExtendedReadPermissionGrantForTesting(
      kTestOrigin1, content::PathInfo(kTestPath),
      ChromeFileSystemAccessPermissionContext::HandleType::kFile);
  auto directory_read_grant = context->GetExtendedReadPermissionGrantForTesting(
      kTestOrigin1, content::PathInfo(kTestPath2),
      ChromeFileSystemAccessPermissionContext::HandleType::kDirectory);
  auto directory_write_grant =
      context->GetExtendedWritePermissionGrantForTesting(
          kTestOrigin2, content::PathInfo(kTestPath3),
          ChromeFileSystemAccessPermissionContext::HandleType::kDirectory);
  auto second_directory_write_grant =
      context->GetExtendedWritePermissionGrantForTesting(
          kTestOrigin2, content::PathInfo(kTestPath4),
          ChromeFileSystemAccessPermissionContext::HandleType::kDirectory);

  base::Value::List revoke_origin1_grant_permissions_args;
  revoke_origin1_grant_permissions_args.Append("https://www.a.com");
  revoke_origin1_grant_permissions_args.Append("/a/b");

  base::Value::List get_file_system_grants_permissions_args;
  get_file_system_grants_permissions_args.Append(kCallbackId);

  handler_->HandleRevokeFileSystemGrant(revoke_origin1_grant_permissions_args);
  handler_->HandleGetFileSystemGrants(get_file_system_grants_permissions_args);
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  const base::Value::List& grants = data.arg3()->GetList();

  // After revoking the `file_read_grant` for kTestOrigin1, only one view grant
  // should remain when retrieving the file system grants for kTestOrigin1.
  EXPECT_EQ(grants[0]
                .GetDict()
                .FindList(site_settings::kFileSystemViewGrants)
                ->size(),
            1UL);

  // Revoking a single grant from an origin with multiple grants in a given
  // grants list only revokes the grant with the given file path.
  // In this case, for kTestOrigin2, only the directory write grant for
  // kTestPath2 is revoked, and the directory write grant with kTestPath4
  // remains in the `directory_write_grants` list.
  base::Value::List revoke_origin2_grant_permissions_args;
  revoke_origin2_grant_permissions_args.Append("https://www.b.com");
  revoke_origin2_grant_permissions_args.Append("/e/");

  handler_->HandleRevokeFileSystemGrant(revoke_origin2_grant_permissions_args);
  handler_->HandleGetFileSystemGrants(get_file_system_grants_permissions_args);
  const content::TestWebUI::CallData& updated_data =
      *web_ui()->call_data().back();
  const base::Value::List& updated_grants = updated_data.arg3()->GetList();

  EXPECT_EQ(updated_grants[1]
                .GetDict()
                .FindList(site_settings::kFileSystemEditGrants)
                ->size(),
            1UL);
  EXPECT_EQ(CHECK_DEREF(CHECK_DEREF(updated_grants[1].GetDict().FindList(
                site_settings::kFileSystemEditGrants))[0]
                            .GetDict()
                            .FindString(site_settings::kFileSystemFilePath)),
            "/f/g/h/");
}

// RevokeGrants() revokes all File System Access permission grants,
// for a given origin.
TEST_F(PersistentPermissionsSiteSettingsHandlerTest,
       HandleRevokeFileSystemGrants) {
  ChromeFileSystemAccessPermissionContext* context =
      FileSystemAccessPermissionContextFactory::GetForProfile(&profile_);

  auto kTestOrigin1 = url::Origin::Create(GURL("https://www.a.com"));
  auto kTestOrigin2 = url::Origin::Create(GURL("https://www.b.com"));

  const base::FilePath kTestPath = base::FilePath(FILE_PATH_LITERAL("/a/b"));
  const base::FilePath kTestPath2 = base::FilePath(FILE_PATH_LITERAL("/c/d"));
  const base::FilePath kTestPath3 = base::FilePath(FILE_PATH_LITERAL("/e/"));
  const base::FilePath kTestPath4 =
      base::FilePath(FILE_PATH_LITERAL("/f/g/h/"));

  context->SetOriginHasExtendedPermissionForTesting(kTestOrigin1);
  context->SetOriginHasExtendedPermissionForTesting(kTestOrigin2);

  // Populate the `grants` object with permissions.
  auto file_read_grant = context->GetExtendedReadPermissionGrantForTesting(
      kTestOrigin1, content::PathInfo(kTestPath),
      ChromeFileSystemAccessPermissionContext::HandleType::kFile);
  auto file_write_grant = context->GetExtendedWritePermissionGrantForTesting(
      kTestOrigin2, content::PathInfo(kTestPath2),
      ChromeFileSystemAccessPermissionContext::HandleType::kFile);
  auto directory_read_grant = context->GetExtendedReadPermissionGrantForTesting(
      kTestOrigin1, content::PathInfo(kTestPath3),
      ChromeFileSystemAccessPermissionContext::HandleType::kDirectory);
  auto directory_write_grant =
      context->GetExtendedWritePermissionGrantForTesting(
          kTestOrigin2, content::PathInfo(kTestPath4),
          ChromeFileSystemAccessPermissionContext::HandleType::kDirectory);

  base::Value::List get_file_system_grants_permissions_args;
  get_file_system_grants_permissions_args.Append(kCallbackId);

  handler_->HandleGetFileSystemGrants(get_file_system_grants_permissions_args);
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  const base::Value::List& grants = data.arg3()->GetList();

  // The number of entries in grants is equal to the number of origins with
  // permission grants, before revoking grants for a given origin.
  EXPECT_EQ(grants.size(), 2UL);

  base::Value::List revoke_origin1_grants_permissions_args;
  revoke_origin1_grants_permissions_args.Append("https://www.a.com");

  handler_->HandleRevokeFileSystemGrants(
      revoke_origin1_grants_permissions_args);
  handler_->HandleGetFileSystemGrants(get_file_system_grants_permissions_args);
  const content::TestWebUI::CallData& updated_data =
      *web_ui()->call_data().back();
  const base::Value::List& updated_grants = updated_data.arg3()->GetList();

  // All grants are revoked for kTestOrigin1, and the grants for kTestOrigin2
  // are unaffected.
  EXPECT_EQ(updated_grants.size(), 1UL);
  EXPECT_EQ(updated_grants[0]
                .GetDict()
                .FindList(site_settings::kFileSystemEditGrants)
                ->size(),
            2UL);

  // Expect that the WebUIListenerCallback was triggered.
  EXPECT_EQ(web_ui()->call_data()[0]->function_name(),
            "cr.webUIListenerCallback");

  EXPECT_EQ(updated_data.arg1()->GetString(), kCallbackId);
}

namespace {

std::vector<std::string> GetExceptionDisplayNames(
    const base::Value::List& exceptions) {
  std::vector<std::string> display_names;
  for (const base::Value& exception : exceptions) {
    const std::string* display_name =
        exception.GetDict().FindString(site_settings::kDisplayName);
    if (display_name) {
      display_names.push_back(*display_name);
    }
  }
  return display_names;
}

}  // namespace

class SiteSettingsHandlerChooserExceptionTest
    : public SiteSettingsHandlerBaseTest {
 protected:
  const GURL kAndroidUrl{"https://android.com"};
  const GURL kChromiumUrl{"https://chromium.org"};
  const GURL kGoogleUrl{"https://google.com"};
  const GURL kWebUIUrl{"chrome://test"};

  void SetUp() override {
    SiteSettingsHandlerBaseTest::SetUp();
    SetUpChooserContext();
    SetUpPolicyGrantedPermissions();

    // Add the observer for permission changes.
    GetChooserContext(profile())->AddObserver(&observer_);
  }

  void TearDown() override {
    GetChooserContext(profile())->RemoveObserver(&observer_);
    SiteSettingsHandlerBaseTest::TearDown();
  }

  void DestroyIncognitoProfile() override {
    GetChooserContext(incognito_profile())->RemoveObserver(&observer_);
    SiteSettingsHandlerBaseTest::DestroyIncognitoProfile();
  }

  // Call SiteSettingsHandler::HandleGetChooserExceptionList for |chooser_type|
  // and return the exception list received by the WebUI.
  void ValidateChooserExceptionList(const std::string& chooser_type,
                                    size_t expected_total_calls) {
    base::Value::List args;
    args.Append(kCallbackId);
    args.Append(chooser_type);

    handler()->HandleGetChooserExceptionList(args);

    EXPECT_EQ(web_ui()->call_data().size(), expected_total_calls);

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());

    ASSERT_TRUE(data.arg1());
    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ(data.arg1()->GetString(), kCallbackId);

    ASSERT_TRUE(data.arg2());
    ASSERT_TRUE(data.arg2()->is_bool());
    EXPECT_TRUE(data.arg2()->GetBool());

    ASSERT_TRUE(data.arg3());
    ASSERT_TRUE(data.arg3()->is_list());
  }

  const base::Value::List& GetChooserExceptionListFromWebUiCallData(
      const std::string& chooser_type,
      size_t expected_total_calls) {
    ValidateChooserExceptionList(chooser_type, expected_total_calls);
    return web_ui()->call_data().back()->arg3()->GetList();
  }

  // Iterate through the exception's sites array and return true if a site
  // exception matches |requesting_origin| and |embedding_origin|.
  bool ChooserExceptionContainsSiteException(const base::Value::Dict& exception,
                                             std::string_view origin) {
    const base::Value::List* sites = exception.FindList(site_settings::kSites);
    if (!sites)
      return false;

    for (const auto& site : *sites) {
      const std::string* exception_origin =
          site.GetDict().FindString(site_settings::kOrigin);
      if (!exception_origin)
        continue;
      if (*exception_origin == origin)
        return true;
    }
    return false;
  }

  // Iterate through the |exception_list| array and return true if there is a
  // chooser exception with |display_name| that contains a site exception for
  // |origin|.
  bool ChooserExceptionContainsSiteException(
      const base::Value::List& exceptions,
      std::string_view display_name,
      std::string_view origin) {
    for (const auto& exception : exceptions) {
      const std::string* exception_display_name =
          exception.GetDict().FindString(site_settings::kDisplayName);
      if (!exception_display_name)
        continue;

      if (*exception_display_name == display_name) {
        return ChooserExceptionContainsSiteException(exception.GetDict(),
                                                     origin);
      }
    }
    return false;
  }

  void TestHandleGetChooserExceptionList() {
    AddPersistentDevice();
    AddEphemeralDevice();
    AddUserGrantedDevice();
    base::RunLoop().RunUntilIdle();

    SetUpUserGrantedPermissions();

    base::RunLoop().RunUntilIdle();
    web_ui()->ClearTrackedCalls();

    const std::string group_name(
        site_settings::ContentSettingsTypeToGroupName(content_type()));

    const base::Value::List& exceptions =
        GetChooserExceptionListFromWebUiCallData(group_name,
                                                 /*expected_total_calls=*/1u);

    // There are 9 granted permissions:
    // 1. Persistent permission for persistent-device on kChromiumOrigin
    // 2. Persistent permission for persistent-device on kGoogleOrigin
    // 3. Persistent permission for persistent-device on kWebUIOrigin
    // 4. Persistent permission for user-granted-device on kAndroidOrigin
    // 5. Ephemeral permission for ephemeral-device on kAndroidOrigin
    // 6. Policy-granted permission for any device on kGoogleOrigin
    // 7. Policy-granted permission for vendor 18D1 on kAndroidOrigin
    // 8. Policy-granted permission for vendor 18D2 on kAndroidOrigin
    // 9. Policy-granted permission for device 18D1:162E on kChromiumOrigin
    //
    // Permission 3 is ignored by GetChooserExceptionListFromProfile because its
    // origin has a WebUI scheme (chrome://).
    //
    // Some of the user-granted permissions are redundant due to policy-granted
    // permissions. Permission 1 is redundant due to permission 9; 2 is
    // redundant due to 6; 5 is redundant due to 8. UsbChooserContext omits
    // redundant items but other APIs do not. This causes
    // GetChooserExceptionListForProfile to return a different number of
    // exceptions depending on the API.
    //
    // UsbChooserContext also detects when a policy-granted permission refers to
    // the same origin and device IDs as a user-granted permission. When
    // deduplicating redundant exceptions, the user-granted exception display
    // name is used because it contains the device name.
    //
    // TODO(crbug.com/40247735): Update SerialChooserContext and
    // HidChooserContext to deduplicate redundant exceptions.
    switch (content_type()) {
      case ContentSettingsType::BLUETOOTH_CHOOSER_DATA:
        // BluetoothChooserContext creates a different permission object for
        // each (device,origin) pair, so persistent-device shows up for each of
        // kChromiumOrigin and kGoogleOrigin.
        //
        // TODO(crbug.com/40667219): No policy-granted exceptions are
        // included because Web Bluetooth does not support granting device
        // permissions by policy.
        EXPECT_THAT(
            GetExceptionDisplayNames(exceptions),
            UnorderedElementsAre("persistent-device", "persistent-device",
                                 "ephemeral-device", "user-granted-device"));
        break;
      case ContentSettingsType::HID_CHOOSER_DATA:
      case ContentSettingsType::SERIAL_CHOOSER_DATA:
        EXPECT_THAT(
            GetExceptionDisplayNames(exceptions),
            UnorderedElementsAre("persistent-device", "user-granted-device",
                                 "ephemeral-device", GetAllDevicesDisplayName(),
                                 GetDevicesFromGoogleDisplayName(),
                                 GetDevicesFromVendor18D2DisplayName(),
                                 GetUnknownProductDisplayName()));
        break;
      case ContentSettingsType::USB_CHOOSER_DATA:
        EXPECT_THAT(
            GetExceptionDisplayNames(exceptions),
            UnorderedElementsAre("persistent-device", "user-granted-device",
                                 GetAllDevicesDisplayName(),
                                 GetDevicesFromGoogleDisplayName(),
                                 GetDevicesFromVendor18D2DisplayName()));
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }

    // Don't include WebUI schemes.
    const std::string kWebUIOriginStr =
        kWebUIUrl.DeprecatedGetOriginAsURL().spec();
    EXPECT_FALSE(ChooserExceptionContainsSiteException(
        exceptions, "persistent-device", kWebUIOriginStr));
  }

  void TestHandleGetChooserExceptionListForOffTheRecord() {
    SetUpOffTheRecordChooserContext();
    AddPersistentDevice();
    AddEphemeralDevice();
    AddUserGrantedDevice();
    AddOffTheRecordDevice();
    base::RunLoop().RunUntilIdle();

    SetUpUserGrantedPermissions();

    base::RunLoop().RunUntilIdle();
    web_ui()->ClearTrackedCalls();

    const std::string group_name(
        site_settings::ContentSettingsTypeToGroupName(content_type()));

    // The objects returned by GetChooserExceptionListFromProfile should also
    // include the incognito permissions.
    {
      const base::Value::List& exceptions =
          GetChooserExceptionListFromWebUiCallData(group_name,
                                                   /*expected_total_calls=*/1u);
      switch (content_type()) {
        case ContentSettingsType::BLUETOOTH_CHOOSER_DATA:
          EXPECT_THAT(
              GetExceptionDisplayNames(exceptions),
              UnorderedElementsAre("persistent-device", "persistent-device",
                                   "ephemeral-device", "user-granted-device",
                                   "off-the-record-device"));
          break;
        case ContentSettingsType::HID_CHOOSER_DATA:
        case ContentSettingsType::SERIAL_CHOOSER_DATA:
          EXPECT_THAT(
              GetExceptionDisplayNames(exceptions),
              UnorderedElementsAre(
                  "persistent-device", "ephemeral-device",
                  "user-granted-device", "off-the-record-device",
                  GetAllDevicesDisplayName(), GetDevicesFromGoogleDisplayName(),
                  GetDevicesFromVendor18D2DisplayName(),
                  GetUnknownProductDisplayName()));
          break;
        case ContentSettingsType::USB_CHOOSER_DATA:
          EXPECT_THAT(GetExceptionDisplayNames(exceptions),
                      UnorderedElementsAre(
                          "persistent-device", "user-granted-device",
                          "off-the-record-device", GetAllDevicesDisplayName(),
                          GetDevicesFromGoogleDisplayName(),
                          GetDevicesFromVendor18D2DisplayName(),
                          GetUnknownProductDisplayName()));
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
    }

    // Destroy the off the record profile and check that the objects returned do
    // not include incognito permissions anymore. The destruction of the profile
    // causes the "onIncognitoStatusChanged" WebUIListener callback to fire.
    DestroyIncognitoProfile();
    EXPECT_EQ(web_ui()->call_data().size(), 2u);

    {
      const base::Value::List& exceptions =
          GetChooserExceptionListFromWebUiCallData(group_name,
                                                   /*expected_total_calls=*/3u);
      switch (content_type()) {
        case ContentSettingsType::BLUETOOTH_CHOOSER_DATA:
          EXPECT_THAT(
              GetExceptionDisplayNames(exceptions),
              UnorderedElementsAre("persistent-device", "persistent-device",
                                   "ephemeral-device", "user-granted-device"));
          break;
        case ContentSettingsType::HID_CHOOSER_DATA:
        case ContentSettingsType::SERIAL_CHOOSER_DATA:
          EXPECT_THAT(GetExceptionDisplayNames(exceptions),
                      UnorderedElementsAre(
                          "persistent-device", "ephemeral-device",
                          "user-granted-device", GetAllDevicesDisplayName(),
                          GetDevicesFromGoogleDisplayName(),
                          GetDevicesFromVendor18D2DisplayName(),
                          GetUnknownProductDisplayName()));
          break;
        case ContentSettingsType::USB_CHOOSER_DATA:
          EXPECT_THAT(
              GetExceptionDisplayNames(exceptions),
              UnorderedElementsAre("persistent-device", "user-granted-device",
                                   GetAllDevicesDisplayName(),
                                   GetDevicesFromGoogleDisplayName(),
                                   GetDevicesFromVendor18D2DisplayName()));
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
    }
  }

  void TestHandleResetChooserExceptionForSite() {
    AddPersistentDevice();
    AddEphemeralDevice();
    AddUserGrantedDevice();
    base::RunLoop().RunUntilIdle();

    SetUpUserGrantedPermissions();

    base::RunLoop().RunUntilIdle();
    web_ui()->ClearTrackedCalls();

    const std::string group_name(
        site_settings::ContentSettingsTypeToGroupName(content_type()));
    const auto kAndroidOrigin = url::Origin::Create(kAndroidUrl);
    const auto kChromiumOrigin = url::Origin::Create(kChromiumUrl);
    const auto kGoogleOrigin = url::Origin::Create(kGoogleUrl);
    const std::string kAndroidOriginStr =
        kAndroidUrl.DeprecatedGetOriginAsURL().spec();
    const std::string kChromiumOriginStr =
        kChromiumUrl.DeprecatedGetOriginAsURL().spec();
    const std::string kGoogleOriginStr =
        kGoogleUrl.DeprecatedGetOriginAsURL().spec();

    {
      const base::Value::List& exceptions =
          GetChooserExceptionListFromWebUiCallData(group_name,
                                                   /*expected_total_calls=*/1u);
      switch (content_type()) {
        case ContentSettingsType::BLUETOOTH_CHOOSER_DATA:
          EXPECT_THAT(
              GetExceptionDisplayNames(exceptions),
              UnorderedElementsAre("persistent-device", "persistent-device",
                                   "ephemeral-device", "user-granted-device"));
          break;
        case ContentSettingsType::HID_CHOOSER_DATA:
        case ContentSettingsType::SERIAL_CHOOSER_DATA:
          EXPECT_THAT(GetExceptionDisplayNames(exceptions),
                      UnorderedElementsAre(
                          "persistent-device", "ephemeral-device",
                          "user-granted-device", GetAllDevicesDisplayName(),
                          GetDevicesFromGoogleDisplayName(),
                          GetDevicesFromVendor18D2DisplayName(),
                          GetUnknownProductDisplayName()));
          break;
        case ContentSettingsType::USB_CHOOSER_DATA:
          EXPECT_THAT(
              GetExceptionDisplayNames(exceptions),
              UnorderedElementsAre("persistent-device", "user-granted-device",
                                   GetAllDevicesDisplayName(),
                                   GetDevicesFromGoogleDisplayName(),
                                   GetDevicesFromVendor18D2DisplayName()));
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
    }

    // User granted USB permissions for devices also containing policy
    // permissions should be able to be reset without removing the chooser
    // exception object from the list.
    base::Value::List args;
    args.Append(group_name);
    args.Append(kGoogleOriginStr);
    args.Append(GetPersistentDeviceValueForOrigin(kGoogleOrigin));

    EXPECT_CALL(observer_,
                OnObjectPermissionChanged({guard_type()}, content_type()));
    EXPECT_CALL(observer_, OnPermissionRevoked(kGoogleOrigin));
    handler()->HandleResetChooserExceptionForSite(args);
    GetChooserContext(profile())->FlushScheduledSaveSettingsCalls();

    // The HandleResetChooserExceptionForSite() method should have also caused
    // the WebUIListenerCallbacks for contentSettingSitePermissionChanged and
    // contentSettingChooserPermissionChanged to fire.
    EXPECT_EQ(web_ui()->call_data().size(), 3u);
    {
      // The exception list size should not have been reduced since there is
      // still a policy granted permission for "persistent-device".
      const base::Value::List& exceptions =
          GetChooserExceptionListFromWebUiCallData(group_name,
                                                   /*expected_total_calls=*/4u);
      switch (content_type()) {
        case ContentSettingsType::BLUETOOTH_CHOOSER_DATA:
          EXPECT_THAT(
              GetExceptionDisplayNames(exceptions),
              UnorderedElementsAre("persistent-device", "ephemeral-device",
                                   "user-granted-device"));
          break;
        case ContentSettingsType::HID_CHOOSER_DATA:
        case ContentSettingsType::SERIAL_CHOOSER_DATA:
          EXPECT_THAT(GetExceptionDisplayNames(exceptions),
                      UnorderedElementsAre(
                          "persistent-device", "ephemeral-device",
                          "user-granted-device", GetAllDevicesDisplayName(),
                          GetDevicesFromGoogleDisplayName(),
                          GetDevicesFromVendor18D2DisplayName(),
                          GetUnknownProductDisplayName()));
          break;
        case ContentSettingsType::USB_CHOOSER_DATA:
          EXPECT_THAT(
              GetExceptionDisplayNames(exceptions),
              UnorderedElementsAre("persistent-device", "user-granted-device",
                                   GetAllDevicesDisplayName(),
                                   GetDevicesFromGoogleDisplayName(),
                                   GetDevicesFromVendor18D2DisplayName()));
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }

      // Ensure that the sites list does not contain the URLs of the removed
      // permission.
      EXPECT_FALSE(ChooserExceptionContainsSiteException(
          exceptions, "persistent-device", kGoogleOriginStr));

      // User granted exceptions that are also granted by policy are only
      // displayed through the policy granted site exception, so ensure that the
      // policy exception is present under "persistent-device".
      if (content_type() != ContentSettingsType::BLUETOOTH_CHOOSER_DATA) {
        EXPECT_TRUE(ChooserExceptionContainsSiteException(
            exceptions, "persistent-device", kChromiumOriginStr));
      }
    }

    // Try revoking the user-granted permission for persistent-device. There is
    // also a policy-granted permission for the same device.
    args.clear();
    args.Append(group_name);
    args.Append(kChromiumOriginStr);
    args.Append(GetPersistentDeviceValueForOrigin(kChromiumOrigin));

    EXPECT_CALL(observer_,
                OnObjectPermissionChanged({guard_type()}, content_type()));
    EXPECT_CALL(observer_, OnPermissionRevoked(kChromiumOrigin));
    handler()->HandleResetChooserExceptionForSite(args);
    GetChooserContext(profile())->FlushScheduledSaveSettingsCalls();

    // The HandleResetChooserExceptionForSite() method should have also caused
    // the WebUIListenerCallbacks for contentSettingSitePermissionChanged and
    // contentSettingChooserPermissionChanged to fire.
    EXPECT_EQ(web_ui()->call_data().size(), 6u);
    {
      const base::Value::List& exceptions =
          GetChooserExceptionListFromWebUiCallData(group_name,
                                                   /*expected_total_calls=*/7u);
      switch (content_type()) {
        case ContentSettingsType::BLUETOOTH_CHOOSER_DATA:
          EXPECT_THAT(
              GetExceptionDisplayNames(exceptions),
              UnorderedElementsAre("ephemeral-device", "user-granted-device"));
          break;
        case ContentSettingsType::HID_CHOOSER_DATA:
        case ContentSettingsType::SERIAL_CHOOSER_DATA:
          EXPECT_THAT(
              GetExceptionDisplayNames(exceptions),
              UnorderedElementsAre("ephemeral-device", "user-granted-device",
                                   GetAllDevicesDisplayName(),
                                   GetDevicesFromGoogleDisplayName(),
                                   GetDevicesFromVendor18D2DisplayName(),
                                   GetUnknownProductDisplayName()));
          break;
        case ContentSettingsType::USB_CHOOSER_DATA:
          EXPECT_THAT(GetExceptionDisplayNames(exceptions),
                      UnorderedElementsAre(
                          "user-granted-device", GetAllDevicesDisplayName(),
                          GetDevicesFromGoogleDisplayName(),
                          GetDevicesFromVendor18D2DisplayName(),
                          GetUnknownProductDisplayName()));
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }

      // Ensure that the sites list still displays a site exception entry for an
      // origin of kGoogleOriginStr.  Since now the device has had its
      // permission revoked, the policy-provided object will not be able to
      // deduce the name "persistent-device" from the connected device. As such
      // we check that the policy is still active by looking for the genericly
      // constructed name.
      if (content_type() != ContentSettingsType::BLUETOOTH_CHOOSER_DATA) {
        EXPECT_TRUE(ChooserExceptionContainsSiteException(
            exceptions, GetUnknownProductDisplayName(), kChromiumOriginStr));
        EXPECT_FALSE(ChooserExceptionContainsSiteException(
            exceptions, "persistent-device", kGoogleOriginStr));
      }

      // Ensure the exception for user-granted-device on kAndroidOrigin is
      // present since we will try to revoke it.
      EXPECT_TRUE(ChooserExceptionContainsSiteException(
          exceptions, "user-granted-device", kAndroidOriginStr));
    }

    // User granted USB permissions that are not covered by policy should be
    // able to be reset and the chooser exception entry should be removed from
    // the list when the exception only has one site exception granted to it.
    args.clear();
    args.Append(group_name);
    args.Append(kAndroidOriginStr);
    args.Append(GetUserGrantedDeviceValueForOrigin(kAndroidOrigin));

    EXPECT_CALL(observer_,
                OnObjectPermissionChanged({guard_type()}, content_type()));
    EXPECT_CALL(observer_, OnPermissionRevoked(kAndroidOrigin));
    handler()->HandleResetChooserExceptionForSite(args);
    GetChooserContext(profile())->FlushScheduledSaveSettingsCalls();

    // The HandleResetChooserExceptionForSite() method should have also caused
    // the WebUIListenerCallbacks for contentSettingSitePermissionChanged and
    // contentSettingChooserPermissionChanged to fire.
    EXPECT_EQ(web_ui()->call_data().size(), 9u);
    {
      const base::Value::List& exceptions =
          GetChooserExceptionListFromWebUiCallData(
              group_name, /*expected_total_calls=*/10u);
      switch (content_type()) {
        case ContentSettingsType::BLUETOOTH_CHOOSER_DATA:
          EXPECT_THAT(GetExceptionDisplayNames(exceptions),
                      UnorderedElementsAre("ephemeral-device"));
          break;
        case ContentSettingsType::HID_CHOOSER_DATA:
        case ContentSettingsType::SERIAL_CHOOSER_DATA:
          EXPECT_THAT(GetExceptionDisplayNames(exceptions),
                      UnorderedElementsAre(
                          "ephemeral-device", GetAllDevicesDisplayName(),
                          GetDevicesFromGoogleDisplayName(),
                          GetDevicesFromVendor18D2DisplayName(),
                          GetUnknownProductDisplayName()));
          break;
        case ContentSettingsType::USB_CHOOSER_DATA:
          EXPECT_THAT(
              GetExceptionDisplayNames(exceptions),
              UnorderedElementsAre(GetAllDevicesDisplayName(),
                                   GetDevicesFromGoogleDisplayName(),
                                   GetDevicesFromVendor18D2DisplayName(),
                                   GetUnknownProductDisplayName()));
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
      EXPECT_FALSE(ChooserExceptionContainsSiteException(
          exceptions, "user-granted-device", kAndroidOriginStr));
    }
  }

  void TestHandleSetOriginPermissions() {
    constexpr std::string_view kYoutubeOriginStr = "https://youtube.com/";
    const GURL kYoutubeUrl{kYoutubeOriginStr};
    const auto kYoutubeOrigin = url::Origin::Create(kYoutubeUrl);

    // Grant permissions for user-granted-device on `kYoutubeOrigin`.
    AddUserGrantedDevice();
    SetUpUserGrantedPermissionForOrigin(kYoutubeOrigin);

    base::RunLoop().RunUntilIdle();
    web_ui()->ClearTrackedCalls();

    const std::string group_name(
        site_settings::ContentSettingsTypeToGroupName(content_type()));

    {
      const base::Value::List& exceptions =
          GetChooserExceptionListFromWebUiCallData(group_name,
                                                   /*expected_total_calls=*/1u);
      switch (content_type()) {
        case ContentSettingsType::BLUETOOTH_CHOOSER_DATA:
          EXPECT_THAT(GetExceptionDisplayNames(exceptions),
                      UnorderedElementsAre("user-granted-device"));
          break;
        case ContentSettingsType::HID_CHOOSER_DATA:
        case ContentSettingsType::SERIAL_CHOOSER_DATA:
        case ContentSettingsType::USB_CHOOSER_DATA:
          EXPECT_THAT(GetExceptionDisplayNames(exceptions),
                      UnorderedElementsAre(
                          "user-granted-device", GetAllDevicesDisplayName(),
                          GetDevicesFromGoogleDisplayName(),
                          GetDevicesFromVendor18D2DisplayName(),
                          GetUnknownProductDisplayName()));
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
      EXPECT_TRUE(ChooserExceptionContainsSiteException(
          exceptions, "user-granted-device", kYoutubeOriginStr));
    }

    // Clear data for kYoutubeOrigin. The permission should be revoked.
    base::Value::List args;
    args.Append(kYoutubeOriginStr);
    args.Append(base::Value());
    args.Append(
        content_settings::ContentSettingToString(CONTENT_SETTING_DEFAULT));

    EXPECT_CALL(observer_,
                OnObjectPermissionChanged({guard_type()}, content_type()));
    EXPECT_CALL(observer_, OnPermissionRevoked(kYoutubeOrigin));
    handler()->HandleSetOriginPermissions(args);
    GetChooserContext(profile())->FlushScheduledSaveSettingsCalls();

    // HandleSetOriginPermissions caused WebUIListenerCallbacks:
    // * contentSettingsChooserPermissionChanged once
    // * contentSettingsSitePermissionChanged once
    // * contentSettingsSitePermissionChanged again for `content_type()`
    EXPECT_EQ(3u, web_ui()->call_data().size());
    {
      const base::Value::List& exceptions =
          GetChooserExceptionListFromWebUiCallData(group_name,
                                                   /*expected_total_calls=*/4u);
      switch (content_type()) {
        case ContentSettingsType::BLUETOOTH_CHOOSER_DATA:
          EXPECT_TRUE(exceptions.empty());
          break;
        case ContentSettingsType::HID_CHOOSER_DATA:
        case ContentSettingsType::SERIAL_CHOOSER_DATA:
        case ContentSettingsType::USB_CHOOSER_DATA:
          EXPECT_THAT(
              GetExceptionDisplayNames(exceptions),
              UnorderedElementsAre(GetAllDevicesDisplayName(),
                                   GetDevicesFromGoogleDisplayName(),
                                   GetDevicesFromVendor18D2DisplayName(),
                                   GetUnknownProductDisplayName()));
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
      EXPECT_FALSE(ChooserExceptionContainsSiteException(
          exceptions, "user-granted-device", kYoutubeOriginStr));
    }
  }

  void TestHandleSetOriginPermissionsPolicyOnly() {
    const auto kGoogleOrigin = url::Origin::Create(kGoogleUrl);
    const std::string kGoogleOriginStr =
        kGoogleUrl.DeprecatedGetOriginAsURL().spec();

    base::RunLoop().RunUntilIdle();
    web_ui()->ClearTrackedCalls();

    const std::string group_name(
        site_settings::ContentSettingsTypeToGroupName(content_type()));

    {
      const base::Value::List& exceptions =
          GetChooserExceptionListFromWebUiCallData(group_name,
                                                   /*expected_total_calls=*/1u);
      switch (content_type()) {
        case ContentSettingsType::BLUETOOTH_CHOOSER_DATA:
          EXPECT_TRUE(exceptions.empty());
          break;
        case ContentSettingsType::HID_CHOOSER_DATA:
        case ContentSettingsType::SERIAL_CHOOSER_DATA:
        case ContentSettingsType::USB_CHOOSER_DATA:
          EXPECT_THAT(
              GetExceptionDisplayNames(exceptions),
              UnorderedElementsAre(GetAllDevicesDisplayName(),
                                   GetDevicesFromGoogleDisplayName(),
                                   GetDevicesFromVendor18D2DisplayName(),
                                   GetUnknownProductDisplayName()));
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
    }

    // Clear data for kGoogleOrigin.
    base::Value::List args;
    args.Append(kGoogleOriginStr);
    args.Append(base::Value());
    args.Append(
        content_settings::ContentSettingToString(CONTENT_SETTING_DEFAULT));

    EXPECT_CALL(observer_, OnObjectPermissionChanged).Times(0);
    EXPECT_CALL(observer_, OnPermissionRevoked).Times(0);
    handler()->HandleSetOriginPermissions(args);
    GetChooserContext(profile())->FlushScheduledSaveSettingsCalls();

    EXPECT_EQ(1u, web_ui()->call_data().size());
    {
      const base::Value::List& exceptions =
          GetChooserExceptionListFromWebUiCallData(group_name,
                                                   /*expected_total_calls=*/2u);
      switch (content_type()) {
        case ContentSettingsType::BLUETOOTH_CHOOSER_DATA:
          EXPECT_TRUE(exceptions.empty());
          break;
        case ContentSettingsType::HID_CHOOSER_DATA:
        case ContentSettingsType::SERIAL_CHOOSER_DATA:
        case ContentSettingsType::USB_CHOOSER_DATA:
          EXPECT_THAT(
              GetExceptionDisplayNames(exceptions),
              UnorderedElementsAre(GetAllDevicesDisplayName(),
                                   GetDevicesFromGoogleDisplayName(),
                                   GetDevicesFromVendor18D2DisplayName(),
                                   GetUnknownProductDisplayName()));
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
    }
  }

  // Returns the ContentSettingsType for the chooser data (device permissions).
  virtual ContentSettingsType content_type() = 0;

  // Returns the ContentSettingsType for the guard permission.
  virtual ContentSettingsType guard_type() = 0;

  // Returns the chooser context for `profile`.
  virtual permissions::ObjectPermissionContextBase* GetChooserContext(
      Profile* profile) = 0;

  // Sets up the chooser context.
  virtual void SetUpChooserContext() = 0;

  // Creates an incognito profile and sets up the chooser context for that
  // profile. Tests must call DestroyIncognitoProfile before exiting.
  virtual void SetUpOffTheRecordChooserContext() {}

  // Grants permissions used by tests. There are four devices that are
  // granted user permissions. Two (persistent-device and ephemeral-device) are
  // covered by different policy permissions, while the third
  // (user-granted-device) is not covered by policy at all. If the
  // off-the-record-device is present, a user-granted permission is granted for
  // the incognito profile.
  virtual void SetUpUserGrantedPermissions() = 0;

  // Configures policies to automatically grant device permissions.
  virtual void SetUpPolicyGrantedPermissions() {}

  // Grants device permissions for user-granted-device on `origin`.
  virtual void SetUpUserGrantedPermissionForOrigin(
      const url::Origin& origin) = 0;

  // Create and add a device eligible for persistent permissions. The device
  // name is "persistent-device".
  virtual void AddPersistentDevice() = 0;

  // Create and add a device ineligible for persistent permissions. The device
  // name is "ephemeral-device".
  virtual void AddEphemeralDevice() = 0;

  // Create and add a device. The permission policies added in
  // `SetUpPolicyGrantedPermissions` should not grant permissions for this
  // device except for policies which affect all devices. The device name is
  // "user-granted-device".
  virtual void AddUserGrantedDevice() = 0;

  // Create and add a device. Tests should only grant permissions for this
  // device using the off the record profile. The device name is
  // "off-the-record-device".
  virtual void AddOffTheRecordDevice() = 0;

  // Returns the permission object representing persistent-device granted to
  // `origin`.
  virtual base::Value GetPersistentDeviceValueForOrigin(
      const url::Origin& origin) = 0;

  // Returns the permission object representing user-granted-device granted to
  // `origin`.
  virtual base::Value GetUserGrantedDeviceValueForOrigin(
      const url::Origin& origin) = 0;

  // Returns the display name for a chooser exception that allows an origin to
  // access any device.
  virtual std::string GetAllDevicesDisplayName() { return {}; }

  // Returns the display name for a chooser exception that allows an origin to
  // access any device with the Google vendor ID.
  virtual std::string GetDevicesFromGoogleDisplayName() { return {}; }

  // Returns the display name for a chooser exception that allows an origin to
  // access any device from vendor 0x18D2.
  virtual std::string GetDevicesFromVendor18D2DisplayName() { return {}; }

  // Returns the display name for a chooser exception that allows an origin to
  // access a specific device by its vendor and product IDs.
  virtual std::string GetUnknownProductDisplayName() { return {}; }

  permissions::MockPermissionObserver observer_;
};

class SiteSettingsHandlerBluetoothTest
    : public SiteSettingsHandlerChooserExceptionTest {
 protected:
  SiteSettingsHandlerBluetoothTest()
      : SiteSettingsHandlerChooserExceptionTest() {
    feature_list_.InitAndEnableFeature(
        features::kWebBluetoothNewPermissionsBackend);
  }

  permissions::ObjectPermissionContextBase* GetChooserContext(
      Profile* profile) override {
    return BluetoothChooserContextFactory::GetForProfile(profile);
  }

  void SetUpChooserContext() override {
    adapter_ = base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
    EXPECT_CALL(*adapter_, IsPresent).WillRepeatedly(Return(true));
    EXPECT_CALL(*adapter_, IsPowered).WillRepeatedly(Return(true));
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
    base::RunLoop().RunUntilIdle();
  }

  void SetUpOffTheRecordChooserContext() override {
    CreateIncognitoProfile();
    GetChooserContext(incognito_profile())->AddObserver(&observer_);
  }

  void AddPersistentDevice() override {
    persistent_device_ =
        std::make_unique<NiceMock<device::MockBluetoothDevice>>(
            adapter_.get(), /*bluetooth_class=*/0, /*name=*/"persistent-device",
            /*address=*/"1", /*paired=*/false, /*connected=*/false);
  }

  void AddEphemeralDevice() override {
    ephemeral_device_ = std::make_unique<NiceMock<device::MockBluetoothDevice>>(
        adapter_.get(), /*bluetooth_class=*/0, /*name=*/"ephemeral-device",
        /*address=*/"2", /*paired=*/false, /*connected=*/false);
  }

  void AddUserGrantedDevice() override {
    user_granted_device_ =
        std::make_unique<NiceMock<device::MockBluetoothDevice>>(
            adapter_.get(), /*bluetooth_class=*/0,
            /*name=*/"user-granted-device", /*address=*/"3", /*paired=*/false,
            /*connected=*/false);
  }

  void AddOffTheRecordDevice() override {
    off_the_record_device_ =
        std::make_unique<NiceMock<device::MockBluetoothDevice>>(
            adapter_.get(), /*bluetooth_class=*/0,
            /*name=*/"off-the-record-device", /*address=*/"4", /*paired=*/false,
            /*connected=*/false);
  }

  void SetUpUserGrantedPermissions() override {
    const auto kAndroidOrigin = url::Origin::Create(kAndroidUrl);
    const auto kChromiumOrigin = url::Origin::Create(kChromiumUrl);
    const auto kGoogleOrigin = url::Origin::Create(kGoogleUrl);
    const auto kWebUIOrigin = url::Origin::Create(kWebUIUrl);

    auto options = blink::mojom::WebBluetoothRequestDeviceOptions::New();
    options->accept_all_devices = true;
    {
      base::RunLoop loop;
      auto barrier_closure = base::BarrierClosure(5, loop.QuitClosure());
      auto* bluetooth_chooser_context =
          BluetoothChooserContextFactory::GetForProfile(profile());
      EXPECT_CALL(observer_, OnObjectPermissionChanged(
                                 {ContentSettingsType::BLUETOOTH_GUARD},
                                 ContentSettingsType::BLUETOOTH_CHOOSER_DATA))
          .Times(5)
          .WillRepeatedly(RunClosure(barrier_closure));
      bluetooth_chooser_context->GrantServiceAccessPermission(
          kChromiumOrigin, persistent_device_.get(), options.get());
      bluetooth_chooser_context->GrantServiceAccessPermission(
          kGoogleOrigin, persistent_device_.get(), options.get());
      bluetooth_chooser_context->GrantServiceAccessPermission(
          kWebUIOrigin, persistent_device_.get(), options.get());
      bluetooth_chooser_context->GrantServiceAccessPermission(
          kAndroidOrigin, ephemeral_device_.get(), options.get());
      bluetooth_chooser_context->GrantServiceAccessPermission(
          kAndroidOrigin, user_granted_device_.get(), options.get());
      loop.Run();
    }

    if (off_the_record_device_) {
      base::RunLoop loop;
      EXPECT_CALL(observer_, OnObjectPermissionChanged(
                                 {ContentSettingsType::BLUETOOTH_GUARD},
                                 ContentSettingsType::BLUETOOTH_CHOOSER_DATA))
          .WillOnce(RunClosure(loop.QuitClosure()));
      BluetoothChooserContextFactory::GetForProfile(incognito_profile())
          ->GrantServiceAccessPermission(
              kChromiumOrigin, off_the_record_device_.get(), options.get());
      loop.Run();
    }
  }

  void SetUpUserGrantedPermissionForOrigin(const url::Origin& origin) override {
    auto* bluetooth_chooser_context =
        BluetoothChooserContextFactory::GetForProfile(profile());
    base::RunLoop loop;
    EXPECT_CALL(observer_, OnObjectPermissionChanged(
                               {ContentSettingsType::BLUETOOTH_GUARD},
                               ContentSettingsType::BLUETOOTH_CHOOSER_DATA))
        .WillOnce(RunClosure(loop.QuitClosure()));
    auto options = blink::mojom::WebBluetoothRequestDeviceOptions::New();
    options->accept_all_devices = true;
    bluetooth_chooser_context->GrantServiceAccessPermission(
        origin, user_granted_device_.get(), options.get());
    loop.Run();
  }

  base::Value GetPersistentDeviceValueForOrigin(
      const url::Origin& origin) override {
    auto options = blink::mojom::WebBluetoothRequestDeviceOptions::New();
    options->accept_all_devices = true;
    auto device_id =
        BluetoothChooserContextFactory::GetForProfile(profile())
            ->GetWebBluetoothDeviceId(origin, persistent_device_->GetAddress());
    return base::Value(permissions::BluetoothChooserContext::DeviceInfoToValue(
        persistent_device_.get(), options.get(), device_id));
  }

  base::Value GetUserGrantedDeviceValueForOrigin(
      const url::Origin& origin) override {
    auto options = blink::mojom::WebBluetoothRequestDeviceOptions::New();
    options->accept_all_devices = true;
    auto device_id = BluetoothChooserContextFactory::GetForProfile(profile())
                         ->GetWebBluetoothDeviceId(
                             origin, user_granted_device_->GetAddress());
    return base::Value(permissions::BluetoothChooserContext::DeviceInfoToValue(
        user_granted_device_.get(), options.get(), device_id));
  }

  ContentSettingsType content_type() override {
    return ContentSettingsType::BLUETOOTH_CHOOSER_DATA;
  }

  ContentSettingsType guard_type() override {
    return ContentSettingsType::BLUETOOTH_GUARD;
  }

  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> adapter_;
  std::unique_ptr<NiceMock<device::MockBluetoothDevice>> ephemeral_device_;
  std::unique_ptr<NiceMock<device::MockBluetoothDevice>> off_the_record_device_;
  std::unique_ptr<NiceMock<device::MockBluetoothDevice>> persistent_device_;
  std::unique_ptr<NiceMock<device::MockBluetoothDevice>> user_granted_device_;
};

TEST_F(SiteSettingsHandlerBluetoothTest, HandleGetChooserExceptionList) {
  TestHandleGetChooserExceptionList();
}

TEST_F(SiteSettingsHandlerBluetoothTest,
       HandleGetChooserExceptionListForOffTheRecord) {
  TestHandleGetChooserExceptionListForOffTheRecord();
}

TEST_F(SiteSettingsHandlerBluetoothTest, HandleResetChooserExceptionForSite) {
  TestHandleResetChooserExceptionForSite();
}

TEST_F(SiteSettingsHandlerBluetoothTest, HandleSetOriginPermissions) {
  TestHandleSetOriginPermissions();
}

TEST_F(SiteSettingsHandlerBluetoothTest, HandleSetOriginPermissionsPolicyOnly) {
  TestHandleSetOriginPermissionsPolicyOnly();
}

class SiteSettingsHandlerHidTest
    : public SiteSettingsHandlerChooserExceptionTest {
 protected:
  void SetUpChooserContext() override {
    mojo::PendingRemote<device::mojom::HidManager> hid_manager;
    hid_manager_.AddReceiver(hid_manager.InitWithNewPipeAndPassReceiver());
    HidChooserContext* hid_chooser_context =
        HidChooserContextFactory::GetForProfile(profile());
    TestFuture<std::vector<device::mojom::HidDeviceInfoPtr>> get_devices_future;
    hid_chooser_context->SetHidManagerForTesting(
        std::move(hid_manager), get_devices_future.GetCallback());
    EXPECT_TRUE(get_devices_future.Wait());
  }

  void SetUpPolicyGrantedPermissions() override {
    auto* local_state = TestingBrowserProcess::GetGlobal()->local_state();
    ASSERT_TRUE(local_state);
    local_state->Set(prefs::kManagedWebHidAllowDevicesForUrls, ParseJson(R"(
        [
          {
            "devices": [{ "vendor_id": 6353, "product_id": 5678 }],
            "urls": ["https://chromium.org"]
          }, {
            "devices": [{ "vendor_id": 6353 }],
            "urls": ["https://android.com"]
          }, {
            "devices": [{ "vendor_id": 6354 }],
            "urls": ["https://android.com"]
          }
        ])"));
    local_state->Set(prefs::kManagedWebHidAllowAllDevicesForUrls,
                     ParseJson(R"([ "https://google.com" ])"));
  }

  void SetUpOffTheRecordChooserContext() override {
    CreateIncognitoProfile();
    mojo::PendingRemote<device::mojom::HidManager> hid_manager;
    hid_manager_.AddReceiver(hid_manager.InitWithNewPipeAndPassReceiver());
    HidChooserContext* hid_chooser_context =
        HidChooserContextFactory::GetForProfile(incognito_profile());
    TestFuture<std::vector<device::mojom::HidDeviceInfoPtr>> get_devices_future;
    hid_chooser_context->SetHidManagerForTesting(
        std::move(hid_manager), get_devices_future.GetCallback());
    EXPECT_TRUE(get_devices_future.Wait());
    GetChooserContext(incognito_profile())->AddObserver(&observer_);
  }

  void AddPersistentDevice() override {
    persistent_device_ = hid_manager_.CreateAndAddDevice(
        /*physical_device_id=*/"1", /*vendor_id=*/6353, /*product_id=*/5678,
        /*product_name=*/"persistent-device", /*serial_number=*/"123ABC",
        device::mojom::HidBusType::kHIDBusTypeUSB);
  }

  void AddEphemeralDevice() override {
    ephemeral_device_ = hid_manager_.CreateAndAddDevice(
        /*physical_device_id=*/"2", /*vendor_id=*/6354, /*product_id=*/0,
        /*product_name=*/"ephemeral-device", /*serial_number=*/"",
        device::mojom::HidBusType::kHIDBusTypeUSB);
  }

  void AddUserGrantedDevice() override {
    user_granted_device_ = hid_manager_.CreateAndAddDevice(
        /*physical_device_id=*/"3", /*vendor_id=*/6355, /*product_id=*/0,
        /*product_name=*/"user-granted-device", /*serial_number=*/"789XYZ",
        device::mojom::HidBusType::kHIDBusTypeUSB);
  }

  void AddOffTheRecordDevice() override {
    off_the_record_device_ = hid_manager_.CreateAndAddDevice(
        /*physical_device_id=*/"4", /*vendor_id=*/6353,
        /*product_id=*/8765, /*product_name=*/"off-the-record-device",
        /*serial_number=*/"A9B8C7", device::mojom::HidBusType::kHIDBusTypeUSB);
  }

  void SetUpUserGrantedPermissions() override {
    const auto kAndroidOrigin = url::Origin::Create(kAndroidUrl);
    const auto kChromiumOrigin = url::Origin::Create(kChromiumUrl);
    const auto kGoogleOrigin = url::Origin::Create(kGoogleUrl);
    const auto kWebUIOrigin = url::Origin::Create(kWebUIUrl);

    // Add the user granted permissions for testing.
    // These two persistent device permissions should be lumped together
    // with the policy permissions, since they apply to the same device and
    // URL.
    {
      base::RunLoop loop;
      auto barrier_closure = base::BarrierClosure(5, loop.QuitClosure());
      EXPECT_CALL(observer_, OnObjectPermissionChanged(
                                 {ContentSettingsType::HID_GUARD},
                                 ContentSettingsType::HID_CHOOSER_DATA))
          .Times(5)
          .WillRepeatedly(RunClosure(barrier_closure));
      auto* hid_chooser_context =
          HidChooserContextFactory::GetForProfile(profile());
      hid_chooser_context->GrantDevicePermission(kChromiumOrigin,
                                                 *persistent_device_);
      hid_chooser_context->GrantDevicePermission(kGoogleOrigin,
                                                 *persistent_device_);
      hid_chooser_context->GrantDevicePermission(kWebUIOrigin,
                                                 *persistent_device_);
      hid_chooser_context->GrantDevicePermission(kAndroidOrigin,
                                                 *ephemeral_device_);
      hid_chooser_context->GrantDevicePermission(kAndroidOrigin,
                                                 *user_granted_device_);
      loop.Run();
    }

    if (off_the_record_device_) {
      base::RunLoop loop;
      EXPECT_CALL(observer_, OnObjectPermissionChanged(
                                 {ContentSettingsType::HID_GUARD},
                                 ContentSettingsType::HID_CHOOSER_DATA))
          .WillOnce(RunClosure(loop.QuitClosure()));
      HidChooserContextFactory::GetForProfile(incognito_profile())
          ->GrantDevicePermission(kChromiumOrigin, *off_the_record_device_);
      loop.Run();
    }
  }

  void SetUpUserGrantedPermissionForOrigin(const url::Origin& origin) override {
    auto* hid_chooser_context =
        HidChooserContextFactory::GetForProfile(profile());
    base::RunLoop loop;
    EXPECT_CALL(observer_, OnObjectPermissionChanged(
                               {ContentSettingsType::HID_GUARD},
                               ContentSettingsType::HID_CHOOSER_DATA))
        .WillOnce(RunClosure(loop.QuitClosure()));
    hid_chooser_context->GrantDevicePermission(origin, *user_granted_device_);
    loop.Run();
  }

  base::Value GetPersistentDeviceValueForOrigin(
      const url::Origin& origin) override {
    return base::Value(
        HidChooserContext::DeviceInfoToValue(*persistent_device_));
  }

  base::Value GetUserGrantedDeviceValueForOrigin(
      const url::Origin& origin) override {
    return base::Value(
        HidChooserContext::DeviceInfoToValue(*user_granted_device_));
  }

  std::string GetAllDevicesDisplayName() override { return "Any HID device"; }

  std::string GetDevicesFromGoogleDisplayName() override {
    return "HID devices from vendor 18D1";
  }

  std::string GetDevicesFromVendor18D2DisplayName() override {
    return "HID devices from vendor 18D2";
  }

  std::string GetUnknownProductDisplayName() override {
    return "HID device (18D1:162E)";
  }

  permissions::ObjectPermissionContextBase* GetChooserContext(
      Profile* profile) override {
    return HidChooserContextFactory::GetForProfile(profile);
  }

  ContentSettingsType content_type() override {
    return ContentSettingsType::HID_CHOOSER_DATA;
  }

  ContentSettingsType guard_type() override {
    return ContentSettingsType::HID_GUARD;
  }

  device::FakeHidManager hid_manager_;
  device::mojom::HidDeviceInfoPtr ephemeral_device_;
  device::mojom::HidDeviceInfoPtr off_the_record_device_;
  device::mojom::HidDeviceInfoPtr persistent_device_;
  device::mojom::HidDeviceInfoPtr user_granted_device_;
};

TEST_F(SiteSettingsHandlerHidTest, HandleGetChooserExceptionList) {
  TestHandleGetChooserExceptionList();
}

TEST_F(SiteSettingsHandlerHidTest,
       HandleGetChooserExceptionListForOffTheRecord) {
  TestHandleGetChooserExceptionListForOffTheRecord();
}

TEST_F(SiteSettingsHandlerHidTest, HandleResetChooserExceptionForSite) {
  TestHandleResetChooserExceptionForSite();
}

TEST_F(SiteSettingsHandlerHidTest, HandleSetOriginPermissions) {
  TestHandleSetOriginPermissions();
}

TEST_F(SiteSettingsHandlerHidTest, HandleSetOriginPermissionsPolicyOnly) {
  TestHandleSetOriginPermissionsPolicyOnly();
}

class SiteSettingsHandlerSerialTest
    : public SiteSettingsHandlerChooserExceptionTest {
 protected:
  void SetUpChooserContext() override {
    mojo::PendingRemote<device::mojom::SerialPortManager> serial_port_manager;
    serial_port_manager_.AddReceiver(
        serial_port_manager.InitWithNewPipeAndPassReceiver());
    SerialChooserContext* serial_chooser_context =
        SerialChooserContextFactory::GetForProfile(profile());
    serial_chooser_context->SetPortManagerForTesting(
        std::move(serial_port_manager));
    base::RunLoop().RunUntilIdle();
  }

  void SetUpPolicyGrantedPermissions() override {
    auto* local_state = TestingBrowserProcess::GetGlobal()->local_state();
    ASSERT_TRUE(local_state);
    local_state->Set(prefs::kManagedSerialAllowUsbDevicesForUrls, ParseJson(R"(
        [
          {
            "devices": [{ "vendor_id": 6353, "product_id": 5678 }],
            "urls": ["https://chromium.org"]
          }, {
            "devices": [{ "vendor_id": 6353 }],
            "urls": ["https://android.com"]
          }, {
            "devices": [{ "vendor_id": 6354 }],
            "urls": ["https://android.com"]
          }
        ])"));
    local_state->Set(prefs::kManagedSerialAllowAllPortsForUrls,
                     ParseJson(R"([ "https://google.com" ])"));
  }

  void SetUpOffTheRecordChooserContext() override {
    CreateIncognitoProfile();
    mojo::PendingRemote<device::mojom::SerialPortManager> serial_port_manager;
    serial_port_manager_.AddReceiver(
        serial_port_manager.InitWithNewPipeAndPassReceiver());
    SerialChooserContext* serial_chooser_context =
        SerialChooserContextFactory::GetForProfile(incognito_profile());
    serial_chooser_context->SetPortManagerForTesting(
        std::move(serial_port_manager));
    base::RunLoop().RunUntilIdle();
    GetChooserContext(incognito_profile())->AddObserver(&observer_);
  }

  void AddPersistentDevice() override {
    persistent_port_ = device::mojom::SerialPortInfo::New();
    persistent_port_->token = base::UnguessableToken::Create();
    persistent_port_->display_name = "persistent-device";
#if BUILDFLAG(IS_WIN)
    persistent_port_->device_instance_id = "1";
#else
    persistent_port_->has_vendor_id = true;
    persistent_port_->vendor_id = 6353;
    persistent_port_->has_product_id = true;
    persistent_port_->product_id = 5678;
    persistent_port_->serial_number = "123ABC";
#if BUILDFLAG(IS_MAC)
    persistent_port_->usb_driver_name = "AppleUSBCDC";
#endif
#endif  // BUILDFLAG(IS_WIN)
    serial_port_manager_.AddPort(persistent_port_.Clone());
  }

  void AddEphemeralDevice() override {
    ephemeral_port_ = device::mojom::SerialPortInfo::New();
    ephemeral_port_->token = base::UnguessableToken::Create();
    ephemeral_port_->display_name = "ephemeral-device";
#if BUILDFLAG(IS_WIN)
    ephemeral_port_->device_instance_id = "2";
#else
    ephemeral_port_->has_vendor_id = true;
    ephemeral_port_->vendor_id = 6354;
    ephemeral_port_->has_product_id = true;
    ephemeral_port_->product_id = 0;
#if BUILDFLAG(IS_MAC)
    ephemeral_port_->usb_driver_name = "AppleUSBCDC";
#endif
#endif  // BUILDFLAG(IS_WIN)
    serial_port_manager_.AddPort(ephemeral_port_.Clone());
  }

  void AddUserGrantedDevice() override {
    user_granted_port_ = device::mojom::SerialPortInfo::New();
    user_granted_port_->token = base::UnguessableToken::Create();
    user_granted_port_->display_name = "user-granted-device";
#if BUILDFLAG(IS_WIN)
    user_granted_port_->device_instance_id = "3";
#else
    user_granted_port_->has_vendor_id = true;
    user_granted_port_->vendor_id = 6355;
    user_granted_port_->has_product_id = true;
    user_granted_port_->product_id = 0;
    user_granted_port_->serial_number = "789XYZ";
#if BUILDFLAG(IS_MAC)
    user_granted_port_->usb_driver_name = "AppleUSBCDC";
#endif
#endif  // BUILDFLAG(IS_WIN)
    serial_port_manager_.AddPort(user_granted_port_.Clone());
  }

  void AddOffTheRecordDevice() override {
    off_the_record_port_ = device::mojom::SerialPortInfo::New();
    off_the_record_port_->token = base::UnguessableToken::Create();
    off_the_record_port_->display_name = "off-the-record-device";
#if BUILDFLAG(IS_WIN)
    off_the_record_port_->device_instance_id = "4";
#else
    off_the_record_port_->has_vendor_id = true;
    off_the_record_port_->vendor_id = 6353;
    off_the_record_port_->has_product_id = true;
    off_the_record_port_->product_id = 8765;
    off_the_record_port_->serial_number = "A9B8C7";
#if BUILDFLAG(IS_MAC)
    off_the_record_port_->usb_driver_name = "AppleUSBCDC";
#endif
#endif  // BUILDFLAG(IS_WIN)
    serial_port_manager_.AddPort(off_the_record_port_.Clone());
  }

  void SetUpUserGrantedPermissions() override {
    const auto kAndroidOrigin = url::Origin::Create(kAndroidUrl);
    const auto kChromiumOrigin = url::Origin::Create(kChromiumUrl);
    const auto kGoogleOrigin = url::Origin::Create(kGoogleUrl);
    const auto kWebUIOrigin = url::Origin::Create(kWebUIUrl);

    // Add the user granted permissions for testing.
    // These two persistent device permissions should be lumped together
    // with the policy permissions, since they apply to the same device and
    // URL.
    {
      base::RunLoop loop;
      auto barrier_closure = base::BarrierClosure(5, loop.QuitClosure());
      EXPECT_CALL(observer_, OnObjectPermissionChanged(
                                 {ContentSettingsType::SERIAL_GUARD},
                                 ContentSettingsType::SERIAL_CHOOSER_DATA))
          .Times(5)
          .WillRepeatedly(RunClosure(barrier_closure));
      auto* serial_chooser_context =
          SerialChooserContextFactory::GetForProfile(profile());
      serial_chooser_context->GrantPortPermission(kChromiumOrigin,
                                                  *persistent_port_);
      serial_chooser_context->GrantPortPermission(kGoogleOrigin,
                                                  *persistent_port_);
      serial_chooser_context->GrantPortPermission(kWebUIOrigin,
                                                  *persistent_port_);
      serial_chooser_context->GrantPortPermission(kAndroidOrigin,
                                                  *ephemeral_port_);
      serial_chooser_context->GrantPortPermission(kAndroidOrigin,
                                                  *user_granted_port_);
      loop.Run();
    }

    if (off_the_record_port_) {
      base::RunLoop loop;
      EXPECT_CALL(observer_, OnObjectPermissionChanged(
                                 {ContentSettingsType::SERIAL_GUARD},
                                 ContentSettingsType::SERIAL_CHOOSER_DATA))
          .WillOnce(RunClosure(loop.QuitClosure()));
      SerialChooserContextFactory::GetForProfile(incognito_profile())
          ->GrantPortPermission(kChromiumOrigin, *off_the_record_port_);
      loop.Run();
    }
  }

  void SetUpUserGrantedPermissionForOrigin(const url::Origin& origin) override {
    auto* serial_chooser_context =
        SerialChooserContextFactory::GetForProfile(profile());
    base::RunLoop loop;
    EXPECT_CALL(observer_, OnObjectPermissionChanged(
                               {ContentSettingsType::SERIAL_GUARD},
                               ContentSettingsType::SERIAL_CHOOSER_DATA))
        .WillOnce(RunClosure(loop.QuitClosure()));
    serial_chooser_context->GrantPortPermission(origin, *user_granted_port_);
    loop.Run();
  }

  base::Value GetPersistentDeviceValueForOrigin(
      const url::Origin& origin) override {
    return base::Value(
        SerialChooserContext::PortInfoToValue(*persistent_port_));
  }

  base::Value GetUserGrantedDeviceValueForOrigin(
      const url::Origin& origin) override {
    return base::Value(
        SerialChooserContext::PortInfoToValue(*user_granted_port_));
  }

  std::string GetAllDevicesDisplayName() override { return "Any serial port"; }

  std::string GetDevicesFromGoogleDisplayName() override {
    return "USB devices from Google Inc.";
  }

  std::string GetDevicesFromVendor18D2DisplayName() override {
    return "USB devices from vendor 18D2";
  }

  std::string GetUnknownProductDisplayName() override {
    return "USB device from Google Inc. (product 162E)";
  }

  permissions::ObjectPermissionContextBase* GetChooserContext(
      Profile* profile) override {
    return SerialChooserContextFactory::GetForProfile(profile);
  }

  ContentSettingsType content_type() override {
    return ContentSettingsType::SERIAL_CHOOSER_DATA;
  }

  ContentSettingsType guard_type() override {
    return ContentSettingsType::SERIAL_GUARD;
  }

  device::FakeSerialPortManager serial_port_manager_;
  device::mojom::SerialPortInfoPtr ephemeral_port_;
  device::mojom::SerialPortInfoPtr off_the_record_port_;
  device::mojom::SerialPortInfoPtr persistent_port_;
  device::mojom::SerialPortInfoPtr user_granted_port_;
};

TEST_F(SiteSettingsHandlerSerialTest, HandleGetChooserExceptionList) {
  TestHandleGetChooserExceptionList();
}

TEST_F(SiteSettingsHandlerSerialTest,
       HandleGetChooserExceptionListForOffTheRecord) {
  TestHandleGetChooserExceptionListForOffTheRecord();
}

TEST_F(SiteSettingsHandlerSerialTest, HandleResetChooserExceptionForSite) {
  TestHandleResetChooserExceptionForSite();
}

TEST_F(SiteSettingsHandlerSerialTest, HandleSetOriginPermissions) {
  TestHandleSetOriginPermissions();
}

TEST_F(SiteSettingsHandlerSerialTest, HandleSetOriginPermissionsPolicyOnly) {
  TestHandleSetOriginPermissionsPolicyOnly();
}

class SiteSettingsHandlerUsbTest
    : public SiteSettingsHandlerChooserExceptionTest {
 protected:
  void SetUpChooserContext() override {
    mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager;
    usb_device_manager_.AddReceiver(
        device_manager.InitWithNewPipeAndPassReceiver());
    UsbChooserContext* usb_chooser_context =
        UsbChooserContextFactory::GetForProfile(profile());
    usb_chooser_context->SetDeviceManagerForTesting(std::move(device_manager));
    TestFuture<std::vector<device::mojom::UsbDeviceInfoPtr>> get_devices_future;
    usb_chooser_context->GetDevices(get_devices_future.GetCallback());
    EXPECT_TRUE(get_devices_future.Wait());
  }

  void SetUpPolicyGrantedPermissions() override {
    profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                               ParseJson(R"(
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
        ])"));
  }

  void SetUpOffTheRecordChooserContext() override {
    CreateIncognitoProfile();
    mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager;
    usb_device_manager_.AddReceiver(
        device_manager.InitWithNewPipeAndPassReceiver());
    UsbChooserContext* usb_chooser_context =
        UsbChooserContextFactory::GetForProfile(incognito_profile());
    usb_chooser_context->SetDeviceManagerForTesting(std::move(device_manager));
    TestFuture<std::vector<device::mojom::UsbDeviceInfoPtr>> get_devices_future;
    usb_chooser_context->GetDevices(get_devices_future.GetCallback());
    EXPECT_TRUE(get_devices_future.Wait());
    GetChooserContext(incognito_profile())->AddObserver(&observer_);
  }

  void AddPersistentDevice() override {
    persistent_device_ = usb_device_manager_.CreateAndAddDevice(
        /*vendor_id=*/6353, /*product_id=*/5678,
        /*manufacturer_string=*/"Google",
        /*product_string=*/"persistent-device", /*serial_number=*/"123ABC");
  }

  void AddEphemeralDevice() override {
    ephemeral_device_ = usb_device_manager_.CreateAndAddDevice(
        /*vendor_id=*/6354, /*product_id=*/0,
        /*manufacturer_string=*/"Google",
        /*product_string=*/"ephemeral-device", /*serial_number=*/"");
  }

  void AddUserGrantedDevice() override {
    user_granted_device_ = usb_device_manager_.CreateAndAddDevice(
        /*vendor_id=*/6355, /*product_id=*/0,
        /*manufacturer_string=*/"Google",
        /*product_string=*/"user-granted-device",
        /*serial_number=*/"789XYZ");
  }

  void AddOffTheRecordDevice() override {
    off_the_record_device_ = usb_device_manager_.CreateAndAddDevice(
        /*vendor_id=*/6353, /*product_id=*/8765,
        /*manufacturer_string=*/"Google",
        /*product_string=*/"off-the-record-device",
        /*serial_number=*/"A9B8C7");
  }

  void SetUpUserGrantedPermissions() override {
    const auto kAndroidOrigin = url::Origin::Create(kAndroidUrl);
    const auto kChromiumOrigin = url::Origin::Create(kChromiumUrl);
    const auto kGoogleOrigin = url::Origin::Create(kGoogleUrl);
    const auto kWebUIOrigin = url::Origin::Create(kWebUIUrl);

    // Add the user granted permissions for testing.
    // These two persistent device permissions should be lumped together
    // with the policy permissions, since they apply to the same device and
    // URL.
    {
      base::RunLoop loop;
      auto barrier_closure = base::BarrierClosure(5, loop.QuitClosure());
      EXPECT_CALL(observer_, OnObjectPermissionChanged(
                                 {ContentSettingsType::USB_GUARD},
                                 ContentSettingsType::USB_CHOOSER_DATA))
          .Times(5)
          .WillRepeatedly(RunClosure(barrier_closure));
      auto* usb_chooser_context =
          UsbChooserContextFactory::GetForProfile(profile());
      usb_chooser_context->GrantDevicePermission(kChromiumOrigin,
                                                 *persistent_device_);
      usb_chooser_context->GrantDevicePermission(kGoogleOrigin,
                                                 *persistent_device_);
      usb_chooser_context->GrantDevicePermission(kWebUIOrigin,
                                                 *persistent_device_);
      usb_chooser_context->GrantDevicePermission(kAndroidOrigin,
                                                 *ephemeral_device_);
      usb_chooser_context->GrantDevicePermission(kAndroidOrigin,
                                                 *user_granted_device_);
      loop.Run();
    }

    if (off_the_record_device_) {
      base::RunLoop loop;
      EXPECT_CALL(observer_, OnObjectPermissionChanged(
                                 {ContentSettingsType::USB_GUARD},
                                 ContentSettingsType::USB_CHOOSER_DATA))
          .WillOnce(RunClosure(loop.QuitClosure()));
      UsbChooserContextFactory::GetForProfile(incognito_profile())
          ->GrantDevicePermission(kChromiumOrigin, *off_the_record_device_);
      loop.Run();
    }
  }

  void SetUpUserGrantedPermissionForOrigin(const url::Origin& origin) override {
    auto* usb_chooser_context =
        UsbChooserContextFactory::GetForProfile(profile());
    base::RunLoop loop;
    EXPECT_CALL(observer_, OnObjectPermissionChanged(
                               {ContentSettingsType::USB_GUARD},
                               ContentSettingsType::USB_CHOOSER_DATA))
        .WillOnce(RunClosure(loop.QuitClosure()));
    usb_chooser_context->GrantDevicePermission(origin, *user_granted_device_);
    loop.Run();
  }

  base::Value GetPersistentDeviceValueForOrigin(
      const url::Origin& origin) override {
    return base::Value(
        UsbChooserContext::DeviceInfoToValue(*persistent_device_));
  }

  base::Value GetUserGrantedDeviceValueForOrigin(
      const url::Origin& origin) override {
    return base::Value(
        UsbChooserContext::DeviceInfoToValue(*user_granted_device_));
  }

  std::string GetAllDevicesDisplayName() override {
    return "Devices from any vendor";
  }

  std::string GetDevicesFromGoogleDisplayName() override {
    return "Devices from Google Inc.";
  }

  std::string GetDevicesFromVendor18D2DisplayName() override {
    return "Devices from vendor 0x18D2";
  }

  std::string GetUnknownProductDisplayName() override {
    return "Unknown product 0x162E from Google Inc.";
  }

  permissions::ObjectPermissionContextBase* GetChooserContext(
      Profile* profile) override {
    return UsbChooserContextFactory::GetForProfile(profile);
  }

  ContentSettingsType content_type() override {
    return ContentSettingsType::USB_CHOOSER_DATA;
  }

  ContentSettingsType guard_type() override {
    return ContentSettingsType::USB_GUARD;
  }

  device::FakeUsbDeviceManager usb_device_manager_;
  device::mojom::UsbDeviceInfoPtr ephemeral_device_;
  device::mojom::UsbDeviceInfoPtr off_the_record_device_;
  device::mojom::UsbDeviceInfoPtr persistent_device_;
  device::mojom::UsbDeviceInfoPtr user_granted_device_;
};

TEST_F(SiteSettingsHandlerUsbTest, HandleGetChooserExceptionList) {
  TestHandleGetChooserExceptionList();
}

TEST_F(SiteSettingsHandlerUsbTest,
       HandleGetChooserExceptionListForOffTheRecord) {
  TestHandleGetChooserExceptionListForOffTheRecord();
}

TEST_F(SiteSettingsHandlerUsbTest, HandleResetChooserExceptionForSite) {
  TestHandleResetChooserExceptionForSite();
}

TEST_F(SiteSettingsHandlerUsbTest, HandleSetOriginPermissions) {
  TestHandleSetOriginPermissions();
}

TEST_F(SiteSettingsHandlerUsbTest, HandleSetOriginPermissionsPolicyOnly) {
  TestHandleSetOriginPermissionsPolicyOnly();
}

TEST_F(SiteSettingsHandlerTest, HandleClearSiteGroupDataAndCookies) {
  SetupModel();
  std::vector<browsing_data_model_test_util::BrowsingDataEntry>
      expected_browsing_data_model_entries = {
          kGoogleUnpartitionedEntry,
          kExampleUnpartitionedEntry,
          kGoogleOnExampleEntry,
          kExampleOnGoogleSecureEntry,
          kExampleOnGoogleInsecureEntry,
          kExampleLocalStorage,
          kHttpExampleCookie,
          kHttpsWwwExampleCookie,
          kPartitionedHttpsWwwExampleOnGoogleAuCookie,
          kPartitionedHttpsWwwExampleOnGoogleCookie,
          kHttpAbcExampleCookie,
          kHttpGoogleCookieA,
          kHttpGoogleCookieB,
          kHttpGoogleAuCookie,
          kPartitionedHttpsGoogleAu1PCookie,
          kPartitionedHttpsWwwAnotherExampleOnGoogleAuCookie,
          kUngroupedHttpCookie,
      };
  browsing_data_model_test_util::ValidateBrowsingDataEntries(
      handler()->GetBrowsingDataModelForTesting(),
      expected_browsing_data_model_entries);

  auto verify_site_group = [](const base::Value& site_group,
                              std::string expected_etld_plus1) {
    ASSERT_TRUE(site_group.is_dict());
    ASSERT_THAT(CHECK_DEREF(site_group.GetDict().FindString("groupingKey")),
                IsEtldPlus1(expected_etld_plus1));
  };

  base::Value::List storage_and_cookie_list = GetOnStorageFetchedSentList();
  EXPECT_EQ(4U, storage_and_cookie_list.size());
  verify_site_group(storage_and_cookie_list[0], "example.com");
  verify_site_group(storage_and_cookie_list[1], "google.com");
  verify_site_group(storage_and_cookie_list[2], "google.com.au");
  verify_site_group(storage_and_cookie_list[3], "ungrouped.com");

  base::Value::List args;
  args.Append(GroupingKey::CreateFromEtldPlus1("example.com").Serialize());
  handler()->HandleClearSiteGroupDataAndCookies(args);

  // Items partitioned on example.com, as well as unpartitioned example.com
  // storage should be removed.
  RemoveModelEntries(expected_browsing_data_model_entries,
                     {
                         kExampleUnpartitionedEntry,
                         kExampleLocalStorage,
                         kHttpExampleCookie,
                         kHttpsWwwExampleCookie,
                         kHttpAbcExampleCookie,
                         kGoogleOnExampleEntry,
                     });
  browsing_data_model_test_util::ValidateBrowsingDataEntries(
      handler()->GetBrowsingDataModelForTesting(),
      expected_browsing_data_model_entries);

  storage_and_cookie_list = GetOnStorageFetchedSentList();
  EXPECT_EQ(3U, storage_and_cookie_list.size());
  verify_site_group(storage_and_cookie_list[0], "google.com");
  verify_site_group(storage_and_cookie_list[1], "google.com.au");
  verify_site_group(storage_and_cookie_list[2], "ungrouped.com");

  args.clear();
  args.Append(GroupingKey::CreateFromEtldPlus1("google.com").Serialize());

  handler()->HandleClearSiteGroupDataAndCookies(args);
  RemoveModelEntries(expected_browsing_data_model_entries,
                     {
                         kGoogleUnpartitionedEntry,
                         kExampleOnGoogleSecureEntry,
                         kExampleOnGoogleInsecureEntry,
                         kHttpGoogleCookieA,
                         kHttpGoogleCookieB,
                         kPartitionedHttpsWwwExampleOnGoogleCookie,
                     });
  browsing_data_model_test_util::ValidateBrowsingDataEntries(
      handler()->GetBrowsingDataModelForTesting(),
      expected_browsing_data_model_entries);

  storage_and_cookie_list = GetOnStorageFetchedSentList();
  EXPECT_EQ(2U, storage_and_cookie_list.size());
  verify_site_group(storage_and_cookie_list[0], "google.com.au");
  verify_site_group(storage_and_cookie_list[1], "ungrouped.com");

  args.clear();
  args.Append(GroupingKey::CreateFromEtldPlus1("google.com.au").Serialize());

  handler()->HandleClearSiteGroupDataAndCookies(args);
  // No nodes representing storage partitioned on google.com.au should be
  // present.
  RemoveModelEntries(expected_browsing_data_model_entries,
                     {
                         kPartitionedHttpsWwwExampleOnGoogleAuCookie,
                         kHttpGoogleAuCookie,
                         kPartitionedHttpsGoogleAu1PCookie,
                         kPartitionedHttpsWwwAnotherExampleOnGoogleAuCookie,
                     });
  browsing_data_model_test_util::ValidateBrowsingDataEntries(
      handler()->GetBrowsingDataModelForTesting(),
      expected_browsing_data_model_entries);

  storage_and_cookie_list = GetOnStorageFetchedSentList();
  EXPECT_EQ(1U, storage_and_cookie_list.size());
  verify_site_group(storage_and_cookie_list[0], "ungrouped.com");

  args.clear();
  args.Append(GroupingKey::CreateFromEtldPlus1("ungrouped.com").Serialize());

  handler()->HandleClearSiteGroupDataAndCookies(args);

  storage_and_cookie_list = GetOnStorageFetchedSentList();
  EXPECT_EQ(0U, storage_and_cookie_list.size());
}

#if BUILDFLAG(IS_WIN)
TEST_F(SiteSettingsHandlerTest, ClearSiteSpecificMediaLicenses) {
  SetupModel();
  PrefService* user_prefs = profile()->GetPrefs();

  // In the beginning, there should be nothing stored in the origin data.
  ASSERT_EQ(0u, user_prefs->GetDict(prefs::kMediaCdmOriginData).size());

  auto entry_google = base::Value::Dict().Set(
      "https://www.google.com/",
      base::UnguessableTokenToValue(base::UnguessableToken::Create()));

  base::Value::Dict entry_example;
  entry_example.Set(
      "https://www.example.com/",
      base::UnguessableTokenToValue(base::UnguessableToken::Create()));

  {
    ScopedDictPrefUpdate update(user_prefs, prefs::kMediaCdmOriginData);

    base::Value::Dict& dict = update.Get();
    dict.Set("https://www.google.com/", std::move(entry_google));
    dict.Set("https://www.example.com/", std::move(entry_example));
  }
  // The code above adds origin data for both google and example.com
  EXPECT_EQ(2u, user_prefs->GetDict(prefs::kMediaCdmOriginData).size());

  base::Value::List args;
  args = base::Value::List();
  args.Append("https://www.google.com/");
  handler()->HandleClearUnpartitionedUsage(args);

  // The code clears the origin data for just google.com, so there should still
  // be the origin data for example.com left.
  EXPECT_EQ(1u, user_prefs->GetDict(prefs::kMediaCdmOriginData).size());
  EXPECT_TRUE(user_prefs->GetDict(prefs::kMediaCdmOriginData)
                  .contains("https://www.example.com/"));
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(SiteSettingsHandlerTest, ClearClientHints) {
  // Confirm that when the user clears unpartitioned storage, or the eTLD+1
  // group, client hints are also cleared.
  SetupModel();
  handler()->OnStorageFetched();

  GURL hosts[] = {GURL("https://example.com/"), GURL("https://www.example.com"),
                  GURL("https://google.com/"), GURL("https://www.google.com/")};

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  // Add setting for the two hosts host[0], host[1].
  base::Value client_hint_platform_version(14);
  base::Value client_hint_bitness(16);

  base::Value::List client_hints_list;
  client_hints_list.Append(std::move(client_hint_platform_version));
  client_hints_list.Append(std::move(client_hint_bitness));

  base::Value::Dict client_hints_dictionary;
  client_hints_dictionary.Set(client_hints::kClientHintsSettingKey,
                              std::move(client_hints_list));

  // Add setting for the hosts.
  for (const auto& host : hosts) {
    host_content_settings_map->SetWebsiteSettingDefaultScope(
        host, GURL(), ContentSettingsType::CLIENT_HINTS,
        base::Value(client_hints_dictionary.Clone()));
  }

  // Clear at the eTLD+1 level and ensure affected origins are cleared.
  base::Value::List args;
  args.Append(GroupingKey::CreateFromEtldPlus1("example.com").Serialize());
  handler()->HandleClearSiteGroupDataAndCookies(args);
  ContentSettingsForOneType client_hints_settings =
      host_content_settings_map->GetSettingsForOneType(
          ContentSettingsType::CLIENT_HINTS);
  EXPECT_EQ(2U, client_hints_settings.size());

  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[3]),
            client_hints_settings.at(0).primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            client_hints_settings.at(0).secondary_pattern);
  EXPECT_EQ(client_hints_dictionary, client_hints_settings.at(0).setting_value);

  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[2]),
            client_hints_settings.at(1).primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            client_hints_settings.at(1).secondary_pattern);
  EXPECT_EQ(client_hints_dictionary, client_hints_settings.at(1).setting_value);

  // Clear unpartitioned usage data, which should only affect the specific
  // origin.
  args.clear();
  args.Append("https://google.com/");
  handler()->HandleClearUnpartitionedUsage(args);

  // Validate the client hint has been cleared.
  client_hints_settings = host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS);
  EXPECT_EQ(1U, client_hints_settings.size());

  // www.google.com should be the only remaining entry.
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[3]),
            client_hints_settings.at(0).primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            client_hints_settings.at(0).secondary_pattern);
  EXPECT_EQ(client_hints_dictionary, client_hints_settings.at(0).setting_value);

  // Clear unpartitioned usage data through HTTPS scheme, make sure https site
  // client hints have been cleared when the specific origin HTTPS scheme
  // exist.
  args.clear();
  args.Append("http://www.google.com/");
  handler()->HandleClearUnpartitionedUsage(args);

  // Validate the client hint has been cleared.
  client_hints_settings = host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS);
  EXPECT_EQ(0U, client_hints_settings.size());
}

TEST_F(SiteSettingsHandlerTest, ClearReducedAcceptLanguage) {
  // Confirm that when the user clears unpartitioned storage, or the eTLD+1
  // group, reduce accept language are also cleared.
  SetupModel();
  handler()->OnStorageFetched();

  GURL hosts[] = {GURL("https://example.com/"), GURL("https://www.example.com"),
                  GURL("https://google.com/"), GURL("https://www.google.com/")};

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  std::string language = "en-us";
  base::Value::Dict accept_language_dictionary;
  accept_language_dictionary.Set("reduce-accept-language", language);

  // Add setting for the hosts.
  for (const auto& host : hosts) {
    host_content_settings_map->SetWebsiteSettingDefaultScope(
        host, GURL(), ContentSettingsType::REDUCED_ACCEPT_LANGUAGE,
        base::Value(accept_language_dictionary.Clone()));
  }

  // Clear at the eTLD+1 level and ensure affected origins are cleared.
  base::Value::List args;
  args.Append(GroupingKey::CreateFromEtldPlus1("example.com").Serialize());
  handler()->HandleClearSiteGroupDataAndCookies(args);
  ContentSettingsForOneType accept_language_settings =
      host_content_settings_map->GetSettingsForOneType(
          ContentSettingsType::REDUCED_ACCEPT_LANGUAGE);
  EXPECT_EQ(2U, accept_language_settings.size());

  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[3]),
            accept_language_settings.at(0).primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            accept_language_settings.at(0).secondary_pattern);
  EXPECT_EQ(accept_language_dictionary,
            accept_language_settings.at(0).setting_value);

  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[2]),
            accept_language_settings.at(1).primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            accept_language_settings.at(1).secondary_pattern);
  EXPECT_EQ(accept_language_dictionary,
            accept_language_settings.at(1).setting_value);

  // Clear unpartitioned usage data, which should only affect the specific
  // origin.
  args.clear();
  args.Append("https://google.com/");
  handler()->HandleClearUnpartitionedUsage(args);

  // Validate the reduce accept language has been cleared.
  accept_language_settings = host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::REDUCED_ACCEPT_LANGUAGE);
  EXPECT_EQ(1U, accept_language_settings.size());

  // www.google.com should be the only remaining entry.
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[3]),
            accept_language_settings.at(0).primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            accept_language_settings.at(0).secondary_pattern);
  EXPECT_EQ(accept_language_dictionary,
            accept_language_settings.at(0).setting_value);

  // Clear unpartitioned usage data through HTTPS scheme, make sure https site
  // reduced accept language have been cleared when the specific origin HTTPS
  // scheme exist.
  args.clear();
  args.Append("http://www.google.com/");
  handler()->HandleClearUnpartitionedUsage(args);

  // Validate the reduced accept language has been cleared.
  accept_language_settings = host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::REDUCED_ACCEPT_LANGUAGE);
  EXPECT_EQ(0U, accept_language_settings.size());
}

TEST_F(SiteSettingsHandlerTest, ClearDurableStorage) {
  // Confirm that when the user clears durable storage or the eTLD+1
  // group, durable storage are also cleared.
  SetupModel();
  handler()->OnStorageFetched();

  GURL hosts[] = {GURL("https://example.com/"), GURL("https://www.example.com"),
                  GURL("https://google.com/"), GURL("https://www.google.com/")};

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  // Add setting for the hosts.
  for (const auto& host : hosts) {
    host_content_settings_map->SetContentSettingDefaultScope(
        host, GURL(), ContentSettingsType::DURABLE_STORAGE,
        ContentSetting::CONTENT_SETTING_ALLOW);
  }

  // Clear at the eTLD+1 level and ensure affected origins are cleared.
  base::Value::List args;
  args.Append(GroupingKey::CreateFromEtldPlus1("example.com").Serialize());
  handler()->HandleClearSiteGroupDataAndCookies(args);
  ContentSettingsForOneType settings =
      host_content_settings_map->GetSettingsForOneType(
          ContentSettingsType::DURABLE_STORAGE);

  // ContentSettingsType::DURABLE_STORAGE has a default settings type for the
  // wildcard '*' set to BLOCK. Here, we expect 2 but we put 3.
  EXPECT_EQ(3U, settings.size());

  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[3]),
            settings.at(0).primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            settings.at(0).secondary_pattern);
  EXPECT_EQ(ContentSetting::CONTENT_SETTING_ALLOW,
            settings.at(0).setting_value);

  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[2]),
            settings.at(1).primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            settings.at(1).secondary_pattern);
  EXPECT_EQ(ContentSetting::CONTENT_SETTING_ALLOW,
            settings.at(1).setting_value);

  // Clear unpartitioned usage data, which should only affect the specific
  // origin.
  args.clear();
  args.Append("https://google.com/");
  handler()->HandleClearUnpartitionedUsage(args);

  // Validate the reduce accept language has been cleared.
  settings = host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::DURABLE_STORAGE);

  // ContentSettingsType::DURABLE_STORAGE has a default settings type for the
  // wildcard '*' set to BLOCK. Here, we expect 1 but we put 2.
  EXPECT_EQ(2U, settings.size());

  // www.google.com should be the only remaining entry.
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[3]),
            settings.at(0).primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            settings.at(0).secondary_pattern);
  EXPECT_EQ(ContentSetting::CONTENT_SETTING_ALLOW,
            settings.at(0).setting_value);

  // Clear unpartitioned usage data through HTTPS scheme, make sure https site
  // durable storage have been cleared when the specific origin HTTPS
  // scheme exist.
  args.clear();
  args.Append("http://www.google.com/");
  handler()->HandleClearUnpartitionedUsage(args);

  // Validate the durable storage has been cleared.
  settings = host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::DURABLE_STORAGE);

  // ContentSettingsType::DURABLE_STORAGE has a default settings type for the
  // wildcard '*' set to BLOCK. Therefore, when there's only one rule, it means
  // that there are none.
  EXPECT_EQ(1U, settings.size());
}

TEST_F(SiteSettingsHandlerTest, HandleClearPartitionedUsage) {
  // Confirm that removing unpartitioned storage correctly removes the
  // appropriate nodes.
  SetupModel();
  std::vector<browsing_data_model_test_util::BrowsingDataEntry>
      expected_browsing_data_model_entries = {
          kGoogleUnpartitionedEntry,
          kExampleUnpartitionedEntry,
          kGoogleOnExampleEntry,
          kExampleOnGoogleSecureEntry,
          kExampleOnGoogleInsecureEntry,
          kExampleLocalStorage,
          kHttpExampleCookie,
          kHttpsWwwExampleCookie,
          kPartitionedHttpsWwwExampleOnGoogleAuCookie,
          kPartitionedHttpsWwwExampleOnGoogleCookie,
          kHttpAbcExampleCookie,
          kHttpGoogleCookieA,
          kHttpGoogleCookieB,
          kHttpGoogleAuCookie,
          kPartitionedHttpsGoogleAu1PCookie,
          kPartitionedHttpsWwwAnotherExampleOnGoogleAuCookie,
          kUngroupedHttpCookie,
      };
  browsing_data_model_test_util::ValidateBrowsingDataEntries(
      handler()->GetBrowsingDataModelForTesting(),
      expected_browsing_data_model_entries);

  base::Value::List args;
  args.Append("https://www.example.com/");
  args.Append(GroupingKey::CreateFromEtldPlus1("google.com").Serialize());
  handler()->HandleClearPartitionedUsage(args);
  RemoveModelEntries(expected_browsing_data_model_entries,
                     {
                         kExampleOnGoogleSecureEntry,
                         kExampleOnGoogleInsecureEntry,
                         kPartitionedHttpsWwwExampleOnGoogleCookie,
                     });

  browsing_data_model_test_util::ValidateBrowsingDataEntries(
      handler()->GetBrowsingDataModelForTesting(),
      expected_browsing_data_model_entries);
}

TEST_F(SiteSettingsHandlerTest, HandleGetRwsMembershipLabel) {
  base::Value::List args;
  args.Append("getRwsMembershipLabel");
  args.Append(5);
  args.Append("google.com");
  handler()->HandleGetRwsMembershipLabel(args);
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();

  EXPECT_EQ("cr.webUIResponse", data.function_name());
  EXPECT_EQ("getRwsMembershipLabel", data.arg1()->GetString());
  ASSERT_TRUE(data.arg2()->GetBool());
  EXPECT_EQ("5 sites in google.com's group", data.arg3()->GetString());
}

TEST_F(SiteSettingsHandlerTest, HandleGetFormattedBytes) {
  const double size = 120000000000;
  base::Value::List get_args;
  get_args.Append(kCallbackId);
  get_args.Append(size);
  handler()->HandleGetFormattedBytes(get_args);

  // Validate that this method can handle large data.
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());
  EXPECT_EQ(kCallbackId, data.arg1()->GetString());
  ASSERT_TRUE(data.arg2()->GetBool());
  EXPECT_EQ(base::UTF16ToUTF8(ui::FormatBytes(int64_t(size))),
            data.arg3()->GetString());
}

TEST_F(SiteSettingsHandlerTest, HandleGetUsageInfo) {
  SetupDefaultRelatedWebsiteSets(mock_privacy_sandbox_service());

  EXPECT_CALL(*mock_privacy_sandbox_service(), IsPartOfManagedFirstPartySet(_))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      IsPartOfManagedFirstPartySet(ConvertEtldToSchemefulSite("example.com")))
      .Times(2)
      .WillRepeatedly(Return(true));

  // Confirm that usage info only returns unpartitioned storage.
  SetupModel();

  EXPECT_EQ(17,
            std::distance(handler()->GetBrowsingDataModelForTesting()->begin(),
                          handler()->GetBrowsingDataModelForTesting()->end()));

  base::Value::List args;
  args.Append("http://www.example.com");
  handler()->HandleFetchUsageTotal(args);
  handler()->ServicePendingRequests();
  ValidateUsageInfo("http://www.example.com", "302 B", "1 cookie",
                    "1 site in example.com's group", true);

  args.clear();
  args.Append("http://example.com");
  handler()->HandleFetchUsageTotal(args);
  handler()->ServicePendingRequests();
  ValidateUsageInfo("http://example.com", "", "1 cookie",
                    "1 site in example.com's group", true);

  args.clear();
  args.Append("http://google.com");
  handler()->HandleFetchUsageTotal(args);
  handler()->ServicePendingRequests();
  ValidateUsageInfo("http://google.com", "100 B", "2 cookies",
                    "2 sites in google.com's group", false);

  args.clear();
  args.Append("http://ungrouped.com");
  handler()->HandleFetchUsageTotal(args);
  handler()->ServicePendingRequests();
  ValidateUsageInfo("http://ungrouped.com", "", "1 cookie", "", false);

  // Test that the argument URL formatting is preserved in the response because
  // the UI ignores responses with different URL strings.
  args.clear();
  args.Append("http://ungrouped.com//");
  handler()->HandleFetchUsageTotal(args);
  handler()->ServicePendingRequests();
  ValidateUsageInfo("http://ungrouped.com//", "", "1 cookie", "", false);
}

TEST_F(SiteSettingsHandlerTest, RelatedWebsiteSetsMembership) {
  SetupDefaultRelatedWebsiteSets(mock_privacy_sandbox_service());

  EXPECT_CALL(*mock_privacy_sandbox_service(), IsPartOfManagedFirstPartySet(_))
      .Times(2)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      IsPartOfManagedFirstPartySet(ConvertEtldToSchemefulSite("example.com")))
      .Times(1)
      .WillOnce(Return(true));

  SetupModel();

  handler()->ClearAllSitesMapForTesting();

  handler()->OnStorageFetched();
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data.function_name());

  ASSERT_TRUE(data.arg1()->is_string());
  EXPECT_EQ("onStorageListFetched", data.arg1()->GetString());

  ASSERT_TRUE(data.arg2()->is_list());
  const base::Value::List& storage_and_cookie_list = data.arg2()->GetList();
  EXPECT_EQ(4U, storage_and_cookie_list.size());

  auto related_website_sets = GetTestRelatedWebsiteSets();

  ValidateSitesWithRws(storage_and_cookie_list, related_website_sets);
}

TEST_F(SiteSettingsHandlerTest, IsolatedWebAppUsageInfo) {
  std::string iwa_url =
      "isolated-app://"
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/";
  SetupModelWithIsolatedWebAppData({{iwa_url, 1000}});

  base::Value::List args;
  args.Append(iwa_url);
  handler()->HandleFetchUsageTotal(args);
  handler()->ServicePendingRequests();

  ValidateUsageInfo(
      /*expected_usage_host=*/iwa_url, /*expected_usage_string=*/"1,000 B",
      /*expected_cookie_string=*/"",
      /*expected_rws_member_count_string=*/"", /*expected_rws_policy=*/false);
}

TEST_F(SiteSettingsHandlerTest, IsolatedWebAppClearSiteGroupDataAndCookies) {
  GURL iwa_url1(
      "isolated-app://"
      "abcdefztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/");
  GURL iwa_url2(
      "isolated-app://"
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/");
  SetupModelWithIsolatedWebAppData(
      {{iwa_url1.spec(), 1000}, {iwa_url2.spec(), 2000}});

  auto verify_site_group = [](const base::Value& site_group,
                              const GURL& expected_origin,
                              int64_t expected_usage) {
    ASSERT_TRUE(site_group.is_dict());
    EXPECT_THAT(CHECK_DEREF(site_group.GetDict().FindString("groupingKey")),
                IsOrigin(expected_origin));
    ASSERT_EQ(1U, site_group.GetDict().FindList("origins")->size());
    const base::Value::Dict& origin_info =
        site_group.GetDict().FindList("origins")->front().GetDict();
    EXPECT_EQ(expected_usage, origin_info.FindDouble("usage").value());
  };

  base::Value::List all_sites_list = GetOnStorageFetchedSentList();
  EXPECT_EQ(2U, all_sites_list.size());
  verify_site_group(all_sites_list[0], iwa_url1, 1000);
  verify_site_group(all_sites_list[1], iwa_url2, 2000);

  base::Value::List args;
  args.Append(GroupingKey::Create(url::Origin::Create(iwa_url1)).Serialize());
  handler()->HandleClearSiteGroupDataAndCookies(args);

  all_sites_list = GetOnStorageFetchedSentList();
  EXPECT_EQ(1U, all_sites_list.size());
  verify_site_group(all_sites_list[0], iwa_url2, 2000);
}

TEST_F(SiteSettingsHandlerTest, IsolatedWebAppClearUnpartitionedUsage) {
  GURL iwa_url(
      "isolated-app://"
      "abcdefztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/");
  SetupModelWithIsolatedWebAppData({{iwa_url.spec(), 1000}});

  base::Value::List usage_args;
  usage_args.Append(iwa_url.spec());
  handler()->HandleFetchUsageTotal(usage_args);
  handler()->ServicePendingRequests();

  ValidateUsageInfo(
      /*expected_usage_origin=*/iwa_url.spec(),
      /*expected_usage_string=*/"1,000 B",
      /*expected_cookie_string=*/"",
      /*expected_rws_member_count_string=*/"", /*expected_rws_policy=*/false);

  base::Value::List clear_args;
  clear_args.Append(iwa_url.spec());
  handler()->HandleClearUnpartitionedUsage(clear_args);

  handler()->HandleFetchUsageTotal(usage_args);
  handler()->ServicePendingRequests();

  ValidateUsageInfo(
      /*expected_usage_origin=*/iwa_url.spec(),
      /*expected_usage_string=*/"",
      /*expected_cookie_string=*/"",
      /*expected_rws_member_count_string=*/"", /*expected_rws_policy=*/false);
}

#if BUILDFLAG(IS_CHROMEOS)
class SiteSettingsGlobalPermissionTest
    : public SiteSettingsHandlerBaseTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 protected:
  bool CamBlocked() const { return std::get<0>(GetParam()); }
  bool MicBlocked() const { return std::get<1>(GetParam()); }
  bool GeoBlocked() const { return std::get<2>(GetParam()); }
};

TEST_P(SiteSettingsGlobalPermissionTest, GetSystemDeniedPermissions) {
  system_permission_settings::ScopedSettingsForTesting cam_settings(
      ContentSettingsType::MEDIASTREAM_CAMERA, CamBlocked());
  system_permission_settings::ScopedSettingsForTesting mic_settings(
      ContentSettingsType::MEDIASTREAM_MIC, MicBlocked());
  system_permission_settings::ScopedSettingsForTesting geo_settings(
      ContentSettingsType::GEOLOCATION, GeoBlocked());

  base::Value::List args;
  args.Append(kCallbackId);
  handler()->HandleGetSystemDeniedPermissions(args);
  EXPECT_LT(0u, CHECK_DEREF(web_ui()).call_data().size());
  const auto& call_data = *(CHECK_DEREF(web_ui()).call_data().back());
  EXPECT_EQ(3u, call_data.args().size());
  EXPECT_EQ(base::Value(kCallbackId), CHECK_DEREF(call_data.arg1()));
  EXPECT_EQ(base::Value(true), CHECK_DEREF(call_data.arg2()));

  base::Value::List expected_result;
  if (CamBlocked()) {
    expected_result.Append("media-stream-camera");
  }
  if (MicBlocked()) {
    expected_result.Append("media-stream-mic");
  }
  if (GeoBlocked()) {
    expected_result.Append("location");
  }

  EXPECT_EQ(expected_result, CHECK_DEREF(call_data.arg3()));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SiteSettingsGlobalPermissionTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

class SiteSettingsOpenSystemSettingsTest
    : public SiteSettingsHandlerBaseTest,
      public testing::WithParamInterface<ContentSettingsType> {
 public:
  SiteSettingsOpenSystemSettingsTest() {
    system_permission_settings::SetInstanceForTesting(&mock_platform_handle);
  }
  ~SiteSettingsOpenSystemSettingsTest() {
    system_permission_settings::SetInstanceForTesting(nullptr);
  }

  ContentSettingsType PermissionType() const { return GetParam(); }

  NiceMock<system_permission_settings::MockPlatformHandle> mock_platform_handle;
};

TEST_P(SiteSettingsOpenSystemSettingsTest, OpenSystemSettings) {
  base::Value permission_type(
      site_settings::ContentSettingsTypeToGroupName(PermissionType()));
  auto args = base::Value::List().Append(std::move(permission_type));
  EXPECT_CALL(mock_platform_handle, OpenSystemSettings(_, PermissionType()));
  handler()->HandleOpenSystemPermissionSettings(args);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SiteSettingsOpenSystemSettingsTest,
    testing::Values(ContentSettingsType::MEDIASTREAM_CAMERA,
                    ContentSettingsType::MEDIASTREAM_MIC,
                    ContentSettingsType::GEOLOCATION));
#endif

}  // namespace settings
