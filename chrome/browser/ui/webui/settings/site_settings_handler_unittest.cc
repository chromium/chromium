// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/site_settings_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/permissions/notification_permission_review_service_factory.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/privacy_sandbox/mock_privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browsing_data/content/browsing_data_model.h"
#include "components/browsing_data/content/fake_browsing_data_model.h"
#include "components/browsing_data/content/mock_cookie_helper.h"
#include "components/browsing_data/content/mock_local_storage_helper.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/browsing_topics/test_util.h"
#include "components/client_hints/common/client_hints.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/history/core/browser/history_service.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/permissions/object_permission_context_base.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/test/object_permission_context_base_mock_permission_observer.h"
#include "components/permissions/test/permission_test_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/webui/webui_allowlist.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/mock_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#endif

using ::testing::_;
using ::testing::Return;

namespace {

constexpr char kCallbackId[] = "test-callback-id";
constexpr char kSetting[] = "setting";
constexpr char kSource[] = "source";
constexpr char kExtensionName[] = "Test Extension";

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
    {{";", "cookies"}, {false, "Not a valid web address"}},
    {{"*", "cookies"}, {false, "Not a valid web address"}},
    {{"chrome://test", "popups"}, {false, "Not a valid web address"}},
    {{"chrome-untrusted://test", "popups"}, {false, "Not a valid web address"}},
    {{"devtools://devtools", "popups"}, {false, "Not a valid web address"}},
    {{"chrome-search://search", "popups"}, {false, "Not a valid web address"}},
    {{"http://google.com", "location"}, {false, "Origin must be secure"}},
    {{"http://127.0.0.1", "location"}, {true, ""}},  // Localhost is secure.
    {{"http://[::1]", "location"}, {true, ""}}};

// Converts |etld_plus1| into an HTTPS SchemefulSite.
net::SchemefulSite ConvertEtldToSchemefulSite(const std::string etld_plus1) {
  return net::SchemefulSite(GURL(std::string(url::kHttpsScheme) +
                                 url::kStandardSchemeSeparator + etld_plus1 +
                                 "/"));
}

// Validates that the list of sites are aligned with the first party sets
// mapping.
void ValidateSitesWithFps(
    const base::Value::List& storage_and_cookie_list,
    base::flat_map<net::SchemefulSite, net::SchemefulSite>& first_party_sets) {
  for (const base::Value& site_group : storage_and_cookie_list) {
    std::string etld_plus1 = *site_group.GetDict().FindString("etldPlus1");
    auto schemeful_site = ConvertEtldToSchemefulSite(etld_plus1);

    if (first_party_sets.count(schemeful_site)) {
      // Ensure that the `fpsOwner` is set correctly and aligned with
      // |first_party_sets| mapping of site group owners.
      std::string owner_etldplus1 =
          first_party_sets[schemeful_site].GetURL().host();
      ASSERT_EQ(owner_etldplus1, *site_group.GetDict().FindString("fpsOwner"));
      if (owner_etldplus1 == "google.com") {
        ASSERT_EQ(2, *site_group.GetDict().FindInt("fpsNumMembers"));
        ASSERT_EQ(false,
                  *site_group.GetDict().FindBool("fpsEnterpriseManaged"));
      } else if (owner_etldplus1 == "example.com") {
        ASSERT_EQ(1, *site_group.GetDict().FindInt("fpsNumMembers"));
        ASSERT_EQ(true, *site_group.GetDict().FindBool("fpsEnterpriseManaged"));
      }
    } else {
      // The site is not part of a FPS therefore doesn't have `fpsOwner` or
      // `fpsNumMembers` set. `FindString` and `FindInt` should return null.
      ASSERT_FALSE(site_group.GetDict().FindString("fpsOwner"));
      ASSERT_FALSE(site_group.GetDict().FindInt("fpsNumMembers"));
      ASSERT_FALSE(site_group.GetDict().FindBool("fpsEnterpriseManaged"));
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

void InstallWebApp(Profile* profile, const GURL& start_url) {
  apps::AppRegistryCache& cache =
      apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache();
  std::vector<apps::AppPtr> deltas;
  deltas.push_back(
      MakeApp(web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, start_url),
              apps::AppType::kWeb, start_url.spec(), apps::Readiness::kReady,
              apps::InstallReason::kSync));
  cache.OnApps(std::move(deltas), apps::AppType::kWeb,
               /*should_notify_initialized=*/true);
}

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
        NOTREACHED();
        return "";
    }
  }

 private:
  raw_ptr<sync_preferences::TestingPrefServiceSyncable> prefs_;
  ContentSettingsType content_type_;
};

class SiteSettingsHandlerTest : public testing::Test,
                                public testing::WithParamInterface<bool> {
 public:
  SiteSettingsHandlerTest()
      : kNotifications(site_settings::ContentSettingsTypeToGroupName(
            ContentSettingsType::NOTIFICATIONS)),
        kCookies(site_settings::ContentSettingsTypeToGroupName(
            ContentSettingsType::COOKIES)) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<ash::MockUserManager>());
#endif

    // Fully initialize |profile_| in the constructor since some children
    // classes need it right away for SetUp().
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();
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

  void RecordNotification(permissions::NotificationsEngagementService* service,
                          GURL url,
                          int daily_avarage_count) {
    // This many notifications were recorded during the past week in total.
    int total_count = daily_avarage_count * 7;
    service->RecordNotificationDisplayed(url, total_count);
  }

  base::Time GetReferenceTime() {
    base::Time time;
    EXPECT_TRUE(base::Time::FromString("Sat, 1 Sep 2018 11:00:00 GMT", &time));
    return time;
  }

  TestingProfile* profile() { return profile_.get(); }
  TestingProfile* incognito_profile() { return incognito_profile_; }
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

    absl::optional<bool> enabled = event_data->FindBoolKey("enabled");
    ASSERT_TRUE(enabled.has_value());
    EXPECT_EQ(expected_enabled, *enabled);

    const base::Value* pref_data = event_data->FindDictPath("pref");
    ASSERT_TRUE(pref_data && pref_data->is_dict());

    absl::optional<bool> value = pref_data->FindBoolKey("value");
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
                       const site_settings::SiteSettingSource expected_source,
                       size_t expected_total_calls) {
    EXPECT_EQ(expected_total_calls, web_ui()->call_data().size());

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());

    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());

    ASSERT_TRUE(data.arg2()->is_bool());
    ASSERT_TRUE(data.arg2()->GetBool());

    const base::Value* default_value = data.arg3();
    ASSERT_TRUE(default_value->is_dict());
    const std::string* setting = default_value->FindStringKey(kSetting);
    ASSERT_TRUE(setting);
    EXPECT_EQ(content_settings::ContentSettingToString(expected_setting),
              *setting);
    const std::string* source = default_value->FindStringKey(kSource);
    if (source) {
      EXPECT_EQ(site_settings::SiteSettingSourceToString(expected_source),
                *source);
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

    const base::Value& exception = data.arg3()->GetList()[0];
    ASSERT_TRUE(exception.is_dict());

    const std::string* origin = exception.FindStringKey(site_settings::kOrigin);
    ASSERT_TRUE(origin);
    ASSERT_EQ(expected_origin, *origin);

    const std::string* display_name =
        exception.FindStringKey(site_settings::kDisplayName);
    ASSERT_TRUE(display_name);
    ASSERT_EQ(expected_display_name, *display_name);

    const std::string* embedding_origin =
        exception.FindStringKey(site_settings::kEmbeddingOrigin);
    ASSERT_TRUE(embedding_origin);
    ASSERT_EQ(expected_embedding, *embedding_origin);

    const std::string* setting =
        exception.FindStringKey(site_settings::kSetting);
    ASSERT_TRUE(setting);
    ASSERT_EQ(content_settings::ContentSettingToString(expected_setting),
              *setting);

    const std::string* source = exception.FindStringKey(site_settings::kSource);
    ASSERT_TRUE(source);
    ASSERT_EQ(site_settings::SiteSettingSourceToString(expected_source),
              *source);
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

    absl::optional<bool> valid = result->FindBoolKey("isValid");
    ASSERT_TRUE(valid.has_value());
    EXPECT_EQ(expected_validity, *valid);

    const std::string* reason = result->FindStringKey("reason");
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

  void ValidateZoom(const std::string& expected_host,
                    const std::string& expected_zoom,
                    size_t expected_total_calls) {
    EXPECT_EQ(expected_total_calls, web_ui()->call_data().size());

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());

    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ("onZoomLevelsChanged", data.arg1()->GetString());

    ASSERT_TRUE(data.arg2()->is_list());
    const base::Value::List& exceptions = data.arg2()->GetList();
    if (expected_host.empty()) {
      EXPECT_EQ(0U, exceptions.size());
    } else {
      EXPECT_EQ(1U, exceptions.size());

      const base::Value& exception = exceptions[0];
      ASSERT_TRUE(exception.is_dict());

      const std::string* host = exception.FindStringKey("origin");
      ASSERT_TRUE(host);
      ASSERT_EQ(expected_host, *host);

      const std::string* zoom = exception.FindStringKey("zoom");
      ASSERT_TRUE(zoom);
      ASSERT_EQ(expected_zoom, *zoom);
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

  void ValidateUsageInfo(const std::string& expected_usage_host,
                         const std::string& expected_usage_string,
                         const std::string& expected_cookie_string,
                         const std::string& expected_fps_member_count_string,
                         const bool expected_fps_policy) {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());

    ASSERT_TRUE(data.arg_nth(0)->is_string());
    EXPECT_EQ("usage-total-changed", data.arg_nth(0)->GetString());

    ASSERT_TRUE(data.arg_nth(1)->is_string());
    EXPECT_EQ(expected_usage_host, data.arg_nth(1)->GetString());

    ASSERT_TRUE(data.arg_nth(2)->is_string());
    EXPECT_EQ(expected_usage_string, data.arg_nth(2)->GetString());

    ASSERT_TRUE(data.arg_nth(3)->is_string());
    EXPECT_EQ(expected_cookie_string, data.arg_nth(3)->GetString());

    ASSERT_TRUE(data.arg_nth(4)->is_string());
    EXPECT_EQ(expected_fps_member_count_string, data.arg_nth(4)->GetString());

    ASSERT_TRUE(data.arg_nth(5)->is_bool());
    EXPECT_EQ(expected_fps_policy, data.arg_nth(5)->GetBool());
  }

  void ValidateNotificationPermissionUpdate() {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());

    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ("notification-permission-review-list-maybe-changed",
              data.arg1()->GetString());

    ASSERT_TRUE(data.arg2()->is_list());
  }

  void CreateIncognitoProfile() {
    incognito_profile_ = TestingProfile::Builder().BuildIncognito(profile());
  }

  virtual void DestroyIncognitoProfile() {
    if (incognito_profile_) {
      profile_->DestroyOffTheRecordProfile(incognito_profile_);
      incognito_profile_ = nullptr;
    }
  }

  // TODO(https://crbug.com/835712): Currently only set up the cookies and local
  // storage nodes, will update all other nodes in the future.
  void SetupModels() {
    scoped_refptr<browsing_data::MockCookieHelper>
        mock_browsing_data_cookie_helper;
    scoped_refptr<browsing_data::MockLocalStorageHelper>
        mock_browsing_data_local_storage_helper;

    mock_browsing_data_cookie_helper =
        new browsing_data::MockCookieHelper(profile());
    mock_browsing_data_local_storage_helper =
        new browsing_data::MockLocalStorageHelper(profile());

    auto container = std::make_unique<LocalDataContainer>(
        mock_browsing_data_cookie_helper,
        /*database_helper=*/nullptr, mock_browsing_data_local_storage_helper,
        /*session_storage_helper=*/nullptr,
        /*indexed_db_helper=*/nullptr,
        /*file_system_helper=*/nullptr,
        /*quota_helper=*/nullptr,
        /*service_worker_helper=*/nullptr,
        /*data_shared_worker_helper=*/nullptr,
        /*cache_storage_helper=*/nullptr);
    auto mock_cookies_tree_model = std::make_unique<CookiesTreeModel>(
        std::move(container), profile()->GetExtensionSpecialStoragePolicy());

    mock_browsing_data_local_storage_helper->AddLocalStorageForStorageKey(
        blink::StorageKey::CreateFromStringForTesting(
            "https://www.example.com/"),
        2);

    mock_browsing_data_local_storage_helper->Notify();

    mock_browsing_data_cookie_helper->AddCookieSamples(
        GURL("http://example.com"), "A=1");
    mock_browsing_data_cookie_helper->AddCookieSamples(
        GURL("https://www.example.com/"), "B=1");
    mock_browsing_data_cookie_helper->AddCookieSamples(
        GURL("http://abc.example.com"), "C=1");
    mock_browsing_data_cookie_helper->AddCookieSamples(
        GURL("http://google.com"), "A=1");
    mock_browsing_data_cookie_helper->AddCookieSamples(
        GURL("http://google.com"), "B=1");
    mock_browsing_data_cookie_helper->AddCookieSamples(
        GURL("http://google.com.au"), "A=1");

    mock_browsing_data_cookie_helper->AddCookieSamples(
        GURL("https://www.example.com"),
        "__Host-A=1; Path=/; Partitioned; Secure;",
        net::CookiePartitionKey::FromURLForTesting(
            GURL("https://google.com.au")));
    mock_browsing_data_cookie_helper->AddCookieSamples(
        GURL("https://google.com.au"),
        "__Host-A=1; Path=/; Partitioned; Secure;",
        net::CookiePartitionKey::FromURLForTesting(
            GURL("https://google.com.au")));
    mock_browsing_data_cookie_helper->AddCookieSamples(
        GURL("https://www.another-example.com"),
        "__Host-A=1; Path=/; Partitioned; Secure;",
        net::CookiePartitionKey::FromURLForTesting(
            GURL("https://google.com.au")));
    mock_browsing_data_cookie_helper->AddCookieSamples(
        GURL("https://www.example.com"),
        "__Host-A=1; Path=/; Partitioned; Secure;",
        net::CookiePartitionKey::FromURLForTesting(GURL("https://google.com")));

    // Add an entry which will not be grouped with any other entries. This
    // will require a placeholder origin to be correctly added & removed.
    mock_browsing_data_cookie_helper->AddCookieSamples(
        GURL("http://ungrouped.com"), "A=1");

    mock_browsing_data_cookie_helper->Notify();

    auto fake_browsing_data_model = std::make_unique<FakeBrowsingDataModel>();
    fake_browsing_data_model->AddBrowsingData(
        url::Origin::Create(GURL("https://www.google.com")),
        BrowsingDataModel::StorageType::kTrustTokens, 50000000000);

    handler()->SetModelsForTesting(std::move(mock_cookies_tree_model),
                                   std::move(fake_browsing_data_model));
  }

  base::Value::List GetOnStorageFetchedSentList() {
    handler()->ClearAllSitesMapForTesting();

    base::Value::List get_all_sites_args;
    get_all_sites_args.Append(kCallbackId);
    handler()->HandleGetAllSites(get_all_sites_args);
    handler()->ServicePendingRequests();

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    return data.arg2()->GetList().Clone();
  }

  std::vector<CookieTreeNode*> GetHostNodes(GURL url) {
    std::vector<CookieTreeNode*> nodes;
    for (const auto& host_node :
         handler()->cookies_tree_model_->GetRoot()->children()) {
      if (host_node->GetDetailedInfo().origin.GetURL() == url) {
        nodes.push_back(host_node.get());
      }
    }
    return nodes;
  }

  void SetupDefaultFirstPartySets(MockPrivacySandboxService* mock_service) {
    EXPECT_CALL(*mock_service, GetFirstPartySetOwner(_))
        .WillRepeatedly(
            [&](const GURL& url) -> absl::optional<net::SchemefulSite> {
              auto first_party_sets = GetTestFirstPartySets();
              if (first_party_sets.count(net::SchemefulSite(url))) {
                return first_party_sets[net::SchemefulSite(url)];
              }

              return absl::nullopt;
            });
  }

  base::flat_map<net::SchemefulSite, net::SchemefulSite>
  GetTestFirstPartySets() {
    base::flat_map<net::SchemefulSite, net::SchemefulSite> first_party_sets = {
        {ConvertEtldToSchemefulSite("google.com"),
         ConvertEtldToSchemefulSite("google.com")},
        {ConvertEtldToSchemefulSite("google.com.au"),
         ConvertEtldToSchemefulSite("google.com")},
        {ConvertEtldToSchemefulSite("example.com"),
         ConvertEtldToSchemefulSite("example.com")},
        {ConvertEtldToSchemefulSite("unrelated.com"),
         ConvertEtldToSchemefulSite("unrelated.com")}};

    return first_party_sets;
  }

  base::Value::List GetOriginList(int size) {
    base::Value::List origins;
    for (int i = 0; i < size; i++) {
      origins.Append("https://example" + base::NumberToString(i) + ".org:443");
    }
    return origins;
  }

  // Content setting group name for the relevant ContentSettingsType.
  const std::string kNotifications;
  const std::string kCookies;

  const ContentSettingsType kPermissionNotifications =
      ContentSettingsType::NOTIFICATIONS;

  // The number of listeners that are expected to fire when any content setting
  // is changed.
  const size_t kNumberContentSettingListeners = 2;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<TestingProfile> incognito_profile_;
  content::TestWebUI web_ui_;
  std::unique_ptr<SiteSettingsHandler> handler_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
