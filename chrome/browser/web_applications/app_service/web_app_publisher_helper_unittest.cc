// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/web_app_publisher_helper.h"

#include <initializer_list>
#include <memory>
#include <sstream>
#include <utility>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/traits_bag.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/web_applications/app_service/web_apps_with_shortcuts_test.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/stub_notification_display_service.h"
#include "components/ukm/test_ukm_recorder.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/web_applications/app_service/test/loopback_crosapi_app_service_proxy.h"
#endif

namespace web_app {

namespace {

class NoOpWebAppPublisherDelegate : public WebAppPublisherHelper::Delegate {
 public:
  int num_publish_called() { return num_publish_called_; }

 private:
  // WebAppPublisherHelper::Delegate:
  void PublishWebApps(std::vector<apps::AppPtr> apps) override {}
  void PublishWebApp(apps::AppPtr app) override { num_publish_called_++; }
  void ModifyWebAppCapabilityAccess(
      const std::string& app_id,
      absl::optional<bool> accessing_camera,
      absl::optional<bool> accessing_microphone) override {}
  int num_publish_called_ = 0;
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

class WebAppPublisherHelperTest : public testing::Test,
                                  public WebAppsWithShortcutsTest {
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

    publisher_ = std::make_unique<WebAppPublisherHelper>(profile(), provider_,
                                                         &no_op_delegate_);

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  Profile* profile() { return profile_.get(); }

  webapps::AppId CreateShortcut(const GURL& shortcut_url,
                                const std::string& shortcut_name) {
    // Create a web app entry without scope, which would be recognised
    // as ShortcutApp in the web app system.
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->title = base::UTF8ToUTF16(shortcut_name);
    web_app_info->start_url = shortcut_url;

    webapps::AppId app_id =
        test::InstallWebApp(profile(), std::move(web_app_info));
    CHECK(
        WebAppProvider::GetForTest(profile())->registrar_unsafe().IsShortcutApp(
            app_id));
    return app_id;
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

  auto info = std::make_unique<WebAppInstallInfo>();
  info->title = base::UTF8ToUTF16(name);
  info->start_url = start_url;

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

    auto info = std::make_unique<WebAppInstallInfo>();
    info->title = base::UTF8ToUTF16(random_app->untranslated_name());
    info->description =
        base::UTF8ToUTF16(random_app->untranslated_description());
    info->start_url = random_app->start_url();
    info->manifest_id = random_app->manifest_id();
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

TEST_F(WebAppPublisherHelperTest, CreateWebApp_NoteTaking) {
  const std::string name = "some app name";
  const GURL start_url("https://example.com/start_url");
  const GURL new_note_url("https://example.com/new_note");

  auto info = std::make_unique<WebAppInstallInfo>();
  info->title = base::UTF8ToUTF16(name);
  info->start_url = start_url;
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

  auto info = std::make_unique<WebAppInstallInfo>();
  info->title = base::UTF8ToUTF16(name);
  info->start_url = start_url;
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

TEST_F(WebAppPublisherHelperTest, CreateIntentFiltersForWebApp_FileHandlers) {
  const WebApp* app = nullptr;
  {
    ScopedRegistryUpdate update = provider_->sync_bridge_unsafe().BeginUpdate();
    auto new_app = test::CreateWebApp();
    app = new_app.get();
    DCHECK(new_app->start_url().is_valid());
    new_app->SetScope(new_app->start_url().GetWithoutFilename());

    apps::FileHandler::AcceptEntry accept_entry;
    accept_entry.mime_type = "text/plain";
    accept_entry.file_extensions.insert(".txt");
    apps::FileHandler file_handler;
    file_handler.action = GURL("https://example.com/path/handler.html");
    file_handler.accept.push_back(std::move(accept_entry));
    new_app->SetFileHandlers({std::move(file_handler)});
    new_app->SetFileHandlerOsIntegrationState(OsIntegrationState::kEnabled);

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

// Verify that Adobe Express has its OEM install source overwritten as
// InstallReason::kDefault.
// TODO(b/300857328): Remove this workaround.
TEST_F(WebAppPublisherHelperTest, PublishOemAdobeExpressAsDefault) {
  const std::string name = "some app name";
  const GURL start_url("https://example.com/start_url");

  // Manually edit the database to create an app with the specific App ID but a
  // non-matching start URL.
  {
    ScopedRegistryUpdate update = provider_->sync_bridge_unsafe().BeginUpdate();

    auto new_app = std::make_unique<WebApp>(kAdobeExpressAppId);
    new_app->SetStartUrl(start_url);
    new_app->AddSource(WebAppManagement::Type::kOem);
    new_app->SetName(name);

    update->CreateApp(std::move(new_app));
  }

  const WebApp* web_app =
      provider_->registrar_unsafe().GetAppById(kAdobeExpressAppId);

  apps::AppPtr app = publisher_->CreateWebApp(web_app);
  EXPECT_EQ(app->install_reason, apps::InstallReason::kDefault);
}

// Verify that the above behavior only applies when the app is OEM-installed.
// TODO(b/300857328): Remove this workaround.
TEST_F(WebAppPublisherHelperTest, PublishSyncAdobeExpressAsSync) {
  const std::string name = "some app name";
  const GURL start_url("https://example.com/start_url");

  // Manually edit the database to create an app with the specific App ID but a
  // non-matching start URL.
  {
    ScopedRegistryUpdate update = provider_->sync_bridge_unsafe().BeginUpdate();

    auto new_app = std::make_unique<WebApp>(kAdobeExpressAppId);
    new_app->SetStartUrl(start_url);
    new_app->AddSource(WebAppManagement::Type::kSync);
    new_app->SetName(name);

    update->CreateApp(std::move(new_app));
  }

  const WebApp* web_app =
      provider_->registrar_unsafe().GetAppById(kAdobeExpressAppId);

  apps::AppPtr app = publisher_->CreateWebApp(web_app);
  EXPECT_EQ(app->install_reason, apps::InstallReason::kSync);
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(WebAppPublisherHelperTest, UpdateShortcutDoesNotPublishDelta) {
  EnableCrosWebAppShortcutUiUpdate(true);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  LoopbackCrosapiAppServiceProxy loopback(profile());
#endif

  EXPECT_EQ(0, no_op_delegate_.num_publish_called());
  GURL shortcut_url("https://example-shortcut.com/");
  auto shortcut_id = CreateShortcut(shortcut_url, "Shortcut");

  EXPECT_EQ(0, no_op_delegate_.num_publish_called());

  provider_->install_manager().NotifyWebAppInstalled(shortcut_id);
  EXPECT_EQ(0, no_op_delegate_.num_publish_called());

  provider_->install_manager().NotifyWebAppInstalledWithOsHooks(shortcut_id);
  EXPECT_EQ(0, no_op_delegate_.num_publish_called());

  provider_->install_manager().NotifyWebAppSourceRemoved(shortcut_id);
  EXPECT_EQ(0, no_op_delegate_.num_publish_called());

  provider_->install_manager().NotifyWebAppUninstalled(
      shortcut_id, webapps::WebappUninstallSource::kUnknown);
  EXPECT_EQ(0, no_op_delegate_.num_publish_called());

  provider_->install_manager().NotifyWebAppManifestUpdated(shortcut_id);
  EXPECT_EQ(0, no_op_delegate_.num_publish_called());

  provider_->registrar_unsafe().NotifyWebAppFileHandlerApprovalStateChanged(
      shortcut_id);
  EXPECT_EQ(0, no_op_delegate_.num_publish_called());

  provider_->registrar_unsafe().NotifyWebAppDisabledStateChanged(shortcut_id,
                                                                 false);
  EXPECT_EQ(0, no_op_delegate_.num_publish_called());

  // TODO(crbug.com/1412708): Test OnWebAppsDisabledModeChanged;

  provider_->registrar_unsafe().NotifyWebAppLastLaunchTimeChanged(shortcut_id,
                                                                  base::Time());
  EXPECT_EQ(0, no_op_delegate_.num_publish_called());

  provider_->registrar_unsafe().NotifyWebAppUserDisplayModeChanged(
      shortcut_id, mojom::UserDisplayMode::kBrowser);
  EXPECT_EQ(0, no_op_delegate_.num_publish_called());

  provider_->registrar_unsafe().NotifyWebAppRunOnOsLoginModeChanged(
      shortcut_id, RunOnOsLoginMode::kNotRun);
  EXPECT_EQ(0, no_op_delegate_.num_publish_called());

  provider_->registrar_unsafe().NotifyWebAppSettingsPolicyChanged();
  EXPECT_EQ(0, no_op_delegate_.num_publish_called());

  // Stub notification service doesn't notify display, use the real one for
  // testing.
  auto* notification_display_service_ =
      NotificationDisplayServiceFactory::GetForProfile(profile());
  const GURL origin = shortcut_url.DeprecatedGetOriginAsURL();
  const std::string notification_id = "notification-id";
  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      std::u16string(), std::u16string(), ui::ImageModel(),
      base::UTF8ToUTF16(origin.host()), origin,
      message_center::NotifierId(origin),
      message_center::RichNotificationData(), nullptr);
  auto metadata = std::make_unique<PersistentNotificationMetadata>();
  metadata->service_worker_scope = shortcut_url.GetWithoutFilename();
  notification_display_service_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, *notification,
      std::move(metadata));
  EXPECT_EQ(0, no_op_delegate_.num_publish_called());

  auto* stub_display_service = static_cast<StubNotificationDisplayService*>(
      NotificationDisplayServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(
                         &StubNotificationDisplayService::FactoryForTests)));
  stub_display_service->AddObserver(publisher_.get());
  stub_display_service->Display(NotificationHandler::Type::WEB_PERSISTENT,
                                *notification, nullptr);
  stub_display_service->ProcessNotificationOperation(
      NotificationOperation::kClose, NotificationHandler::Type::WEB_PERSISTENT,
      origin, notification_id, absl::nullopt, absl::nullopt, true);
  EXPECT_EQ(0, no_op_delegate_.num_publish_called());
  stub_display_service->RemoveObserver(publisher_.get());

  HostContentSettingsMap* content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  content_settings_map->SetContentSettingDefaultScope(
      shortcut_url, shortcut_url, ContentSettingsType::MEDIASTREAM_CAMERA,
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(0, no_op_delegate_.num_publish_called());

  ukm::TestUkmRecorder test_recorder;
  badging::BadgeManagerFactory::GetForProfile(profile())->SetBadgeForTesting(
      shortcut_id, 1, &test_recorder);
  EXPECT_EQ(0, no_op_delegate_.num_publish_called());
}
#endif

// For non ChromeOS platforms or when the kCrosWebAppShortcutUiUpdate is off,
// we still want to publish shortcuts as web app. This is checking old behaviour
// does not break.
TEST_F(WebAppPublisherHelperTest, UpdateShortcutDoesPublishDelta) {
  EnableCrosWebAppShortcutUiUpdate(false);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  LoopbackCrosapiAppServiceProxy loopback(profile());
#endif
  int expected_called_num = 0;
  EXPECT_EQ(expected_called_num, no_op_delegate_.num_publish_called());
  GURL shortcut_url("https://example-shortcut.com/");
  auto shortcut_id = CreateShortcut(shortcut_url, "Shortcut");
  EXPECT_EQ(++expected_called_num, no_op_delegate_.num_publish_called());

  provider_->install_manager().NotifyWebAppInstalled(shortcut_id);
  EXPECT_EQ(++expected_called_num, no_op_delegate_.num_publish_called());

  provider_->install_manager().NotifyWebAppInstalledWithOsHooks(shortcut_id);
  EXPECT_EQ(++expected_called_num, no_op_delegate_.num_publish_called());

  provider_->install_manager().NotifyWebAppUninstalled(
      shortcut_id, webapps::WebappUninstallSource::kUnknown);
  EXPECT_EQ(++expected_called_num, no_op_delegate_.num_publish_called());

  provider_->install_manager().NotifyWebAppManifestUpdated(shortcut_id);
  EXPECT_EQ(++expected_called_num, no_op_delegate_.num_publish_called());

  provider_->registrar_unsafe().NotifyWebAppFileHandlerApprovalStateChanged(
      shortcut_id);
  EXPECT_EQ(++expected_called_num, no_op_delegate_.num_publish_called());

#if BUILDFLAG(IS_CHROMEOS)
  provider_->registrar_unsafe().NotifyWebAppDisabledStateChanged(shortcut_id,
                                                                 false);
  EXPECT_EQ(++expected_called_num, no_op_delegate_.num_publish_called());
  // TODO(crbug.com/1412708): Test OnWebAppsDisabledModeChanged;
#endif  // BUILDFLAG(IS_CHROMEOS)

  provider_->registrar_unsafe().NotifyWebAppLastLaunchTimeChanged(shortcut_id,
                                                                  base::Time());
  EXPECT_EQ(++expected_called_num, no_op_delegate_.num_publish_called());

  provider_->registrar_unsafe().NotifyWebAppUserDisplayModeChanged(
      shortcut_id, mojom::UserDisplayMode::kBrowser);
  EXPECT_EQ(++expected_called_num, no_op_delegate_.num_publish_called());

  provider_->registrar_unsafe().NotifyWebAppRunOnOsLoginModeChanged(
      shortcut_id, RunOnOsLoginMode::kNotRun);
  EXPECT_EQ(++expected_called_num, no_op_delegate_.num_publish_called());

  provider_->registrar_unsafe().NotifyWebAppSettingsPolicyChanged();
  EXPECT_EQ(++expected_called_num, no_op_delegate_.num_publish_called());

#if BUILDFLAG(IS_CHROMEOS)
  // Stub notification service doesn't notify display, use the real one for
  // testing.
  auto* notification_display_service_ =
      NotificationDisplayServiceFactory::GetForProfile(profile());
  const GURL origin = shortcut_url.DeprecatedGetOriginAsURL();
  const std::string notification_id = "notification-id";
  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      std::u16string(), std::u16string(), ui::ImageModel(),
      base::UTF8ToUTF16(origin.host()), origin,
      message_center::NotifierId(origin),
      message_center::RichNotificationData(), nullptr);
  auto metadata = std::make_unique<PersistentNotificationMetadata>();
  metadata->service_worker_scope = shortcut_url.GetWithoutFilename();
  notification_display_service_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, *notification,
      std::move(metadata));
  EXPECT_EQ(++expected_called_num, no_op_delegate_.num_publish_called());

  auto* stub_display_service = static_cast<StubNotificationDisplayService*>(
      NotificationDisplayServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(
                         &StubNotificationDisplayService::FactoryForTests)));
  stub_display_service->AddObserver(publisher_.get());
  stub_display_service->Display(NotificationHandler::Type::WEB_PERSISTENT,
                                *notification, nullptr);
  stub_display_service->ProcessNotificationOperation(
      NotificationOperation::kClose, NotificationHandler::Type::WEB_PERSISTENT,
      origin, notification_id, absl::nullopt, absl::nullopt, true);
  EXPECT_EQ(++expected_called_num, no_op_delegate_.num_publish_called());
  stub_display_service->RemoveObserver(publisher_.get());
#endif  // BUILDFLAG(IS_CHROMEOS)

  HostContentSettingsMap* content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  content_settings_map->SetContentSettingDefaultScope(
      shortcut_url, shortcut_url, ContentSettingsType::MEDIASTREAM_CAMERA,
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(++expected_called_num, no_op_delegate_.num_publish_called());

#if BUILDFLAG(IS_CHROMEOS)
  ukm::TestUkmRecorder test_recorder;
  badging::BadgeManagerFactory::GetForProfile(profile())->SetBadgeForTesting(
      shortcut_id, 1, &test_recorder);
  EXPECT_EQ(++expected_called_num, no_op_delegate_.num_publish_called());
#endif  // BUILDFLAG(IS_CHROMEOS)
}

class WebAppPublisherHelperTest_WebLockScreenApi
    : public WebAppPublisherHelperTest {
  base::test::ScopedFeatureList features{features::kWebLockScreenApi};
};

TEST_F(WebAppPublisherHelperTest_WebLockScreenApi, CreateWebApp_LockScreen) {
  const std::string name = "some app name";
  const GURL start_url("https://example.com/start_url");
  const GURL lock_screen_url("https://example.com/lock_screen");

  auto info = std::make_unique<WebAppInstallInfo>();
  info->title = base::UTF8ToUTF16(name);
  info->start_url = start_url;
  info->lock_screen_start_url = lock_screen_url;

  webapps::AppId app_id = test::InstallWebApp(profile(), std::move(info));
  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  apps::AppPtr app = publisher_->CreateWebApp(web_app);

  EXPECT_TRUE(HandlesIntent(app, apps_util::CreateStartOnLockScreenIntent()));
}

}  // namespace web_app
