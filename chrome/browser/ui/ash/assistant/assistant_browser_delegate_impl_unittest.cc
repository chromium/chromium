// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_browser_delegate_impl.h"

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/web_applications/test/fake_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/services/assistant/public/shared/constants.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class AssistantBrowserDelegateImplTest : public ChromeAshTestBase {
 public:
  AssistantBrowserDelegateImplTest()
      : fake_user_manager_(std::make_unique<ash::FakeChromeUserManager>()) {
    set_start_session(false);
  }
  ~AssistantBrowserDelegateImplTest() override = default;

  void SetUp() override {
    ChromeAshTestBase::SetUp();

    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();
    auto account_id = AccountId::FromUserEmail(profile_->GetProfileUserName());
    auto* user = fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);

    SimulateUserLogin(account_id);

    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, profile_.get());

    web_app::FakeWebAppProvider::Get(profile_.get())
        ->SetDatabaseFactory(
            std::make_unique<web_app::FakeWebAppDatabaseFactory>());

    delegate_.emplace();
  }

 protected:
  Profile* profile() const { return profile_.get(); }

  web_app::FakeWebAppProvider& fake_provider() const {
    return *web_app::FakeWebAppProvider::Get(profile());
  }

  web_app::FakeWebAppDatabaseFactory& database_factory() const {
    return static_cast<web_app::FakeWebAppDatabaseFactory&>(
        fake_provider().GetDatabaseFactory());
  }

  std::optional<AssistantBrowserDelegateImpl> delegate_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<TestingProfile> profile_;

  base::test::TestFuture<apps::AppLaunchParams,
                         web_app::LaunchWebAppWindowSetting>
      app_launch_future_;
};

TEST_F(AssistantBrowserDelegateImplTest, NewEntryPointOpensApp) {
  // Install the new entry point app.
  const GURL start_url = GURL("https://example.com/path");
  const webapps::AppId app_id =
      web_app::GenerateAppId(/*manifest_id_path=*/std::nullopt, start_url);
  auto web_app = std::make_unique<web_app::WebApp>(app_id);
  web_app->SetStartUrl(start_url);
  web_app->SetName("test app");
  web_app->AddSource(web_app::WebAppManagement::kSystem);
  web_app->SetDisplayMode(web_app::DisplayMode::kStandalone);
  web_app->SetUserDisplayMode(web_app::mojom::UserDisplayMode::kStandalone);

  web_app::Registry registry;
  registry[app_id] = std::move(web_app);
  database_factory().WriteRegistry(registry);

  delegate_->OverrideEntryPointIdForTesting(app_id);

  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

  auto* provider = web_app::WebAppProvider::GetForWebApps(profile());
  static_cast<web_app::FakeWebAppUiManager*>(&provider->ui_manager())
      ->SetOnLaunchWebAppCallback(app_launch_future_.GetRepeatingCallback());

  // Try opening new entry point.
  delegate_->OpenNewEntryPoint();

  // New entry point should open.
  ASSERT_TRUE(app_launch_future_.Wait())
      << "New entry point should launch, but didn't.";
}

TEST_F(AssistantBrowserDelegateImplTest, NewEntryPointDoNotOpenIfNotInstalled) {
  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

  auto* provider = web_app::WebAppProvider::GetForWebApps(profile());
  static_cast<web_app::FakeWebAppUiManager*>(&provider->ui_manager())
      ->SetOnLaunchWebAppCallback(app_launch_future_.GetRepeatingCallback());

  // Try opening new entry point without installing app.
  delegate_->OpenNewEntryPoint();

  // Wait for things to settle to check if entry point is launched.
  task_environment()->RunUntilIdle();

  // New entry point should not open.
  ASSERT_FALSE(app_launch_future_.IsReady())
      << "New entry point should not launch, but somehow it did.";
}