#endif
  raw_ptr<browsing_topics::MockBrowsingTopicsService>
      mock_browsing_topics_service_;
  raw_ptr<MockPrivacySandboxService> mock_privacy_sandbox_service_;
};

// True if testing for handle clear unpartitioned usage with HTTPS scheme URL.
// When set to true, the tests use HTTPS scheme as origin. When set to
// false, the tests use HTTP scheme as origin.
INSTANTIATE_TEST_SUITE_P(All, SiteSettingsHandlerTest, testing::Bool());

TEST_F(SiteSettingsHandlerTest, GetAndSetDefault) {
  // Test the JS -> C++ -> JS callback path for getting and setting defaults.
  base::Value::List get_args;
  get_args.Append(kCallbackId);
  get_args.Append(kNotifications);
  handler()->HandleGetDefaultValueForContentType(get_args);
  ValidateDefault(CONTENT_SETTING_ASK,
                  site_settings::SiteSettingSource::kDefault, 1U);

  // Set the default to 'Blocked'.
  base::Value::List set_args;
  set_args.Append(kNotifications);
  set_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  handler()->HandleSetDefaultValueForContentType(set_args);

  EXPECT_EQ(2U, web_ui()->call_data().size());

  // Verify that the default has been set to 'Blocked'.
  handler()->HandleGetDefaultValueForContentType(get_args);
  ValidateDefault(CONTENT_SETTING_BLOCK,
                  site_settings::SiteSettingSource::kDefault, 3U);
}

// Flaky on CrOS and Linux. https://crbug.com/930481
TEST_F(SiteSettingsHandlerTest, GetAllSites) {
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
    for (const base::Value& site_group : site_groups) {
      const std::string& etld_plus1_string =
          site_group.FindKey("etldPlus1")->GetString();
      const base::Value::List& origin_list =
          site_group.FindKey("origins")->GetList();
      EXPECT_EQ("example.com", etld_plus1_string);
      EXPECT_EQ(2UL, origin_list.size());
      EXPECT_EQ(url1.spec(), origin_list[0].FindKey("origin")->GetString());
      EXPECT_EQ(0, origin_list[0].FindKey("engagement")->GetDouble());
      EXPECT_EQ(url2.spec(), origin_list[1].FindKey("origin")->GetString());
      EXPECT_EQ(0, origin_list[1].FindKey("engagement")->GetDouble());
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
    for (const base::Value& site_group : site_groups) {
      const std::string& etld_plus1_string =
          site_group.FindKey("etldPlus1")->GetString();
      const base::Value::List& origin_list =
          site_group.FindKey("origins")->GetList();
      if (etld_plus1_string == "example2.net") {
        EXPECT_EQ(1UL, origin_list.size());
        EXPECT_EQ(url3.spec(), origin_list[0].FindKey("origin")->GetString());
      } else {
        EXPECT_EQ("example.com", etld_plus1_string);
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
      CONTENT_SETTING_BLOCK,
      auto_blocker->GetEmbargoResult(url4, ContentSettingsType::NOTIFICATIONS)
          ->content_setting);
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
    EXPECT_EQ("example.com", site_groups[0].FindKey("etldPlus1")->GetString());
    EXPECT_EQ("example2.net", site_groups[1].FindKey("etldPlus1")->GetString());
  }

  // Add an expired embargo setting to an existing eTLD+1 group and make sure it
  // still appears.
  for (int i = 0; i < 3; ++i) {
    auto_blocker->RecordDismissAndEmbargo(
        url3, ContentSettingsType::NOTIFICATIONS, false);
  }
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      auto_blocker->GetEmbargoResult(url3, ContentSettingsType::NOTIFICATIONS)
          ->content_setting);
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
    EXPECT_EQ("example.com", site_groups[0].FindKey("etldPlus1")->GetString());
    EXPECT_EQ("example2.net", site_groups[1].FindKey("etldPlus1")->GetString());
  }

  // Add an expired embargo to a new eTLD+1 and make sure it doesn't appear.
  const GURL url5("http://test.example5.com");
  for (int i = 0; i < 3; ++i) {
    auto_blocker->RecordDismissAndEmbargo(
        url5, ContentSettingsType::NOTIFICATIONS, false);
  }
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      auto_blocker->GetEmbargoResult(url5, ContentSettingsType::NOTIFICATIONS)
          ->content_setting);
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
    EXPECT_EQ("example.com", site_groups[0].FindKey("etldPlus1")->GetString());
    EXPECT_EQ("example2.net", site_groups[1].FindKey("etldPlus1")->GetString());
  }

  // Each call to HandleGetAllSites() above added a callback to the profile's
  // browsing_data::LocalStorageHelper, so make sure these aren't stuck waiting
  // to run at the end of the test.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

TEST_F(SiteSettingsHandlerTest, GetRecentSitePermissions) {
  // Constants used only in this test.
  std::string kAllowed = content_settings::ContentSettingToString(
      ContentSetting::CONTENT_SETTING_ALLOW);
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
    EXPECT_EQ(url1.spec(),
              recent_permissions[1].FindKey("origin")->GetString());
    EXPECT_EQ(url1.spec(),
              recent_permissions[0].FindKey("origin")->GetString());

    EXPECT_TRUE(recent_permissions[0].FindKey("incognito")->GetBool());
    EXPECT_FALSE(recent_permissions[1].FindKey("incognito")->GetBool());

    const base::Value::List& incognito_url1_permissions =
        recent_permissions[0].FindKey("recentPermissions")->GetList();
    const base::Value::List& url1_permissions =
        recent_permissions[1].FindKey("recentPermissions")->GetList();

    EXPECT_EQ(1UL, incognito_url1_permissions.size());

    EXPECT_EQ(kNotifications,
              incognito_url1_permissions[0].FindKey("type")->GetString());
    EXPECT_EQ(kBlocked,
              incognito_url1_permissions[0].FindKey("setting")->GetString());
    EXPECT_EQ(kEmbargo,
              incognito_url1_permissions[0].FindKey("source")->GetString());

    EXPECT_EQ(kNotifications, url1_permissions[0].FindKey("type")->GetString());
    EXPECT_EQ(kBlocked, url1_permissions[0].FindKey("setting")->GetString());
    EXPECT_EQ(kEmbargo, url1_permissions[0].FindKey("source")->GetString());
  }
}

