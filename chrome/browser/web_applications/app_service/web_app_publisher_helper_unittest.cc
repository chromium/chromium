// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/web_app_publisher_helper.h"

#include <initializer_list>
#include <memory>
#include <sstream>
#include <utility>

#include "ash/constants/web_app_id_constants.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/traits_bag.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

class NoOpWebAppPublisherDelegate : public WebAppPublisherHelper::Delegate {
  // WebAppPublisherHelper::Delegate:
  void PublishWebApps(std::vector<apps::AppPtr> apps) override {}
  void PublishWebApp(apps::AppPtr app) override {}
  void ModifyWebAppCapabilityAccess(
      const std::string& app_id,
      std::optional<bool> accessing_camera,
      std::optional<bool> accessing_microphone) override {}
};

bool HandlesIntent(const apps::AppPtr& app, const apps::IntentPtr& intent) {
  for (const auto& filter : app->intent_filters) {
    if (intent->MatchFilter(filter)) {
      return true;
    }
  }
  return false;
}

}  // namespace

class WebAppPublisherHelperTest : public testing::Test {
 public:
  WebAppPublisherHelperTest() = default;
  WebAppPublisherHelperTest(const WebAppPublisherHelperTest&) = delete;
  WebAppPublisherHelperTest& operator=(const WebAppPublisherHelperTest&) =
      delete;
  ~WebAppPublisherHelperTest() override = default;

