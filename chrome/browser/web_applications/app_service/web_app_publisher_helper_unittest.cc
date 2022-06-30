// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/web_app_publisher_helper.h"

#include <initializer_list>
#include <memory>
#include <sstream>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/traits_bag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/shortcut.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_manager.h"
#endif

namespace ash {
class SystemWebAppManager;
}

namespace web_app {

namespace {

class NoOpWebAppPublisherDelegate : public WebAppPublisherHelper::Delegate {
  // WebAppPublisherHelper::Delegate:
  void PublishWebApps(std::vector<apps::AppPtr> apps) override{};
  void PublishWebApp(apps::AppPtr app) override{};
  void ModifyWebAppCapabilityAccess(
      const std::string& app_id,
      absl::optional<bool> accessing_camera,
      absl::optional<bool> accessing_microphone) override{};
};

std::string ToString(const apps::AppPtr& app) {
  // No string converter defined for App, so convert to AppUpdate.
  apps::AppUpdate app_update(app.get(), /*delta=*/nullptr, AccountId());
  std::stringstream ss;
  ss << app_update;
  return ss.str();
}

std::string ToString(const apps::mojom::AppPtr& app) {
  // No string converter defined for AppPtr, so convert to AppUpdate.
  apps::AppUpdate app_update(app.get(), /*delta=*/nullptr, AccountId());
  std::stringstream ss;
  ss << app_update;
  return ss.str();
}

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
    profile_ = std::make_unique<TestingProfile>();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    profile_->SetIsMainProfile(true);
#endif

    provider_ = WebAppProvider::GetForWebApps(profile());

    ash::SystemWebAppManager* swa_manager_ptr = nullptr;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    swa_manager_ = std::make_unique<ash::TestSystemWebAppManager>(profile());
    swa_manager_->ConnectSubsystems(provider_);
    swa_manager_ptr = swa_manager_.get();
#endif

    publisher_ = std::make_unique<WebAppPublisherHelper>(
        profile(), provider_,
        /*swa_manager=*/swa_manager_ptr, apps::AppType::kWeb, &no_op_delegate_,
        /*observe_media_requests=*/false);

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  Profile* profile() { return profile_.get(); }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  NoOpWebAppPublisherDelegate no_op_delegate_;
  WebAppProvider* provider_;
  std::unique_ptr<WebAppPublisherHelper> publisher_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<ash::TestSystemWebAppManager> swa_manager_;
#endif
};

TEST_F(WebAppPublisherHelperTest, CreateWebApp_Minimal) {
  const std::string name = "some app name";
  const GURL start_url("https://example.com/start_url");

  auto info = std::make_unique<WebAppInstallInfo>();
  info->title = base::UTF8ToUTF16(name);
  info->start_url = start_url;

  AppId app_id = test::InstallWebApp(profile(), std::move(info));
  const WebApp* web_app = provider_->registrar().GetAppById(app_id);
  apps::AppPtr app = publisher_->CreateWebApp(web_app);

  EXPECT_EQ(app->app_id, app_id);
  EXPECT_EQ(app->name, name);
  EXPECT_EQ(app->publisher_id, start_url.spec());

  // Ensure the legacy mojom converter produces an equivalent App.
  apps::mojom::AppPtr mojom_app = publisher_->ConvertWebApp(web_app);
  EXPECT_EQ(ToString(app), ToString(mojom_app));
}

TEST_F(WebAppPublisherHelperTest, CreateWebApp_Random) {
  for (int seed = 0; seed < 100; ++seed) {
    const GURL base_url("https://example.com/base_url");
    std::unique_ptr<WebApp> random_app =
        test::CreateRandomWebApp(base_url, seed);

    auto info = std::make_unique<WebAppInstallInfo>();
    info->title = base::UTF8ToUTF16(random_app->untranslated_name());
    info->description =
        base::UTF8ToUTF16(random_app->untranslated_description());
    info->start_url = random_app->start_url();
    info->manifest_id = random_app->manifest_id();
    info->file_handlers = random_app->file_handlers();

    // Unable to install a randomly generated web app struct, so just copy
    // necessary fields into the installation flow.
    AppId app_id = test::InstallWebApp(profile(), std::move(info));
    EXPECT_EQ(app_id, random_app->app_id());
    apps::AppPtr app = publisher_->CreateWebApp(random_app.get());

    EXPECT_EQ(app->app_id, random_app->app_id());
    EXPECT_EQ(app->name, random_app->untranslated_name());
    EXPECT_EQ(app->publisher_id, random_app->start_url().spec());

    // Ensure the legacy mojom converter produces an equivalent App.
    apps::mojom::AppPtr mojom_app = publisher_->ConvertWebApp(random_app.get());
    // Shortcuts aren't supported in the mojom struct, so make them consistent.
    app->shortcuts.clear();
    EXPECT_EQ(ToString(app), ToString(mojom_app));
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

  AppId app_id = test::InstallWebApp(profile(), std::move(info));
  const WebApp* web_app = provider_->registrar().GetAppById(app_id);
  apps::AppPtr app = publisher_->CreateWebApp(web_app);

  EXPECT_TRUE(HandlesIntent(app, apps_util::CreateCreateNoteIntent()));

  // Ensure the legacy mojom converter produces an equivalent App.
  apps::mojom::AppPtr mojom_app = publisher_->ConvertWebApp(web_app);
  EXPECT_EQ(ToString(app), ToString(mojom_app));
}

TEST_F(WebAppPublisherHelperTest, CreateWebApp_LockScreen_DisabledByFlag) {
  const std::string name = "some app name";
  const GURL start_url("https://example.com/start_url");
  const GURL lock_screen_url("https://example.com/lock_screen");

  auto info = std::make_unique<WebAppInstallInfo>();
  info->title = base::UTF8ToUTF16(name);
  info->start_url = start_url;
  info->lock_screen_start_url = lock_screen_url;

  AppId app_id = test::InstallWebApp(profile(), std::move(info));
  const WebApp* web_app = provider_->registrar().GetAppById(app_id);
  apps::AppPtr app = publisher_->CreateWebApp(web_app);

  EXPECT_FALSE(HandlesIntent(app, apps_util::CreateStartOnLockScreenIntent()));

  // Ensure the legacy mojom converter produces an equivalent App.
  apps::mojom::AppPtr mojom_app = publisher_->ConvertWebApp(web_app);
  EXPECT_EQ(ToString(app), ToString(mojom_app));
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

  AppId app_id = test::InstallWebApp(profile(), std::move(info));
  const WebApp* web_app = provider_->registrar().GetAppById(app_id);
  apps::AppPtr app = publisher_->CreateWebApp(web_app);

  EXPECT_TRUE(HandlesIntent(app, apps_util::CreateStartOnLockScreenIntent()));

  // Ensure the legacy mojom converter produces an equivalent App.
  apps::mojom::AppPtr mojom_app = publisher_->ConvertWebApp(web_app);
  EXPECT_EQ(ToString(app), ToString(mojom_app));
}

}  // namespace web_app