TEST_F(SiteSettingsHandlerTest, OnStorageFetched) {
  SetupModels();

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
    const base::Value& site_group = storage_and_cookie_list[0];
    ASSERT_TRUE(site_group.is_dict());

    ASSERT_TRUE(site_group.FindStringKey("etldPlus1"));
    ASSERT_EQ("example.com", *site_group.FindStringKey("etldPlus1"));

    EXPECT_EQ(3, site_group.FindKey("numCookies")->GetDouble());

    const base::Value* origin_list = site_group.FindListKey("origins");
    ASSERT_TRUE(origin_list && origin_list->is_list());
    // There will be 2 origins in this case. Cookie node with url
    // http://www.example.com/ will be treat as https://www.example.com/ because
    // this url existed in the storage nodes.
    EXPECT_EQ(2U, origin_list->GetList().size());

    const base::Value& origin_info_0 = origin_list->GetList()[0];
    ASSERT_TRUE(origin_info_0.is_dict());

    EXPECT_EQ("http://abc.example.com/",
              origin_info_0.FindKey("origin")->GetString());
    EXPECT_EQ(0, origin_info_0.FindKey("engagement")->GetDouble());
    EXPECT_EQ(0, origin_info_0.FindKey("usage")->GetDouble());
    EXPECT_EQ(1, origin_info_0.FindKey("numCookies")->GetDouble());

    const base::Value& origin_info_1 = origin_list->GetList()[1];
    ASSERT_TRUE(origin_info_1.is_dict());

    // Even though in the cookies the scheme is http, it still stored as https
    // because there is https data stored.
    EXPECT_EQ("https://www.example.com/",
              origin_info_1.FindKey("origin")->GetString());
    EXPECT_EQ(0, origin_info_1.FindKey("engagement")->GetDouble());
    EXPECT_EQ(2, origin_info_1.FindKey("usage")->GetDouble());
    EXPECT_EQ(1, origin_info_1.FindKey("numCookies")->GetDouble());
  }

  {
    const base::Value& site_group = storage_and_cookie_list[1];
    ASSERT_TRUE(site_group.is_dict());

    ASSERT_TRUE(site_group.FindStringKey("etldPlus1"));
    ASSERT_EQ("google.com", *site_group.FindStringKey("etldPlus1"));

    EXPECT_EQ(3, site_group.FindKey("numCookies")->GetDouble());

    const base::Value* origin_list = site_group.FindListKey("origins");
    ASSERT_TRUE(origin_list && origin_list->is_list());

    EXPECT_EQ(2U, origin_list->GetList().size());

    const base::Value& partitioned_origin_info = origin_list->GetList()[0];
    ASSERT_TRUE(partitioned_origin_info.is_dict());

    EXPECT_EQ("https://www.example.com/",
              partitioned_origin_info.FindKey("origin")->GetString());
    EXPECT_EQ(0, partitioned_origin_info.FindKey("engagement")->GetDouble());
    EXPECT_EQ(0, partitioned_origin_info.FindKey("usage")->GetDouble());
    EXPECT_EQ(1, partitioned_origin_info.FindKey("numCookies")->GetDouble());
    EXPECT_TRUE(partitioned_origin_info.FindKey("isPartitioned")->GetBool());

    const base::Value& unpartitioned_origin_info = origin_list->GetList()[1];
    ASSERT_TRUE(unpartitioned_origin_info.is_dict());

    EXPECT_EQ("https://www.google.com/",
              unpartitioned_origin_info.FindKey("origin")->GetString());
    EXPECT_EQ(0, unpartitioned_origin_info.FindKey("engagement")->GetDouble());
    EXPECT_EQ(50000000000,
              unpartitioned_origin_info.FindKey("usage")->GetDouble());
    EXPECT_EQ(0, unpartitioned_origin_info.FindKey("numCookies")->GetDouble());
    EXPECT_FALSE(unpartitioned_origin_info.FindKey("isPartitioned")->GetBool());
  }

  {
    const base::Value& site_group = storage_and_cookie_list[2];
    ASSERT_TRUE(site_group.is_dict());

    ASSERT_TRUE(site_group.FindStringKey("etldPlus1"));
    ASSERT_EQ("google.com.au", *site_group.FindStringKey("etldPlus1"));

    EXPECT_EQ(4, site_group.FindKey("numCookies")->GetDouble());

    const base::Value* origin_list = site_group.FindListKey("origins");
    ASSERT_TRUE(origin_list && origin_list->is_list());

    // The unpartitioned cookie set for google.com.au should be associated with
    // the eTLD+1, and thus won't have an origin entry as other origin entries
    // exist for the unpartitioned storage. The partitioned cookie for
    // google.com.au, partitioned by google.com.au should have also created an
    // entry.
    EXPECT_EQ(3U, origin_list->GetList().size());

    const base::Value& partitioned_origin_one_info = origin_list->GetList()[0];
    ASSERT_TRUE(partitioned_origin_one_info.is_dict());

    EXPECT_EQ("https://google.com.au/",
              partitioned_origin_one_info.FindKey("origin")->GetString());
    EXPECT_EQ(0,
              partitioned_origin_one_info.FindKey("engagement")->GetDouble());
    EXPECT_EQ(0, partitioned_origin_one_info.FindKey("usage")->GetDouble());
    EXPECT_EQ(1,
              partitioned_origin_one_info.FindKey("numCookies")->GetDouble());
    EXPECT_TRUE(
        partitioned_origin_one_info.FindKey("isPartitioned")->GetBool());

    const base::Value& partitioned_origin_two_info = origin_list->GetList()[1];
    EXPECT_EQ("https://www.another-example.com/",
              partitioned_origin_two_info.FindKey("origin")->GetString());
    EXPECT_EQ(0,
              partitioned_origin_two_info.FindKey("engagement")->GetDouble());
    EXPECT_EQ(0, partitioned_origin_two_info.FindKey("usage")->GetDouble());
    EXPECT_EQ(1,
              partitioned_origin_two_info.FindKey("numCookies")->GetDouble());
    EXPECT_TRUE(
        partitioned_origin_two_info.FindKey("isPartitioned")->GetBool());

    const base::Value& partitioned_origin_three_info =
        origin_list->GetList()[2];
    ASSERT_TRUE(partitioned_origin_three_info.is_dict());

    EXPECT_EQ("https://www.example.com/",
              partitioned_origin_three_info.FindKey("origin")->GetString());
    EXPECT_EQ(0,
              partitioned_origin_three_info.FindKey("engagement")->GetDouble());
    EXPECT_EQ(0, partitioned_origin_three_info.FindKey("usage")->GetDouble());
    EXPECT_EQ(1,
              partitioned_origin_three_info.FindKey("numCookies")->GetDouble());
    EXPECT_TRUE(
        partitioned_origin_three_info.FindKey("isPartitioned")->GetBool());
  }

  {
    const base::Value& site_group = storage_and_cookie_list[3];
    ASSERT_TRUE(site_group.is_dict());

    ASSERT_TRUE(site_group.FindStringKey("etldPlus1"));
    ASSERT_EQ("ungrouped.com", *site_group.FindStringKey("etldPlus1"));

    EXPECT_EQ(1, site_group.FindKey("numCookies")->GetDouble());

    const base::Value* origin_list = site_group.FindListKey("origins");
    ASSERT_TRUE(origin_list && origin_list->is_list());
    EXPECT_EQ(1U, origin_list->GetList().size());

    const base::Value& origin_info = origin_list->GetList()[0];
    ASSERT_TRUE(origin_info.is_dict());

    EXPECT_EQ("http://ungrouped.com/",
              origin_info.FindKey("origin")->GetString());
    EXPECT_EQ(0, origin_info.FindKey("engagement")->GetDouble());
    EXPECT_EQ(0, origin_info.FindKey("usage")->GetDouble());
    EXPECT_EQ(1, origin_info.FindKey("numCookies")->GetDouble());
  }
}

TEST_F(SiteSettingsHandlerTest, InstalledApps) {
  InstallWebApp(profile(), GURL("http://abc.example.com/path"));

  SetupModels();

  base::Value::List storage_and_cookie_list = GetOnStorageFetchedSentList();
  EXPECT_EQ(4U, storage_and_cookie_list.size());

  {
    const base::Value& site_group = storage_and_cookie_list[0];
    ASSERT_TRUE(site_group.is_dict());

    ASSERT_TRUE(site_group.FindStringKey("etldPlus1"));
    ASSERT_EQ("example.com", *site_group.FindStringKey("etldPlus1"));

    ASSERT_TRUE(site_group.FindKey("hasInstalledPWA")->GetBool());

    const base::Value* origin_list = site_group.FindListKey("origins");
    ASSERT_TRUE(origin_list);

    const base::Value& origin_info = origin_list->GetList()[0];
    ASSERT_TRUE(origin_info.is_dict());

    EXPECT_EQ("http://abc.example.com/",
              origin_info.FindKey("origin")->GetString());
    EXPECT_TRUE(origin_info.FindKey("isInstalled")->GetBool());
  }

  // Verify that installed booleans are false for other siteGroups/origins
  {
    const base::Value& site_group = storage_and_cookie_list[1];
    ASSERT_TRUE(site_group.is_dict());

    ASSERT_TRUE(site_group.FindStringKey("etldPlus1"));
    ASSERT_EQ("google.com", *site_group.FindStringKey("etldPlus1"));
    EXPECT_FALSE(site_group.FindKey("hasInstalledPWA")->GetBool());

    const base::Value* origin_list = site_group.FindListKey("origins");
    ASSERT_TRUE(origin_list);

    for (const auto& origin_info : origin_list->GetList()) {
      ASSERT_TRUE(origin_info.is_dict());
      EXPECT_FALSE(origin_info.FindKey("isInstalled")->GetBool());
    }
  }
}

TEST_F(SiteSettingsHandlerTest, IncognitoExceptions) {
  constexpr char kOriginToBlock[] = "https://www.blocked.com:443";

  auto validate_exception = [&kOriginToBlock](const base::Value& exception) {
    ASSERT_TRUE(exception.is_dict());

    ASSERT_TRUE(exception.FindStringKey(site_settings::kOrigin));
    ASSERT_EQ(kOriginToBlock, *exception.FindStringKey(site_settings::kOrigin));
  };

  CreateIncognitoProfile();

  {
    base::Value::List set_args;
    set_args.Append(kOriginToBlock);  // Primary pattern.
    set_args.Append(std::string());   // Secondary pattern.
    set_args.Append(kNotifications);
    set_args.Append(
        content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
    set_args.Append(true);  // Incognito.

    handler()->HandleSetCategoryPermissionForPattern(set_args);

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
    base::Value::List set_args;
    set_args.Append(kOriginToBlock);  // Primary pattern.
    set_args.Append(std::string());   // Secondary pattern.
    set_args.Append(kNotifications);
    set_args.Append(
        content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
    set_args.Append(false);  // Incognito.

    handler()->HandleSetCategoryPermissionForPattern(set_args);

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
    ASSERT_EQ(1U, web_ui()->call_data().size());
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
        CONTENT_SETTING_BLOCK,
        auto_blocker
            ->GetEmbargoResult(GURL(kOriginToEmbargo), kPermissionNotifications)
            ->content_setting);
  }

  // Check there are 2 blocked origins.
  {
    base::Value::List exceptions;
    site_settings::GetExceptionsForContentType(
        kPermissionNotifications, profile(), /*extension_registry=*/nullptr,
        web_ui(),
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
        kPermissionNotifications, profile(), /*extension_registry=*/nullptr,
        web_ui(),
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
        kPermissionNotifications, profile(), /*extension_registry=*/nullptr,
        web_ui(),
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
  ASSERT_EQ(1U, web_ui()->call_data().size());

  // Reset blocked origin.
  base::Value::List reset_args;
  reset_args.Append(kInvalidOrigin);
  reset_args.Append(std::string());
  reset_args.Append(kNotifications);
  reset_args.Append(false);  // Incognito.
  // Check that this method is not crashing for an invalid origin.
  handler()->HandleResetCategoryPermissionForPattern(reset_args);
}

TEST_F(SiteSettingsHandlerTest, Origins) {
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
    EXPECT_EQ(1U, web_ui()->call_data().size());
  }

  base::Value::List get_exception_list_args;
  get_exception_list_args.Append(kCallbackId);
  get_exception_list_args.Append(kNotifications);
  handler()->HandleGetExceptionList(get_exception_list_args);
  ValidateOrigin(google, "", google, CONTENT_SETTING_BLOCK,
                 site_settings::SiteSettingSource::kPreference, 2U);

  {
    // Reset things back to how they were.
    base::Value::List reset_args;
    reset_args.Append(google);
    reset_args.Append(std::string());
    reset_args.Append(kNotifications);
    reset_args.Append(false);  // Incognito.
    handler()->HandleResetCategoryPermissionForPattern(reset_args);
    EXPECT_EQ(3U, web_ui()->call_data().size());
  }

  // Verify the reset was successful.
  handler()->HandleGetExceptionList(get_exception_list_args);
  ValidateNoOrigin(4U);
}

TEST_F(SiteSettingsHandlerTest, NotificationPermissionRevokeUkm) {
  const std::string google("https://www.google.com");
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile(), ServiceAccessType::EXPLICIT_ACCESS);
  history_service->AddPage(GURL(google), base::Time::Now(),
                           history::SOURCE_BROWSED);
  base::RunLoop origin_queried_waiter;
  history_service->set_origin_queried_closure_for_testing(
      origin_queried_waiter.QuitClosure());

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

  origin_queried_waiter.Run();

  auto entries = ukm_recorder.GetEntriesByName("Permission");
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries.front();

  ukm_recorder.ExpectEntrySourceHasUrl(entry, GURL(google));
  EXPECT_EQ(
      *ukm_recorder.GetEntryMetric(entry, "Source"),
      static_cast<int64_t>(permissions::PermissionSourceUI::SITE_SETTINGS));
  size_t num_values = 0;
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "PermissionType"),
            ContentSettingTypeToHistogramValue(
                ContentSettingsType::NOTIFICATIONS, &num_values));
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "Action"),
            static_cast<int64_t>(permissions::PermissionAction::REVOKED));
}