  void SetUp() override {
    TestingProfile::Builder builder;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    builder.SetIsMainProfile(true);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    profile_ = builder.Build();

    provider_ = WebAppProvider::GetForWebApps(profile());
    apps::WaitForAppServiceProxyReady(
        apps::AppServiceProxyFactory::GetForProfile(profile()));

    publisher_ = std::make_unique<WebAppPublisherHelper>(profile(), provider_,
                                                         &no_op_delegate_);

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  Profile* profile() { return profile_.get(); }

  webapps::AppId CreateShortcut(const GURL& shortcut_url,
                                const std::string& shortcut_name) {
    return test::InstallShortcut(profile(), shortcut_name, shortcut_url);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  NoOpWebAppPublisherDelegate no_op_delegate_;
  raw_ptr<WebAppProvider> provider_ = nullptr;
  std::unique_ptr<WebAppPublisherHelper> publisher_;
};

TEST_F(WebAppPublisherHelperTest, CreateWebApp_Minimal) {
  const std::string name = "some app name";
  const GURL start_url("https://example.com/start_url");

  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->title = base::UTF8ToUTF16(name);

  webapps::AppId app_id = test::InstallWebApp(profile(), std::move(info));
  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  apps::AppPtr app = publisher_->CreateWebApp(web_app);

  EXPECT_EQ(app->app_id, app_id);
  EXPECT_EQ(app->name, name);
  EXPECT_EQ(app->publisher_id, start_url.spec());
}

TEST_F(WebAppPublisherHelperTest, CreateWebApp_Random) {
  for (uint32_t seed = 0; seed < 100; ++seed) {
    std::unique_ptr<WebApp> random_app =
        test::CreateRandomWebApp({.seed = seed});

    auto info = std::make_unique<WebAppInstallInfo>(random_app->manifest_id(),
                                                    random_app->start_url());
    info->title = base::UTF8ToUTF16(random_app->untranslated_name());
    info->description =
        base::UTF8ToUTF16(random_app->untranslated_description());
    info->file_handlers = random_app->file_handlers();

    // Unable to install a randomly generated web app struct, so just copy
    // necessary fields into the installation flow.
    webapps::AppId app_id = test::InstallWebApp(profile(), std::move(info));
    EXPECT_EQ(app_id, random_app->app_id());
    apps::AppPtr app = publisher_->CreateWebApp(random_app.get());

    EXPECT_EQ(app->app_id, random_app->app_id());
    EXPECT_EQ(app->name, random_app->untranslated_name());
    EXPECT_EQ(app->publisher_id, random_app->start_url().spec());
  }
}

// Verifies that the extended_scope matches the specified domain but not
// unrelated domains.
TEST_F(WebAppPublisherHelperTest, CreateWebApp_ScopeExtension) {
  const std::string name = "some app name";
  const GURL start_url("https://example.com/start_url");
  const GURL extended_scope_url("https://example.org/foo");
  const GURL outside_extended_scope_url("https://nonexample.org/foo");

  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->title = base::UTF8ToUTF16(name);
  info->validated_scope_extensions = {
      ScopeExtensionInfo{.origin = url::Origin::Create(extended_scope_url)}};

  webapps::AppId app_id = test::InstallWebApp(profile(), std::move(info));
  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  apps::AppPtr app = publisher_->CreateWebApp(web_app);

  EXPECT_TRUE(HandlesIntent(
      app, std::make_unique<apps::Intent>(apps_util::kIntentActionView,
                                          extended_scope_url)));
  EXPECT_FALSE(HandlesIntent(
      app, std::make_unique<apps::Intent>(apps_util::kIntentActionView,
                                          outside_extended_scope_url)));
}

// Verifies that the extended_scope with a registrable_domain wildcard matches
// the domain and its subdomains but not unrelated domains.
TEST_F(WebAppPublisherHelperTest, CreateWebApp_WildcardScopeExtension) {
  const std::string name = "some app name";
  const GURL start_url("https://example.com/start_url");
  const GURL extended_scope_url("https://example.org/foo");
  const GURL subdomain_extended_scope_url("https://sub.example.org/foo");
  const GURL outside_extended_scope_url("https://nonexample.org/foo");

  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->title = base::UTF8ToUTF16(name);
  info->validated_scope_extensions = {
      ScopeExtensionInfo{.origin = url::Origin::Create(extended_scope_url),
                         .has_origin_wildcard = true}};

  webapps::AppId app_id = test::InstallWebApp(profile(), std::move(info));
  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  apps::AppPtr app = publisher_->CreateWebApp(web_app);

  EXPECT_TRUE(HandlesIntent(
      app, std::make_unique<apps::Intent>(apps_util::kIntentActionView,
                                          extended_scope_url)));
  EXPECT_TRUE(HandlesIntent(
      app, std::make_unique<apps::Intent>(apps_util::kIntentActionView,
                                          subdomain_extended_scope_url)));
  EXPECT_FALSE(HandlesIntent(
      app, std::make_unique<apps::Intent>(apps_util::kIntentActionView,
                                          outside_extended_scope_url)));
}

TEST_F(WebAppPublisherHelperTest, CreateWebApp_NoteTaking) {
  const std::string name = "some app name";
  const GURL start_url("https://example.com/start_url");
  const GURL new_note_url("https://example.com/new_note");

  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->title = base::UTF8ToUTF16(name);
  info->note_taking_new_note_url = new_note_url;

  webapps::AppId app_id = test::InstallWebApp(profile(), std::move(info));
  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  apps::AppPtr app = publisher_->CreateWebApp(web_app);

  EXPECT_TRUE(HandlesIntent(app, apps_util::CreateCreateNoteIntent()));
}

TEST_F(WebAppPublisherHelperTest, CreateWebApp_LockScreen_DisabledByFlag) {
  const std::string name = "some app name";
  const GURL start_url("https://example.com/start_url");
  const GURL lock_screen_url("https://example.com/lock_screen");

  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->title = base::UTF8ToUTF16(name);
  info->lock_screen_start_url = lock_screen_url;

  webapps::AppId app_id = test::InstallWebApp(profile(), std::move(info));
  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  apps::AppPtr app = publisher_->CreateWebApp(web_app);

  EXPECT_FALSE(HandlesIntent(app, apps_util::CreateStartOnLockScreenIntent()));
}

TEST_F(WebAppPublisherHelperTest,
       CreateIntentFiltersForWebApp_WebApp_HasUrlFilter) {
  const WebApp* app = nullptr;
  {
    ScopedRegistryUpdate update = provider_->sync_bridge_unsafe().BeginUpdate();
    auto new_app = test::CreateWebApp();
    app = new_app.get();
    DCHECK(new_app->start_url().is_valid());
    new_app->SetScope(new_app->start_url().GetWithoutFilename());
    update->CreateApp(std::move(new_app));
  }

  apps::IntentFilters filters =
      WebAppPublisherHelper::CreateIntentFiltersForWebApp(*provider_, *app);

  ASSERT_EQ(filters.size(), 1u);
  apps::IntentFilterPtr& filter = filters[0];
  EXPECT_FALSE(filter->activity_name.has_value());
  EXPECT_FALSE(filter->activity_label.has_value());
  ASSERT_EQ(filter->conditions.size(), 4U);

  {
    const apps::Condition& condition = *filter->conditions[0];
    EXPECT_EQ(condition.condition_type, apps::ConditionType::kAction);
    ASSERT_EQ(condition.condition_values.size(), 1U);
    EXPECT_EQ(condition.condition_values[0]->match_type,
              apps::PatternMatchType::kLiteral);
    EXPECT_EQ(condition.condition_values[0]->value,
              apps_util::kIntentActionView);
  }

  {
    const apps::Condition& condition = *filter->conditions[1];
    EXPECT_EQ(condition.condition_type, apps::ConditionType::kScheme);
    ASSERT_EQ(condition.condition_values.size(), 1U);
    EXPECT_EQ(condition.condition_values[0]->match_type,
              apps::PatternMatchType::kLiteral);
    EXPECT_EQ(condition.condition_values[0]->value, app->scope().scheme());
  }

  {
    const apps::Condition& condition = *filter->conditions[2];
    EXPECT_EQ(condition.condition_type, apps::ConditionType::kAuthority);
    ASSERT_EQ(condition.condition_values.size(), 1U);
    EXPECT_EQ(condition.condition_values[0]->match_type,
              apps::PatternMatchType::kLiteral);
    EXPECT_EQ(condition.condition_values[0]->value,
              apps_util::AuthorityView::Encode(app->scope()));
  }

  {
    const apps::Condition& condition = *filter->conditions[3];
    EXPECT_EQ(condition.condition_type, apps::ConditionType::kPath);
    ASSERT_EQ(condition.condition_values.size(), 1U);
    EXPECT_EQ(condition.condition_values[0]->match_type,
              apps::PatternMatchType::kPrefix);
    EXPECT_EQ(condition.condition_values[0]->value, app->scope().path());
  }
}

// TODO(crbug.com/327431493): Use a more holistic approach than adding apps to
// the registry.
TEST_F(WebAppPublisherHelperTest, CreateIntentFiltersForWebApp_FileHandlers) {
  const WebApp* app = nullptr;
  {
    ScopedRegistryUpdate update = provider_->sync_bridge_unsafe().BeginUpdate();
    auto new_app = test::CreateWebApp();
    app = new_app.get();
    DCHECK(new_app->start_url().is_valid());
    new_app->SetScope(new_app->start_url().GetWithoutFilename());

    apps::FileHandler::AcceptEntry accept_entry;
    proto::WebAppOsIntegrationState test_state;

    accept_entry.mime_type = "text/plain";
    accept_entry.file_extensions.insert(".txt");
    apps::FileHandler file_handler;
    file_handler.action = GURL("https://example.com/path/handler.html");

    proto::FileHandling::FileHandler* file_handler_proto =
        test_state.mutable_file_handling()->add_file_handlers();
    file_handler_proto->set_action(file_handler.action.spec());
    auto* accept_entry_proto = file_handler_proto->add_accept();
    accept_entry_proto->set_mimetype(accept_entry.mime_type);
    accept_entry_proto->add_file_extensions(".txt");

    file_handler.accept.push_back(std::move(accept_entry));
    new_app->SetFileHandlers({std::move(file_handler)});
    new_app->SetCurrentOsIntegrationStates(test_state);

    update->CreateApp(std::move(new_app));
  }

  apps::IntentFilters filters =
      WebAppPublisherHelper::CreateIntentFiltersForWebApp(*provider_, *app);

  ASSERT_EQ(filters.size(), 2u);
  // 1st filter is URL filter.

  // File filter - View action
  const apps::IntentFilterPtr& file_filter = filters[1];
  ASSERT_EQ(file_filter->conditions.size(), 2u);
  const apps::Condition& view_cond = *file_filter->conditions[0];
  EXPECT_EQ(view_cond.condition_type, apps::ConditionType::kAction);
  ASSERT_EQ(view_cond.condition_values.size(), 1u);
  EXPECT_EQ(view_cond.condition_values[0]->value, apps_util::kIntentActionView);

  // File filter - mime & file extension match
  const apps::Condition& file_cond = *file_filter->conditions[1];
  EXPECT_EQ(file_cond.condition_type, apps::ConditionType::kFile);
  ASSERT_EQ(file_cond.condition_values.size(), 2u);
  EXPECT_EQ(file_cond.condition_values[0]->match_type,
            apps::PatternMatchType::kMimeType);
  EXPECT_EQ(file_cond.condition_values[0]->value, "text/plain");
  EXPECT_EQ(file_cond.condition_values[1]->match_type,
            apps::PatternMatchType::kFileExtension);
  EXPECT_EQ(file_cond.condition_values[1]->value, ".txt");
}

class WebAppPublisherHelperTest_WebLockScreenApi
    : public WebAppPublisherHelperTest {
  base::test::ScopedFeatureList features{features::kWebLockScreenApi};
};

TEST_F(WebAppPublisherHelperTest_WebLockScreenApi, CreateWebApp_LockScreen) {
  const std::string name = "some app name";
  const GURL start_url("https://example.com/start_url");
  const GURL lock_screen_url("https://example.com/lock_screen");

  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->title = base::UTF8ToUTF16(name);
  info->lock_screen_start_url = lock_screen_url;

  webapps::AppId app_id = test::InstallWebApp(profile(), std::move(info));
  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  apps::AppPtr app = publisher_->CreateWebApp(web_app);

  EXPECT_TRUE(HandlesIntent(app, apps_util::CreateStartOnLockScreenIntent()));
}

}  // namespace web_app