// TODO(crbug.com/1076294): Test flakes on TSAN and ASAN.
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
  // A user-set pattern should not show up as default.
  handler()->HandleGetOriginPermissions(get_origin_permissions_args);
  ValidateOrigin(google, google, expected_display_name, CONTENT_SETTING_ALLOW,
                 site_settings::SiteSettingSource::kPreference, 5U);

  base::Value::List set_notification_origin_args;
  set_notification_origin_args.Append(google);
  set_notification_origin_args.Append("");
  set_notification_origin_args.Append(kNotifications);
  set_notification_origin_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_BLOCK));
  set_notification_origin_args.Append(false);
  handler()->HandleSetCategoryPermissionForPattern(
      set_notification_origin_args);
  // A user-set per-origin permission should not show up as default.
  handler()->HandleGetOriginPermissions(get_origin_permissions_args);
  ValidateOrigin(google, google, expected_display_name, CONTENT_SETTING_BLOCK,
                 site_settings::SiteSettingSource::kPreference, 7U);

  // Enterprise-policy set defaults should not show up as default.
  source_setter.SetPolicyDefault(CONTENT_SETTING_ALLOW);
  handler()->HandleGetOriginPermissions(get_origin_permissions_args);
  ValidateOrigin(google, google, expected_display_name, CONTENT_SETTING_ALLOW,
                 site_settings::SiteSettingSource::kPolicy, 8U);
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

TEST_F(SiteSettingsHandlerTest, ExceptionHelpers) {
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]google.com");
  base::Value::Dict exception = site_settings::GetExceptionForPage(
      ContentSettingsType::NOTIFICATIONS, /*profile=*/nullptr, pattern,
      ContentSettingsPattern::Wildcard(), pattern.ToString(),
      CONTENT_SETTING_BLOCK,
      site_settings::SiteSettingSourceToString(
          site_settings::SiteSettingSource::kPreference),
      false);

  CHECK(exception.FindString(site_settings::kOrigin));
  CHECK(exception.FindString(site_settings::kDisplayName));
  CHECK(exception.FindString(site_settings::kEmbeddingOrigin));
  CHECK(exception.FindString(site_settings::kSetting));
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
                  .SetManifest(extensions::DictionaryBuilder()
                                   .Set("name", kExtensionName)
                                   .Set("version", "1.0.0")
                                   .Set("manifest_version", 2)
                                   .Build())
                  .SetID("ahfgeienlihckogmohjhadlkjgocpleb")
                  .Build();

  base::Value::List exceptions;
  site_settings::AddExceptionForHostedApp("[*.]google.com", *extension.get(),
                                          &exceptions);

  const base::Value& dictionary = exceptions[0];
  CHECK(dictionary.is_dict());
  CHECK(dictionary.FindStringKey(site_settings::kOrigin));
  CHECK(dictionary.FindStringKey(site_settings::kDisplayName));
  CHECK(dictionary.FindStringKey(site_settings::kEmbeddingOrigin));
  CHECK(dictionary.FindStringKey(site_settings::kSetting));
  CHECK(dictionary.FindBoolKey(site_settings::kIncognito).has_value());

  // Again, don't need to check the results.
  handler()->HandleSetCategoryPermissionForPattern(args);
}

TEST_F(SiteSettingsHandlerTest, ExtensionDisplayName) {
  auto* extension_registry = extensions::ExtensionRegistry::Get(profile());
  std::string test_extension_id = "test-extension-url";
  std::string test_extension_url = "chrome-extension://" + test_extension_id;
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder()
          .SetManifest(extensions::DictionaryBuilder()
                           .Set("name", kExtensionName)
                           .Set("version", "1.0.0")
                           .Set("manifest_version", 2)
                           .Build())
          .SetID(test_extension_id)
          .Build();
  extension_registry->AddEnabled(extension);

  base::Value::List get_origin_permissions_args;
  get_origin_permissions_args.Append(kCallbackId);
  get_origin_permissions_args.Append(test_extension_url);
  {
    base::Value::List category_list;
    category_list.Append(kNotifications);
    get_origin_permissions_args.Append(std::move(category_list));
  }
  handler()->HandleGetOriginPermissions(get_origin_permissions_args);
  ValidateOrigin(test_extension_url, test_extension_url, kExtensionName,
                 CONTENT_SETTING_ASK,
                 site_settings::SiteSettingSource::kDefault, 1U);
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
  std::string host("http://www.google.com");
  double zoom_level = 1.1;

  content::HostZoomMap* host_zoom_map =
      content::HostZoomMap::GetDefaultForBrowserContext(profile());
  host_zoom_map->SetZoomLevelForHost(host, zoom_level);
  ValidateZoom(host, "122%", 1U);

  base::Value::List args;
  handler()->HandleFetchZoomLevels(args);
  ValidateZoom(host, "122%", 2U);

  args.Append("http://www.google.com");
  handler()->HandleRemoveZoomLevel(args);
  ValidateZoom("", "", 3U);

  double default_level = host_zoom_map->GetDefaultZoomLevel();
  double level = host_zoom_map->GetZoomLevelForHostAndScheme("http", host);
  EXPECT_EQ(default_level, level);
}

class SiteSettingsHandlerInfobarTest : public BrowserWithTestWindowTest {
 public:
  SiteSettingsHandlerInfobarTest()
      : kNotifications(site_settings::ContentSettingsTypeToGroupName(
            ContentSettingsType::NOTIFICATIONS)) {}
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

    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile()));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
  }

  void TearDown() override {
    // SiteSettingsHandler maintains a HostZoomMap::Subscription internally, so
    // make sure that's cleared before BrowserContext / profile destruction.
    handler()->DisallowJavascript();

    // Also destroy |browser2_| before the profile. browser()'s destruction is
    // handled in BrowserWithTestWindowTest::TearDown().
    browser2()->tab_strip_model()->CloseAllTabs();
    browser2_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

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

  const std::string kNotifications;

 private:
  content::TestWebUI web_ui_;
  std::unique_ptr<SiteSettingsHandler> handler_;
  std::unique_ptr<BrowserWindow> window2_;
  std::unique_ptr<Browser> browser2_;
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
    EXPECT_EQ(0u,
              GetInfoBarManagerForTab(browser(), i, nullptr)->infobar_count());
  }

  AddTab(browser2(), about);
  AddTab(browser2(), example_subdomain);
  AddTab(browser2(), origin_query);
  AddTab(browser2(), insecure);
  for (int i = 0; i < browser2()->tab_strip_model()->count(); ++i) {
    EXPECT_EQ(0u,
              GetInfoBarManagerForTab(browser2(), i, nullptr)->infobar_count());
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
          1u, GetInfoBarManagerForTab(browser(), i, &tab_url)->infobar_count());
      EXPECT_TRUE(url::IsSameOriginWith(origin, tab_url));
    } else {
      EXPECT_EQ(
          0u, GetInfoBarManagerForTab(browser(), i, &tab_url)->infobar_count());
      EXPECT_FALSE(url::IsSameOriginWith(origin, tab_url));
    }
  }
  for (int i = 0; i < browser2()->tab_strip_model()->count(); ++i) {
    if (i == /*origin_query=*/1) {
      EXPECT_EQ(
          1u,
          GetInfoBarManagerForTab(browser2(), i, &tab_url)->infobar_count());
      EXPECT_TRUE(url::IsSameOriginWith(origin, tab_url));
    } else {
      EXPECT_EQ(
          0u,
          GetInfoBarManagerForTab(browser2(), i, &tab_url)->infobar_count());
      EXPECT_FALSE(url::IsSameOriginWith(origin, tab_url));
    }
  }

  // Navigate the |foo| tab to the same origin as |origin_anchor|, and the
  // |origin_query| tab to a different origin.
  const GURL origin_path("https://www.example.com/path/to/page.html");
  content::WebContents* foo_contents =
      browser()->tab_strip_model()->GetWebContentsAt(/*foo=*/0);
  NavigateAndCommit(foo_contents, origin_path);

  const GURL example_without_www("https://example.com/");
  content::WebContents* origin_query_contents =
      browser2()->tab_strip_model()->GetWebContentsAt(/*origin_query=*/1);
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
          1u, GetInfoBarManagerForTab(browser(), i, &tab_url)->infobar_count());
      EXPECT_TRUE(url::IsSameOriginWith(origin, tab_url));
    } else {
      EXPECT_EQ(
          0u, GetInfoBarManagerForTab(browser(), i, &tab_url)->infobar_count());
      EXPECT_FALSE(url::IsSameOriginWith(origin, tab_url));
    }
  }
  // The infobar on the original |origin_query| tab (which has now been
  // navigated to |example_without_www|) should disappear.
  for (int i = 0; i < browser2()->tab_strip_model()->count(); ++i) {
    EXPECT_EQ(
        0u, GetInfoBarManagerForTab(browser2(), i, &tab_url)->infobar_count());
    EXPECT_FALSE(url::IsSameOriginWith(origin, tab_url));
  }

  // Make sure it's the correct infobar that's being shown.
  EXPECT_EQ(infobars::InfoBarDelegate::PAGE_INFO_INFOBAR_DELEGATE,
            GetInfoBarManagerForTab(browser(), /*origin_path=*/0, &tab_url)
                ->infobar_at(0)
                ->delegate()
                ->GetIdentifier());
  EXPECT_TRUE(url::IsSameOriginWith(origin, tab_url));
}

TEST_F(SiteSettingsHandlerTest, SessionOnlyException) {
  const std::string google_with_port("https://www.google.com:443");
  base::Value::List set_args;
  set_args.Append(google_with_port);  // Primary pattern.
  set_args.Append(std::string());     // Secondary pattern.
  set_args.Append(kCookies);
  set_args.Append(
      content_settings::ContentSettingToString(CONTENT_SETTING_SESSION_ONLY));
  set_args.Append(false);  // Incognito.
  handler()->HandleSetCategoryPermissionForPattern(set_args);

  EXPECT_EQ(kNumberContentSettingListeners, web_ui()->call_data().size());
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
  EXPECT_EQ(content_settings::SETTING_SOURCE_ALLOWLIST, info.source);

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

    const std::string etld_plus1_string =
        site_groups[0].FindKey("etldPlus1")->GetString();
    EXPECT_EQ("example.com", etld_plus1_string);
    const base::Value::List& origin_list =
        site_groups[0].FindKey("origins")->GetList();
    EXPECT_EQ(1UL, origin_list.size());
    EXPECT_EQ(kWebUrl.spec(), origin_list[0].FindKey("origin")->GetString());
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
              exception_list[0].FindKey("origin")->GetString());
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
    EXPECT_EQ(kWebUrl.spec(),
              recent_permission_list[0].FindKey("origin")->GetString());
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

    EXPECT_EQ(origin.GetURL().spec(),
              exception_list[0].FindKey("origin")->GetString());
    EXPECT_EQ("allowlist", exception_list[0].FindKey("source")->GetString());
  }
}

class PersistentPermissionsSiteSettingsHandlerTest
    : public SiteSettingsHandlerTest {
  void SetUp() override {
    SiteSettingsHandlerTest::SetUp();
    handler_ = std::make_unique<SiteSettingsHandler>(&profile_);
    handler_->set_web_ui(web_ui());
    handler_->AllowJavascript();
    web_ui()->ClearTrackedCalls();
  }

  void TearDown() override { handler_->DisallowJavascript(); }

 public:
  PersistentPermissionsSiteSettingsHandlerTest() {
    // TODO(crbug.com/1373962): Remove this feature list enabler
    // when Persistent Permissions is launched.

    // Enable Persisted Permissions.
    feature_list_.InitAndEnableFeature(
        features::kFileSystemAccessPersistentPermissions);
  }

 protected:
  std::unique_ptr<SiteSettingsHandler> handler_;
  TestingProfile profile_;

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

  const base::FilePath kTestPath = base::FilePath(FILE_PATH_LITERAL("/a/b/"));
  const base::FilePath kTestPath2 = base::FilePath(FILE_PATH_LITERAL("/c/d/"));
  const base::FilePath kTestPath3 = base::FilePath(FILE_PATH_LITERAL("/e/"));
  const base::FilePath kTestPath4 =
      base::FilePath(FILE_PATH_LITERAL("/f/g/h/"));

  // Populate the `grants` object with permissions.
  auto file_read_grant = context->GetPersistedReadPermissionGrantForTesting(
      kTestOrigin1, kTestPath,
      ChromeFileSystemAccessPermissionContext::HandleType::kFile);
  auto file_write_grant = context->GetPersistedWritePermissionGrantForTesting(
      kTestOrigin2, kTestPath2,
      ChromeFileSystemAccessPermissionContext::HandleType::kFile);
  auto directory_read_grant =
      context->GetPersistedReadPermissionGrantForTesting(
          kTestOrigin1, kTestPath3,
          ChromeFileSystemAccessPermissionContext::HandleType::kDirectory);
  auto directory_write_grant =
      context->GetPersistedWritePermissionGrantForTesting(
          kTestOrigin2, kTestPath4,
          ChromeFileSystemAccessPermissionContext::HandleType::kDirectory);

  EXPECT_EQ(context->GetPermissionGrants(kTestOrigin1).file_read_grants.size(),
            1UL);
  EXPECT_EQ(context->GetPermissionGrants(kTestOrigin2).file_read_grants.size(),
            0UL);
  EXPECT_EQ(context->GetPermissionGrants(kTestOrigin1).file_write_grants.size(),
            0UL);
  EXPECT_EQ(context->GetPermissionGrants(kTestOrigin2).file_write_grants.size(),
            1UL);
  EXPECT_EQ(
      context->GetPermissionGrants(kTestOrigin1).directory_read_grants.size(),
      1UL);
  EXPECT_EQ(
      context->GetPermissionGrants(kTestOrigin2).directory_read_grants.size(),
      0UL);
  EXPECT_EQ(
      context->GetPermissionGrants(kTestOrigin1).directory_write_grants.size(),
      0UL);
  EXPECT_EQ(
      context->GetPermissionGrants(kTestOrigin2).directory_write_grants.size(),
      1UL);

  url::Origin originToTest = kTestOrigin1;
  base::Value::List get_file_system_origin_permissions_args;
  get_file_system_origin_permissions_args.Append(kCallbackId);
  get_file_system_origin_permissions_args.Append(originToTest.GetURL().spec());

  handler_->HandleGetFileSystemGrants(get_file_system_origin_permissions_args);
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  const base::Value::List& grants = data.arg3()->GetList();

  EXPECT_EQ(grants.size(), 2UL);

  EXPECT_FALSE(grants[0].FindKey(site_settings::kIsDirectory)->GetBool());
  EXPECT_TRUE(grants[1].FindKey(site_settings::kIsDirectory)->GetBool());

  EXPECT_EQ(grants[0].FindKey(site_settings::kDisplayName)->GetString(),
            FilePathToValue(file_read_grant->GetPath()).GetString());
  EXPECT_EQ(grants[1].FindKey(site_settings::kDisplayName)->GetString(),
            FilePathToValue(directory_read_grant->GetPath()).GetString());

  EXPECT_FALSE(grants[0].FindKey(site_settings::kIsWritable)->GetBool());
  EXPECT_FALSE(grants[1].FindKey(site_settings::kIsWritable)->GetBool());

  originToTest = kTestOrigin2;
  base::Value::List get_file_system_origin2_permissions_args;
  get_file_system_origin2_permissions_args.Append(kCallbackId);
  get_file_system_origin2_permissions_args.Append(originToTest.GetURL().spec());

  handler_->HandleGetFileSystemGrants(get_file_system_origin2_permissions_args);
  const content::TestWebUI::CallData& origin2_data =
      *web_ui()->call_data().back();
  const base::Value::List& origin2_grants = origin2_data.arg3()->GetList();

  EXPECT_EQ(origin2_grants.size(), 2UL);

  EXPECT_FALSE(
      origin2_grants[0].FindKey(site_settings::kIsDirectory)->GetBool());
  EXPECT_TRUE(
      origin2_grants[1].FindKey(site_settings::kIsDirectory)->GetBool());

  EXPECT_EQ(origin2_grants[0].FindKey(site_settings::kDisplayName)->GetString(),
            FilePathToValue(file_write_grant->GetPath()).GetString());
  EXPECT_EQ(origin2_grants[1].FindKey(site_settings::kDisplayName)->GetString(),
            FilePathToValue(directory_write_grant->GetPath()).GetString());

  EXPECT_TRUE(origin2_grants[0].FindKey(site_settings::kIsWritable)->GetBool());
  EXPECT_TRUE(origin2_grants[1].FindKey(site_settings::kIsWritable)->GetBool());
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

}  // namespace

class SiteSettingsHandlerChooserExceptionTest : public SiteSettingsHandlerTest {
 protected:
  const GURL kAndroidUrl{"https://android.com"};
  const GURL kChromiumUrl{"https://chromium.org"};
  const GURL kGoogleUrl{"https://google.com"};
  const GURL kWebUIUrl{"chrome://test"};

  void SetUp() override {
    // Set up UsbChooserContext first, since the granting of device permissions
    // causes the WebUI listener callbacks for
    // contentSettingSitePermissionChanged and
    // contentSettingChooserPermissionChanged to be fired. The base class SetUp
    // method reset the WebUI call data.
    SetUpUsbChooserContext();
    SiteSettingsHandlerTest::SetUp();
  }

  void TearDown() override {
    auto* chooser_context = UsbChooserContextFactory::GetForProfile(profile());
    chooser_context->permissions::ObjectPermissionContextBase::RemoveObserver(
        &observer_);
  }

  // Sets up the UsbChooserContext with two devices and permissions for these
  // devices. It also adds three policy defined permissions. There are three
  // devices that are granted user permissions. Two are covered by different
  // policy permissions, while the third is not covered by policy at all. These
  // unit tests will check that the WebUI is able to receive the exceptions and
  // properly manipulate their permissions.
  void SetUpUsbChooserContext() {
    persistent_device_info_ = device_manager_.CreateAndAddDevice(
        6353, 5678, "Google", "Gizmo", "123ABC");
    ephemeral_device_info_ =
        device_manager_.CreateAndAddDevice(6354, 0, "Google", "Gadget", "");
    user_granted_device_info_ = device_manager_.CreateAndAddDevice(
        6355, 0, "Google", "Widget", "789XYZ");

    auto* chooser_context = UsbChooserContextFactory::GetForProfile(profile());
    mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager;
    device_manager_.AddReceiver(
        device_manager.InitWithNewPipeAndPassReceiver());
    chooser_context->SetDeviceManagerForTesting(std::move(device_manager));
    chooser_context->GetDevices(base::DoNothing());
    base::RunLoop().RunUntilIdle();

    const auto kAndroidOrigin = url::Origin::Create(kAndroidUrl);
    const auto kChromiumOrigin = url::Origin::Create(kChromiumUrl);
    const auto kGoogleOrigin = url::Origin::Create(kGoogleUrl);
    const auto kWebUIOrigin = url::Origin::Create(kWebUIUrl);

    // Add the user granted permissions for testing.
    // These two persistent device permissions should be lumped together with
    // the policy permissions, since they apply to the same device and URL.
    chooser_context->GrantDevicePermission(kChromiumOrigin,
                                           *persistent_device_info_);
    chooser_context->GrantDevicePermission(kGoogleOrigin,
                                           *persistent_device_info_);
    chooser_context->GrantDevicePermission(kWebUIOrigin,
                                           *persistent_device_info_);
    chooser_context->GrantDevicePermission(kAndroidOrigin,
                                           *ephemeral_device_info_);
    chooser_context->GrantDevicePermission(kAndroidOrigin,
                                           *user_granted_device_info_);

    // Add the policy granted permissions for testing.
    auto policy_value = base::JSONReader::ReadDeprecated(kUsbPolicySetting);
    DCHECK(policy_value);
    profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                               *policy_value);

    // Add the observer for permission changes.
    chooser_context->permissions::ObjectPermissionContextBase::AddObserver(
        &observer_);
  }

  void SetUpOffTheRecordUsbChooserContext() {
    off_the_record_device_ = device_manager_.CreateAndAddDevice(
        6353, 8765, "Google", "Contraption", "A9B8C7");

    CreateIncognitoProfile();
    auto* chooser_context =
        UsbChooserContextFactory::GetForProfile(incognito_profile());
    mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager;
    device_manager_.AddReceiver(
        device_manager.InitWithNewPipeAndPassReceiver());
    chooser_context->SetDeviceManagerForTesting(std::move(device_manager));
    chooser_context->GetDevices(base::DoNothing());
    base::RunLoop().RunUntilIdle();

    const auto kChromiumOrigin = url::Origin::Create(kChromiumUrl);
    chooser_context->GrantDevicePermission(kChromiumOrigin,
                                           *off_the_record_device_);

    // Add the observer for permission changes.
    chooser_context->permissions::ObjectPermissionContextBase::AddObserver(
        &observer_);
  }

  void DestroyIncognitoProfile() override {
    auto* chooser_context =
        UsbChooserContextFactory::GetForProfile(incognito_profile());
    chooser_context->permissions::ObjectPermissionContextBase::RemoveObserver(
        &observer_);

    SiteSettingsHandlerTest::DestroyIncognitoProfile();
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

  const base::Value& GetChooserExceptionListFromWebUiCallData(
      const std::string& chooser_type,
      size_t expected_total_calls) {
    ValidateChooserExceptionList(chooser_type, expected_total_calls);
    return *web_ui()->call_data().back()->arg3();
  }

  // Iterate through the exception's sites array and return true if a site
  // exception matches |requesting_origin| and |embedding_origin|.
  bool ChooserExceptionContainsSiteException(const base::Value& exception,
                                             const std::string& origin) {
    const base::Value* sites = exception.FindListKey(site_settings::kSites);
    if (!sites)
      return false;

    for (const auto& site : sites->GetList()) {
      const std::string* exception_origin =
          site.FindStringKey(site_settings::kOrigin);
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
  bool ChooserExceptionContainsSiteException(const base::Value& exceptions,
                                             const std::string& display_name,
                                             const std::string& origin) {
    if (!exceptions.is_list())
      return false;

    for (const auto& exception : exceptions.GetList()) {
      const std::string* exception_display_name =
          exception.FindStringKey(site_settings::kDisplayName);
      if (!exception_display_name)
        continue;

      if (*exception_display_name == display_name) {
        return ChooserExceptionContainsSiteException(exception, origin);
      }
    }
    return false;
  }

  device::mojom::UsbDeviceInfoPtr ephemeral_device_info_;
  device::mojom::UsbDeviceInfoPtr off_the_record_device_;
  device::mojom::UsbDeviceInfoPtr persistent_device_info_;
  device::mojom::UsbDeviceInfoPtr user_granted_device_info_;

  permissions::MockPermissionObserver observer_;

 private:
  device::FakeUsbDeviceManager device_manager_;
};

TEST_F(SiteSettingsHandlerChooserExceptionTest,
       HandleGetChooserExceptionListForUsb) {
  const std::string kUsbChooserGroupName(
      site_settings::ContentSettingsTypeToGroupName(
          ContentSettingsType::USB_CHOOSER_DATA));

  const base::Value& exceptions = GetChooserExceptionListFromWebUiCallData(
      kUsbChooserGroupName, /*expected_total_calls=*/1u);
  EXPECT_EQ(exceptions.GetList().size(), 5u);

  // Don't include WebUI schemes.
  const std::string kWebUIOriginStr =
      kWebUIUrl.DeprecatedGetOriginAsURL().spec();
  EXPECT_FALSE(ChooserExceptionContainsSiteException(exceptions, "Gizmo",
                                                     kWebUIOriginStr));
}

TEST_F(SiteSettingsHandlerChooserExceptionTest,
       HandleGetChooserExceptionListForUsbOffTheRecord) {
  const std::string kUsbChooserGroupName(
      site_settings::ContentSettingsTypeToGroupName(
          ContentSettingsType::USB_CHOOSER_DATA));
  SetUpOffTheRecordUsbChooserContext();
  web_ui()->ClearTrackedCalls();

  // The objects returned by GetChooserExceptionListFromProfile should also
  // include the incognito permissions. The two extra objects represent the
  // "Widget" device and the policy permission for "Unknown product 0x162E from
  // Google Inc.". The policy granted permission shows up here because the off
  // the record profile does not have a user granted permission for the
  // |persistent_device_info_|, so it cannot use the name of that device.
  {
    const base::Value& exceptions = GetChooserExceptionListFromWebUiCallData(
        kUsbChooserGroupName, /*expected_total_calls=*/1u);
    EXPECT_EQ(exceptions.GetList().size(), 7u);
    for (const auto& exception : exceptions.GetList()) {
      LOG(INFO) << exception.FindKey(site_settings::kDisplayName)->GetString();
    }
  }

  // Destroy the off the record profile and check that the objects returned do
  // not include incognito permissions anymore. The destruction of the profile
  // causes the "onIncognitoStatusChanged" WebUIListener callback to fire.
  DestroyIncognitoProfile();
  EXPECT_EQ(web_ui()->call_data().size(), 2u);

  {
    const base::Value& exceptions = GetChooserExceptionListFromWebUiCallData(
        kUsbChooserGroupName, /*expected_total_calls=*/3u);
    EXPECT_EQ(exceptions.GetList().size(), 5u);
  }
}

TEST_F(SiteSettingsHandlerChooserExceptionTest,
       HandleResetChooserExceptionForSiteForUsb) {
  const std::string kUsbChooserGroupName(
      site_settings::ContentSettingsTypeToGroupName(
          ContentSettingsType::USB_CHOOSER_DATA));
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
    const base::Value& exceptions = GetChooserExceptionListFromWebUiCallData(
        kUsbChooserGroupName, /*expected_total_calls=*/1u);
    EXPECT_EQ(exceptions.GetList().size(), 5u);
  }

  // User granted USB permissions for devices also containing policy permissions
  // should be able to be reset without removing the chooser exception object
  // from the list.
  base::Value::List args;
  args.Append(kUsbChooserGroupName);
  args.Append("https://unused.com");
  args.Append(kGoogleOriginStr);
  args.Append(UsbChooserContext::DeviceInfoToValue(*persistent_device_info_));

  EXPECT_CALL(observer_,
              OnObjectPermissionChanged(absl::optional<ContentSettingsType>(
                                            ContentSettingsType::USB_GUARD),
                                        ContentSettingsType::USB_CHOOSER_DATA));
  EXPECT_CALL(observer_, OnPermissionRevoked(kGoogleOrigin));
  handler()->HandleResetChooserExceptionForSite(args);
  auto* chooser_context = UsbChooserContextFactory::GetForProfile(profile());
  chooser_context->FlushScheduledSaveSettingsCalls();

  // The HandleResetChooserExceptionForSite() method should have also caused the
  // WebUIListenerCallbacks for contentSettingSitePermissionChanged and
  // contentSettingChooserPermissionChanged to fire.
  EXPECT_EQ(web_ui()->call_data().size(), 3u);
  {
    // The exception list size should not have been reduced since there is still
    // a policy granted permission for the "Gizmo" device.
    const base::Value& exceptions = GetChooserExceptionListFromWebUiCallData(
        kUsbChooserGroupName, /*expected_total_calls=*/4u);
    EXPECT_EQ(exceptions.GetList().size(), 5u);

    // Ensure that the sites list does not contain the URLs of the removed
    // permission.
    EXPECT_FALSE(ChooserExceptionContainsSiteException(exceptions, "Gizmo",
                                                       kGoogleOriginStr));
  }

  // User granted USB permissions that are also granted by policy should not
  // be able to be reset.
  args.clear();
  args.Append(kUsbChooserGroupName);
  args.Append("https://unused.com");
  args.Append(kChromiumOriginStr);
  args.Append(UsbChooserContext::DeviceInfoToValue(*persistent_device_info_));

  {
    const base::Value& exceptions =
        GetChooserExceptionListFromWebUiCallData(kUsbChooserGroupName, 5u);
    EXPECT_EQ(exceptions.GetList().size(), 5u);

    // User granted exceptions that are also granted by policy are only
    // displayed through the policy granted site exception, so ensure that the
    // policy exception is present under the "Gizmo" device.
    EXPECT_TRUE(ChooserExceptionContainsSiteException(exceptions, "Gizmo",
                                                      kChromiumOriginStr));
    EXPECT_FALSE(ChooserExceptionContainsSiteException(exceptions, "Gizmo",
                                                       kGoogleOriginStr));
  }

  EXPECT_CALL(observer_,
              OnObjectPermissionChanged(absl::optional<ContentSettingsType>(
                                            ContentSettingsType::USB_GUARD),
                                        ContentSettingsType::USB_CHOOSER_DATA));
  EXPECT_CALL(observer_, OnPermissionRevoked(kChromiumOrigin));
  handler()->HandleResetChooserExceptionForSite(args);
  chooser_context->FlushScheduledSaveSettingsCalls();

  // The HandleResetChooserExceptionForSite() method should have also caused the
  // WebUIListenerCallbacks for contentSettingSitePermissionChanged and
  // contentSettingChooserPermissionChanged to fire.
  EXPECT_EQ(web_ui()->call_data().size(), 7u);
  {
    const base::Value& exceptions = GetChooserExceptionListFromWebUiCallData(
        kUsbChooserGroupName, /*expected_total_calls=*/8u);
    EXPECT_EQ(exceptions.GetList().size(), 5u);

    // Ensure that the sites list still displays a site exception entry for an
    // origin of kGoogleOriginStr.  Since now the device has had its
    // permission revoked, the policy-provided object will not be able to deduce
    // the name "Gizmo" from the connected device. As such we check that the
    // policy is still active by looking for the genericly constructed name.
    EXPECT_TRUE(ChooserExceptionContainsSiteException(
        exceptions, "Unknown product 0x162E from Google Inc.",
        kChromiumOriginStr));
    EXPECT_FALSE(ChooserExceptionContainsSiteException(exceptions, "Gizmo",
                                                       kGoogleOriginStr));
  }

  // User granted USB permissions that are not covered by policy should be able
  // to be reset and the chooser exception entry should be removed from the list
  // when the exception only has one site exception granted to it..
  args.clear();
  args.Append(kUsbChooserGroupName);
  args.Append("https://unused.com");
  args.Append(kAndroidOriginStr);
  args.Append(UsbChooserContext::DeviceInfoToValue(*user_granted_device_info_));

  {
    const base::Value& exceptions =
        GetChooserExceptionListFromWebUiCallData(kUsbChooserGroupName, 9u);
    EXPECT_EQ(exceptions.GetList().size(), 5u);
    EXPECT_TRUE(ChooserExceptionContainsSiteException(exceptions, "Widget",
                                                      kAndroidOriginStr));
  }

  EXPECT_CALL(observer_,
              OnObjectPermissionChanged(absl::optional<ContentSettingsType>(
                                            ContentSettingsType::USB_GUARD),
                                        ContentSettingsType::USB_CHOOSER_DATA));
  EXPECT_CALL(observer_, OnPermissionRevoked(kAndroidOrigin));
  handler()->HandleResetChooserExceptionForSite(args);
  chooser_context->FlushScheduledSaveSettingsCalls();

  // The HandleResetChooserExceptionForSite() method should have also caused the
  // WebUIListenerCallbacks for contentSettingSitePermissionChanged and
  // contentSettingChooserPermissionChanged to fire.
  EXPECT_EQ(web_ui()->call_data().size(), 11u);
  {
    const base::Value& exceptions = GetChooserExceptionListFromWebUiCallData(
        kUsbChooserGroupName, /*expected_total_calls=*/12u);
    EXPECT_EQ(exceptions.GetList().size(), 4u);
    EXPECT_FALSE(ChooserExceptionContainsSiteException(exceptions, "Widget",
                                                       kAndroidOriginStr));
  }
}

TEST_F(SiteSettingsHandlerTest, HandleClearEtldPlus1DataAndCookies) {
  SetupModels();

  EXPECT_EQ(28u,
            handler()->cookies_tree_model_->GetRoot()->GetTotalNodeCount());

  auto verify_site_group = [](const base::Value& site_group,
                              std::string expected_etld_plus1) {
    ASSERT_TRUE(site_group.is_dict());
    const std::string* etld_plus1 = site_group.FindStringKey("etldPlus1");
    ASSERT_TRUE(etld_plus1);
    ASSERT_EQ(expected_etld_plus1, *etld_plus1);
  };

  base::Value::List storage_and_cookie_list = GetOnStorageFetchedSentList();
  EXPECT_EQ(4U, storage_and_cookie_list.size());
  verify_site_group(storage_and_cookie_list[0], "example.com");

  base::Value::List args;
  args.Append("example.com");
  handler()->HandleClearEtldPlus1DataAndCookies(args);

  // All host nodes for non-secure example.com, and abc.example.com, which do
  // not have any unpartitioned  storage, should have been removed.
  ASSERT_EQ(0u, GetHostNodes(GURL("http://example.com")).size());
  ASSERT_EQ(0u, GetHostNodes(GURL("http://abc.example.com")).size());

  // Confirm that partitioned cookies for www.example.com have not been deleted,
  auto remaining_host_nodes = GetHostNodes(GURL("https://www.example.com"));

  // example.com storage partitioned on other sites should still remain.
  {
    ASSERT_EQ(1u, remaining_host_nodes.size());
    ASSERT_EQ(1u, remaining_host_nodes[0]->children().size());
    const auto& storage_node = remaining_host_nodes[0]->children()[0];
    ASSERT_EQ(CookieTreeNode::DetailedInfo::TYPE_COOKIES,
              storage_node->GetDetailedInfo().node_type);
    ASSERT_EQ(2u, storage_node->children().size());
    for (const auto& cookie_node : storage_node->children()) {
      const auto& cookie = cookie_node->GetDetailedInfo().cookie;
      EXPECT_EQ("www.example.com", cookie->Domain());
      EXPECT_TRUE(cookie->IsPartitioned());
    }
  }

  EXPECT_EQ(19u,
            handler()->cookies_tree_model_->GetRoot()->GetTotalNodeCount());

  storage_and_cookie_list = GetOnStorageFetchedSentList();
  EXPECT_EQ(3U, storage_and_cookie_list.size());
  verify_site_group(storage_and_cookie_list[0], "google.com");

  args.clear();
  args.Append("google.com");

  handler()->HandleClearEtldPlus1DataAndCookies(args);

  EXPECT_EQ(14u,
            handler()->cookies_tree_model_->GetRoot()->GetTotalNodeCount());

  storage_and_cookie_list = GetOnStorageFetchedSentList();
  EXPECT_EQ(2U, storage_and_cookie_list.size());
  verify_site_group(storage_and_cookie_list[0], "google.com.au");

  args.clear();
  args.Append("google.com.au");

  handler()->HandleClearEtldPlus1DataAndCookies(args);
  // No nodes representing storage partitioned on google.com.au should be
  // present.
  for (const auto& host_node :
       handler()->cookies_tree_model_->GetRoot()->children()) {
    for (const auto& storage_node : host_node->children()) {
      if (storage_node->GetDetailedInfo().node_type !=
          CookieTreeNode::DetailedInfo::TYPE_COOKIES) {
        continue;
      }
      for (const auto& cookie_node : storage_node->children()) {
        const auto& cookie = cookie_node->GetDetailedInfo().cookie;
        if (cookie->IsPartitioned()) {
          EXPECT_NE("google.com.au",
                    cookie->PartitionKey()->site().GetURL().host());
        }
      }
    }
  }

  storage_and_cookie_list = GetOnStorageFetchedSentList();
  EXPECT_EQ(1U, storage_and_cookie_list.size());
  verify_site_group(storage_and_cookie_list[0], "ungrouped.com");

  args.clear();
  args.Append("ungrouped.com");

  handler()->HandleClearEtldPlus1DataAndCookies(args);

  storage_and_cookie_list = GetOnStorageFetchedSentList();
  EXPECT_EQ(0U, storage_and_cookie_list.size());
}

TEST_P(SiteSettingsHandlerTest, HandleClearUnpartitionedUsage) {
  SetupModels();

  EXPECT_EQ(28u,
            handler()->cookies_tree_model_->GetRoot()->GetTotalNodeCount());
  EXPECT_EQ(1, std::distance(handler()->browsing_data_model_->begin(),
                             handler()->browsing_data_model_->end()));

  base::Value::List args;
  args.Append(GetParam() ? "https://www.example.com/"
                         : "http://www.example.com/");
  handler()->HandleClearUnpartitionedUsage(args);

  EXPECT_EQ(1, std::distance(handler()->browsing_data_model_->begin(),
                             handler()->browsing_data_model_->end()));

  // Confirm that only the unpartitioned items for example.com have been
  // cleared.
  auto remaining_host_nodes = GetHostNodes(GURL("https://www.example.com"));

  // There should only be partitioned cookie entries remaining for the site.
  ASSERT_EQ(1u, remaining_host_nodes.size());
  ASSERT_EQ(1u, remaining_host_nodes[0]->children().size());
  const auto& storage_node = remaining_host_nodes[0]->children()[0];
  ASSERT_EQ(CookieTreeNode::DetailedInfo::TYPE_COOKIES,
            storage_node->GetDetailedInfo().node_type);
  ASSERT_EQ(2u, storage_node->children().size());
  for (const auto& cookie_node : storage_node->children()) {
    const auto& cookie = cookie_node->GetDetailedInfo().cookie;
    EXPECT_EQ("www.example.com", cookie->Domain());
    EXPECT_TRUE(cookie->IsPartitioned());
  }

  // Partitioned storage, even when keyed on the cookie domain site, should
  // not be cleared.
  args = base::Value::List();
  args.Append("https://google.com.au/");
  handler()->HandleClearUnpartitionedUsage(args);

  remaining_host_nodes = GetHostNodes(GURL("https://google.com.au"));

  // A single partitioned cookie should remain.
  ASSERT_EQ(1u, remaining_host_nodes.size());
  ASSERT_EQ(1u, remaining_host_nodes[0]->children().size());
  const auto& cookies_node = remaining_host_nodes[0]->children()[0];
  ASSERT_EQ(1u, cookies_node->children().size());
  const auto& cookie_node = cookies_node->children()[0];
  const auto& cookie = cookie_node->GetDetailedInfo().cookie;
  EXPECT_TRUE(cookie->IsPartitioned());

  args = base::Value::List();
  args.Append("https://www.google.com/");
  handler()->HandleClearUnpartitionedUsage(args);

  EXPECT_EQ(0, std::distance(handler()->browsing_data_model_->begin(),
                             handler()->browsing_data_model_->end()));

// Clearing Site Specific Media Licenses Tests
#if BUILDFLAG(IS_WIN)
  PrefService* user_prefs = profile()->GetPrefs();

  // In the beginning, there should be nothing stored in the origin data.
  ASSERT_EQ(0u, user_prefs->GetDict(prefs::kMediaCdmOriginData).size());

  base::Value::Dict entry_google;
  entry_google.Set(
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

  args = base::Value::List();
  args.Append("https://www.google.com/");
  handler()->HandleClearUnpartitionedUsage(args);

  // The code clears the origin data for just google.com, so there should still
  // be the origin data for example.com left.
  EXPECT_EQ(1u, user_prefs->GetDict(prefs::kMediaCdmOriginData).size());
  EXPECT_TRUE(user_prefs->GetDict(prefs::kMediaCdmOriginData)
                  .contains("https://www.example.com/"));

#endif  // BUILDFLAG(IS_WIN)
}

TEST_F(SiteSettingsHandlerTest, ClearClientHints) {
  // Confirm that when the user clears unpartitioned storage, or the eTLD+1
  // group, client hints are also cleared.
  SetupModels();
  handler()->OnStorageFetched();

  GURL hosts[] = {GURL("https://example.com/"), GURL("https://www.example.com"),
                  GURL("https://google.com/"), GURL("https://www.google.com/")};

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  ContentSettingsForOneType client_hints_settings;

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
  args.Append("example.com");
  handler()->HandleClearEtldPlus1DataAndCookies(args);
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, &client_hints_settings);
  EXPECT_EQ(2U, client_hints_settings.size());

  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[2]),
            client_hints_settings.at(0).primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            client_hints_settings.at(0).secondary_pattern);
  EXPECT_EQ(client_hints_dictionary, client_hints_settings.at(0).setting_value);

  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[3]),
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
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, &client_hints_settings);
  EXPECT_EQ(1U, client_hints_settings.size());

  // www.google.com should be the only remainining entry.
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
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, &client_hints_settings);
  EXPECT_EQ(0U, client_hints_settings.size());
}

TEST_F(SiteSettingsHandlerTest, ClearReducedAcceptLanguage) {
  // Confirm that when the user clears unpartitioned storage, or the eTLD+1
  // group, reduce accept language are also cleared.
  SetupModels();
  handler()->OnStorageFetched();

  GURL hosts[] = {GURL("https://example.com/"), GURL("https://www.example.com"),
                  GURL("https://google.com/"), GURL("https://www.google.com/")};

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  ContentSettingsForOneType accept_language_settings;

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
  args.Append("example.com");
  handler()->HandleClearEtldPlus1DataAndCookies(args);
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::REDUCED_ACCEPT_LANGUAGE, &accept_language_settings);
  EXPECT_EQ(2U, accept_language_settings.size());

  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[2]),
            accept_language_settings.at(0).primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            accept_language_settings.at(0).secondary_pattern);
  EXPECT_EQ(accept_language_dictionary,
            accept_language_settings.at(0).setting_value);

  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[3]),
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
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::REDUCED_ACCEPT_LANGUAGE, &accept_language_settings);
  EXPECT_EQ(1U, accept_language_settings.size());

  // www.google.com should be the only remainining entry.
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
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::REDUCED_ACCEPT_LANGUAGE, &accept_language_settings);
  EXPECT_EQ(0U, accept_language_settings.size());
}

TEST_F(SiteSettingsHandlerTest, HandleClearPartitionedUsage) {
  // Confirm that removing unpartitioned storage correctly removes the
  // appropriate nodes.
  SetupModels();
  EXPECT_EQ(28u,
            handler()->cookies_tree_model_->GetRoot()->GetTotalNodeCount());
  EXPECT_EQ(1, std::distance(handler()->browsing_data_model_->begin(),
                             handler()->browsing_data_model_->end()));

  base::Value::List args;
  args.Append("https://www.example.com/");
  args.Append("google.com");
  handler()->HandleClearPartitionedUsage(args);

  // This should have only removed cookies for embedded.com partitioned on
  // google.com, leaving other cookies and storage untouched.
  auto remaining_host_nodes = GetHostNodes(GURL("https://www.example.com"));
  ASSERT_EQ(1u, remaining_host_nodes.size());

  // Both cookies and local storage type nodes should remain.
  ASSERT_EQ(2u, remaining_host_nodes[0]->children().size());

  for (const auto& storage_node : remaining_host_nodes[0]->children()) {
    if (storage_node->GetDetailedInfo().node_type ==
        CookieTreeNode::DetailedInfo::TYPE_COOKIES) {
      // Two cookies should remain, one unpartitioned and one partitioned on
      // a different site.
      ASSERT_EQ(2u, storage_node->children().size());
      for (const auto& cookie_node : storage_node->children()) {
        const auto& cookie = cookie_node->GetDetailedInfo().cookie;
        if (cookie->IsPartitioned())
          ASSERT_EQ("google.com.au",
                    cookie->PartitionKey()->site().GetURL().host());
      }
    } else {
      ASSERT_EQ(storage_node->GetDetailedInfo().node_type,
                CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGES);
    }
  }

  // Should not have affected the browsing data model.
  // TODO(crbug.com/1271155): Update when partitioned storage is represented
  // by the browsing data model.
  EXPECT_EQ(1, std::distance(handler()->browsing_data_model_->begin(),
                             handler()->browsing_data_model_->end()));
}

TEST_F(SiteSettingsHandlerTest, CookieSettingDescription) {
  const auto kBlocked = [](int num) {
    return l10n_util::GetPluralStringFUTF8(
        IDS_SETTINGS_SITE_SETTINGS_COOKIES_BLOCK, num);
  };
  const auto kAllowed = [](int num) {
    return l10n_util::GetPluralStringFUTF8(
        IDS_SETTINGS_SITE_SETTINGS_COOKIES_ALLOW, num);
  };
  const std::string kBlockThirdParty = l10n_util::GetStringUTF8(
      IDS_SETTINGS_SITE_SETTINGS_COOKIES_BLOCK_THIRD_PARTY);
  const std::string kBlockThirdPartyIncognito = l10n_util::GetStringUTF8(
      IDS_SETTINGS_SITE_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_INCOGNITO);

  // Enforce expected default profile setting.
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kIncognitoOnly));
  auto* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile());
  content_settings->SetDefaultContentSetting(
      ContentSettingsType::COOKIES, ContentSetting::CONTENT_SETTING_ALLOW);
  web_ui()->ClearTrackedCalls();

  // Validate get method works.
  base::Value::List get_args;
  get_args.Append(kCallbackId);
  handler()->HandleGetCookieSettingDescription(get_args);
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();

  EXPECT_EQ("cr.webUIResponse", data.function_name());
  EXPECT_EQ(kCallbackId, data.arg1()->GetString());
  ASSERT_TRUE(data.arg2()->GetBool());
  EXPECT_EQ(kBlockThirdPartyIncognito, data.arg3()->GetString());

  // Multiple listeners will be called when prefs and content settings are
  // changed in this test. Increment our expected call_data index accordingly.
  int expected_call_index = 0;
  const int kPrefListenerIndex = 1;
  const int kContentSettingListenerIndex = 2;

  // Check updates are working,
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));
  expected_call_index += kPrefListenerIndex;
  ValidateCookieSettingUpdate(kBlockThirdParty, expected_call_index);

  content_settings->SetDefaultContentSetting(
      ContentSettingsType::COOKIES, ContentSetting::CONTENT_SETTING_BLOCK);
  expected_call_index += kContentSettingListenerIndex;
  ValidateCookieSettingUpdate(kBlocked(0), expected_call_index);

  // Check changes which do not affect the effective cookie setting.
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kOff));
  expected_call_index += kPrefListenerIndex;
  ValidateCookieSettingUpdate(kBlocked(0), expected_call_index);

  // Set to allow and check previous changes are respected.
  content_settings->SetDefaultContentSetting(
      ContentSettingsType::COOKIES, ContentSetting::CONTENT_SETTING_ALLOW);
  expected_call_index += kContentSettingListenerIndex;
  ValidateCookieSettingUpdate(kAllowed(0), expected_call_index);

  // Confirm exceptions are counted correctly.
  GURL url1("https://example.com");
  GURL url2("http://example.com");
  GURL url3("http://another.example.com");
  content_settings->SetContentSettingDefaultScope(
      url1, url1, ContentSettingsType::COOKIES,
      ContentSetting::CONTENT_SETTING_BLOCK);
  expected_call_index += kContentSettingListenerIndex;
  ValidateCookieSettingUpdate(kAllowed(1), expected_call_index);

  content_settings->SetContentSettingDefaultScope(
      url2, url2, ContentSettingsType::COOKIES,
      ContentSetting::CONTENT_SETTING_ALLOW);
  expected_call_index += kContentSettingListenerIndex;
  ValidateCookieSettingUpdate(kAllowed(1), expected_call_index);

  content_settings->SetContentSettingDefaultScope(
      url3, url3, ContentSettingsType::COOKIES,
      ContentSetting::CONTENT_SETTING_SESSION_ONLY);
  expected_call_index += kContentSettingListenerIndex;
  ValidateCookieSettingUpdate(kAllowed(1), expected_call_index);

  content_settings->SetDefaultContentSetting(
      ContentSettingsType::COOKIES, ContentSetting::CONTENT_SETTING_BLOCK);
  expected_call_index += kContentSettingListenerIndex;
  ValidateCookieSettingUpdate(kBlocked(2), expected_call_index);
}

TEST_F(SiteSettingsHandlerTest, HandleGetFpsMembershipLabel) {
  base::Value::List args;
  args.Append("getFpsMembershipLabel");
  args.Append(5);
  args.Append("google.com");
  handler()->HandleGetFpsMembershipLabel(args);
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();

  EXPECT_EQ("cr.webUIResponse", data.function_name());
  EXPECT_EQ("getFpsMembershipLabel", data.arg1()->GetString());
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
  SetupDefaultFirstPartySets(mock_privacy_sandbox_service());

  EXPECT_CALL(*mock_privacy_sandbox_service(), IsPartOfManagedFirstPartySet(_))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      IsPartOfManagedFirstPartySet(ConvertEtldToSchemefulSite("example.com")))
      .Times(2)
      .WillRepeatedly(Return(true));

  // Confirm that usage info only returns unpartitioned storage.
  SetupModels();

  EXPECT_EQ(28u,
            handler()->cookies_tree_model_->GetRoot()->GetTotalNodeCount());
  EXPECT_EQ(1, std::distance(handler()->browsing_data_model_->begin(),
                             handler()->browsing_data_model_->end()));

  base::Value::List args;
  args.Append("www.example.com");
  handler()->HandleFetchUsageTotal(args);
  handler()->ServicePendingRequests();
  ValidateUsageInfo("www.example.com", "2 B", "1 cookie",
                    "1 site in example.com's group", true);

  args.clear();
  args.Append("example.com");
  handler()->HandleFetchUsageTotal(args);
  handler()->ServicePendingRequests();
  ValidateUsageInfo("example.com", "", "1 cookie",
                    "1 site in example.com's group", true);

  args.clear();
  args.Append("google.com");
  handler()->HandleFetchUsageTotal(args);
  handler()->ServicePendingRequests();
  ValidateUsageInfo("google.com", "", "2 cookies",
                    "2 sites in google.com's group", false);
  args.clear();
  args.Append("ungrouped.com");
  handler()->HandleFetchUsageTotal(args);
  handler()->ServicePendingRequests();
  ValidateUsageInfo("ungrouped.com", "", "1 cookie", "", false);
}

TEST_F(SiteSettingsHandlerTest, NonTreeModelDeletion) {
  // Confirm that a BrowsingDataRemover task is started to remove Privacy
  // Sandbox APIs that are not integrated with the tree model.
  SetupModels();

  base::Value::List storage_and_cookie_list = GetOnStorageFetchedSentList();
  EXPECT_EQ(4U, storage_and_cookie_list.size());
  EXPECT_CALL(*mock_browsing_topics_service(),
              ClearTopicsDataForOrigin(
                  url::Origin::Create(GURL("https://www.google.com"))));
  EXPECT_CALL(*mock_browsing_topics_service(),
              ClearTopicsDataForOrigin(
                  url::Origin::Create(GURL("https://google.com"))));

  base::Value::List args;
  args.Append("google.com");
  handler()->HandleClearEtldPlus1DataAndCookies(args);

  auto* browsing_data_remover = profile()->GetBrowsingDataRemover();
  EXPECT_EQ(content::BrowsingDataRemover::DATA_TYPE_PRIVACY_SANDBOX &
                ~content::BrowsingDataRemover::DATA_TYPE_TRUST_TOKENS,
            browsing_data_remover->GetLastUsedRemovalMaskForTesting());
  EXPECT_EQ(base::Time::Min(),
            browsing_data_remover->GetLastUsedBeginTimeForTesting());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            browsing_data_remover->GetLastUsedOriginTypeMaskForTesting());
}

TEST_F(SiteSettingsHandlerTest, FirstPartySetsMembership) {
  SetupDefaultFirstPartySets(mock_privacy_sandbox_service());

  EXPECT_CALL(*mock_privacy_sandbox_service(), IsPartOfManagedFirstPartySet(_))
      .Times(2)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      IsPartOfManagedFirstPartySet(ConvertEtldToSchemefulSite("example.com")))
      .Times(1)
      .WillOnce(Return(true));

  SetupModels();

  handler()->ClearAllSitesMapForTesting();

  handler()->OnStorageFetched();
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data.function_name());

  ASSERT_TRUE(data.arg1()->is_string());
  EXPECT_EQ("onStorageListFetched", data.arg1()->GetString());

  ASSERT_TRUE(data.arg2()->is_list());
  const base::Value::List& storage_and_cookie_list = data.arg2()->GetList();
  EXPECT_EQ(4U, storage_and_cookie_list.size());

  auto first_party_sets = GetTestFirstPartySets();

  ValidateSitesWithFps(storage_and_cookie_list, first_party_sets);
}

TEST_F(SiteSettingsHandlerTest,
       HandleIgnoreOriginsForNotificationPermissionReview) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      ::features::kSafetyCheckNotificationPermissions);

  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile());
  ContentSettingsForOneType ignored_patterns;
  content_settings->GetSettingsForOneType(
      ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW, &ignored_patterns);
  ASSERT_EQ(0U, ignored_patterns.size());

  base::Value::List args;
  args.Append(GetOriginList(1));
  handler()->HandleIgnoreOriginsForNotificationPermissionReview(args);

  // Check there is 1 origin in ignore list.
  content_settings->GetSettingsForOneType(
      ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW, &ignored_patterns);
  ASSERT_EQ(1U, ignored_patterns.size());

  ValidateNotificationPermissionUpdate();
}

TEST_F(SiteSettingsHandlerTest, HandleBlockNotificationPermissionForOrigins) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      ::features::kSafetyCheckNotificationPermissions);

  base::Value::List args;
  base::Value::List origins = GetOriginList(2);
  args.Append(origins.Clone());

  handler()->HandleBlockNotificationPermissionForOrigins(args);

  // Check the permission for the two origins is block.
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile());
  ContentSettingsForOneType notification_permissions;
  content_settings->GetSettingsForOneType(ContentSettingsType::NOTIFICATIONS,
                                          &notification_permissions);
  auto type = content_settings->GetContentSetting(
      GURL(origins[0].GetString()), GURL(), ContentSettingsType::NOTIFICATIONS);
  ASSERT_EQ(CONTENT_SETTING_BLOCK, type);

  type = content_settings->GetContentSetting(
      GURL(origins[1].GetString()), GURL(), ContentSettingsType::NOTIFICATIONS);
  ASSERT_EQ(CONTENT_SETTING_BLOCK, type);

  ValidateNotificationPermissionUpdate();
}

TEST_F(SiteSettingsHandlerTest, HandleAllowNotificationPermissionForOrigins) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      ::features::kSafetyCheckNotificationPermissions);

  base::Value::List args;
  base::Value::List origins = GetOriginList(2);
  args.Append(origins.Clone());
  handler()->HandleAllowNotificationPermissionForOrigins(args);

  // Check the permission for the two origins is allow.
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile());
  ContentSettingsForOneType notification_permissions;
  content_settings->GetSettingsForOneType(ContentSettingsType::NOTIFICATIONS,
                                          &notification_permissions);
  auto type = content_settings->GetContentSetting(
      GURL(origins[0].GetString()), GURL(), ContentSettingsType::NOTIFICATIONS);
  ASSERT_EQ(CONTENT_SETTING_ALLOW, type);

  type = content_settings->GetContentSetting(
      GURL(origins[1].GetString()), GURL(), ContentSettingsType::NOTIFICATIONS);
  ASSERT_EQ(CONTENT_SETTING_ALLOW, type);

  ValidateNotificationPermissionUpdate();
}

TEST_F(SiteSettingsHandlerTest, HandleResetNotificationPermissionForOrigins) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      ::features::kSafetyCheckNotificationPermissions);

  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile());
  base::Value::List args;
  base::Value::List origins = GetOriginList(1);
  args.Append(origins.Clone());

  content_settings->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString(origins[0].GetString()),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
      CONTENT_SETTING_ALLOW);

  handler()->HandleResetNotificationPermissionForOrigins(args);

  // Check the permission for the origin is reset.
  auto type = content_settings->GetContentSetting(
      GURL(origins[0].GetString()), GURL(), ContentSettingsType::NOTIFICATIONS);
  ASSERT_EQ(CONTENT_SETTING_ASK, type);

  ValidateNotificationPermissionUpdate();
}

TEST_F(SiteSettingsHandlerTest, PopulateNotificationPermissionReviewData) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      ::features::kSafetyCheckNotificationPermissions);

  // Add a couple of notification permission and check they appear in review
  // list.
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  GURL urls[] = {GURL("https://google.com:443"),
                 GURL("https://www.youtube.com:443"),
                 GURL("https://www.example.com:443")};

  map->SetContentSettingDefaultScope(urls[0], GURL(),
                                     ContentSettingsType::NOTIFICATIONS,
                                     CONTENT_SETTING_ALLOW);
  map->SetContentSettingDefaultScope(urls[1], GURL(),
                                     ContentSettingsType::NOTIFICATIONS,
                                     CONTENT_SETTING_ALLOW);
  map->SetContentSettingDefaultScope(urls[2], GURL(),
                                     ContentSettingsType::NOTIFICATIONS,
                                     CONTENT_SETTING_ALLOW);

  // Record initial display date to enable comparing dictionaries for
  // NotificationEngagementService.
  auto* notification_engagement_service =
      NotificationsEngagementServiceFactory::GetForProfile(profile());
  std::string displayedDate =
      notification_engagement_service->GetBucketLabel(base::Time::Now());

  auto* site_engagement_service =
      site_engagement::SiteEngagementServiceFactory::GetForProfile(profile());

  // Set a host to have minimum engagement. This should be in review list.
  RecordNotification(notification_engagement_service, urls[0], 1);
  site_engagement::SiteEngagementScore score =
      site_engagement_service->CreateEngagementScore(urls[0]);
  score.Reset(0.5, GetReferenceTime());
  score.Commit();
  EXPECT_EQ(blink::mojom::EngagementLevel::MINIMAL,
            site_engagement_service->GetEngagementLevel(urls[0]));

  // Set a host to have large number of notifications, but low engagement. This
  // should be in review list.
  RecordNotification(notification_engagement_service, urls[1], 5);
  site_engagement_service->AddPointsForTesting(urls[1], 1.0);
  EXPECT_EQ(blink::mojom::EngagementLevel::LOW,
            site_engagement_service->GetEngagementLevel(urls[1]));

  // Set a host to have medium engagement and high notification count. This
  // should not be in review list.
  RecordNotification(notification_engagement_service, urls[2], 5);
  site_engagement_service->AddPointsForTesting(urls[2], 50.0);
  EXPECT_EQ(blink::mojom::EngagementLevel::MEDIUM,
            site_engagement_service->GetEngagementLevel(urls[2]));

  const auto& notification_permissions =
      handler()->PopulateNotificationPermissionReviewData();
  // Check if resulting list contains only the expected URLs. They should be in
  // descending order of notification count.
  EXPECT_EQ(2UL, notification_permissions.size());
  EXPECT_EQ("https://www.youtube.com:443",
            *notification_permissions[0].FindStringKey(site_settings::kOrigin));
  EXPECT_EQ("https://google.com:443",
            *notification_permissions[1].FindStringKey(site_settings::kOrigin));

  // Increasing notification count also promotes host in the list.
  RecordNotification(notification_engagement_service,
                     GURL("https://google.com:443"), 10);
  const auto& updated_notification_permissions =
      handler()->PopulateNotificationPermissionReviewData();
  EXPECT_EQ(2UL, updated_notification_permissions.size());
  EXPECT_EQ("https://google.com:443",
            *updated_notification_permissions[0].FindStringKey(
                site_settings::kOrigin));
  EXPECT_EQ("https://www.youtube.com:443",
            *updated_notification_permissions[1].FindStringKey(
                site_settings::kOrigin));
}

TEST_F(SiteSettingsHandlerTest,
       HandleUndoIgnoreOriginsForNotificationPermissionReview) {
  base::Value::List args;
  args.Append(GetOriginList(1));
  handler()->HandleIgnoreOriginsForNotificationPermissionReview(args);

  // Check there is 1 origin in ignore list.
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile());
  ContentSettingsForOneType ignored_patterns;
  ASSERT_EQ(0U, ignored_patterns.size());
  content_settings->GetSettingsForOneType(
      ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW, &ignored_patterns);
  ASSERT_EQ(1U, ignored_patterns.size());

  // Check there are no origins in ignore list.
  handler()->HandleUndoIgnoreOriginsForNotificationPermissionReview(args);
  content_settings->GetSettingsForOneType(
      ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW, &ignored_patterns);
  ASSERT_EQ(0U, ignored_patterns.size());
}

TEST_F(SiteSettingsHandlerTest,
       SendNotificationPermissionReviewList_FeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      ::features::kSafetyCheckNotificationPermissions);

  handler()->SendNotificationPermissionReviewList();

  ValidateNotificationPermissionUpdate();
}

TEST_F(SiteSettingsHandlerTest,
       SendNotificationPermissionReviewList_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndDisableFeature(
      ::features::kSafetyCheckNotificationPermissions);

  handler()->SendNotificationPermissionReviewList();

  ASSERT_EQ(0U, web_ui()->call_data().size());
}
}  // namespace settings
