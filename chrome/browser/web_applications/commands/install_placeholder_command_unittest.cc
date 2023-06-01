// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_placeholder_command.h"

#include <memory>

#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/mock_data_retriever.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {
namespace {

class InstallPlaceholderCommandTest : public WebAppTest {
 public:
  const int kIconSize = 96;
  const GURL kInstallUrl = GURL("https://example.com");

  void SetUp() override {
    WebAppTest::SetUp();
    auto shortcut_manager = std::make_unique<TestShortcutManager>(profile());
    shortcut_manager_ = shortcut_manager.get();
    FakeWebAppProvider::Get(profile())
        ->GetOsIntegrationManager()
        .AsTestOsIntegrationManager()
        ->SetShortcutManager(std::move(shortcut_manager));

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }
  FakeOsIntegrationManager& fake_os_integration_manager() {
    return static_cast<FakeOsIntegrationManager&>(
        provider()->os_integration_manager());
  }
  TestShortcutManager* shortcut_manager() { return shortcut_manager_; }

 private:
  raw_ptr<TestShortcutManager, DanglingUntriaged> shortcut_manager_;
};

TEST_F(InstallPlaceholderCommandTest, InstallPlaceholder) {
  ExternalInstallOptions options(kInstallUrl, mojom::UserDisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  base::test::TestFuture<const AppId&, webapps::InstallResultCode, bool> future;
  provider()->scheduler().InstallPlaceholder(options, future.GetCallback(),
                                             web_contents()->GetWeakPtr());
  EXPECT_EQ(future.Get<webapps::InstallResultCode>(),
            webapps::InstallResultCode::kSuccessNewInstall);
  const AppId app_id = future.Get<AppId>();
  EXPECT_TRUE(provider()->registrar_unsafe().IsPlaceholderApp(
      app_id, WebAppManagement::kPolicy));
  EXPECT_EQ(fake_os_integration_manager().num_create_shortcuts_calls(), 1u);
  auto last_install_options =
      fake_os_integration_manager().get_last_install_options();
  EXPECT_TRUE(last_install_options->add_to_desktop);
  EXPECT_TRUE(last_install_options->add_to_quick_launch_bar);
  EXPECT_FALSE(last_install_options->os_hooks[OsHookType::kRunOnOsLogin]);
  if (AreOsIntegrationSubManagersEnabled()) {
    absl::optional<proto::WebAppOsIntegrationState> os_state =
        provider()->registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
    ASSERT_TRUE(os_state.has_value());
    EXPECT_TRUE(os_state->has_shortcut());
    EXPECT_EQ(os_state->run_on_os_login().run_on_os_login_mode(),
              proto::RunOnOsLoginMode::NOT_RUN);
  }
}

TEST_F(InstallPlaceholderCommandTest, InstallPlaceholderWithOverrideIconUrl) {
  ExternalInstallOptions options(kInstallUrl, mojom::UserDisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  const GURL icon_url("https://example.com/test.png");
  options.override_icon_url = icon_url;
  base::test::TestFuture<const AppId&, webapps::InstallResultCode, bool> future;

  auto data_retriever =
      std::make_unique<testing::StrictMock<MockDataRetriever>>();

  bool skip_page_favicons = true;
  SkBitmap bitmap;
  std::vector<gfx::Size> icon_sizes(1, gfx::Size(kIconSize, kIconSize));
  bitmap.allocN32Pixels(kIconSize, kIconSize);
  bitmap.eraseColor(SK_ColorRED);
  IconsMap icons = {{icon_url, {bitmap}}};
  DownloadedIconsHttpResults http_result = {
      {icon_url, net::HttpStatusCode::HTTP_OK}};
  EXPECT_CALL(*data_retriever,
              GetIcons(testing::_, testing::ElementsAre(icon_url),
                       skip_page_favicons, base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<3>(
          IconsDownloadedResult::kCompleted, std::move(icons), http_result));

  auto command = std::make_unique<InstallPlaceholderCommand>(
      profile(), options, future.GetCallback(), web_contents()->GetWeakPtr(),
      std::move(data_retriever));
  provider()->command_manager().ScheduleCommand(std::move(command));

  EXPECT_EQ(future.Get<webapps::InstallResultCode>(),
            webapps::InstallResultCode::kSuccessNewInstall);
  const AppId app_id = future.Get<AppId>();
  EXPECT_TRUE(provider()->registrar_unsafe().IsPlaceholderApp(
      app_id, WebAppManagement::kPolicy));
  EXPECT_EQ(fake_os_integration_manager().num_create_shortcuts_calls(), 1u);
  if (AreOsIntegrationSubManagersEnabled()) {
    absl::optional<proto::WebAppOsIntegrationState> os_state =
        provider()->registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
    ASSERT_TRUE(os_state.has_value());
    EXPECT_TRUE(os_state->has_shortcut());
  }
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(InstallPlaceholderCommandTest,
       InstallPlaceholderWithUninstallAndReplace) {
  GURL old_app_url("http://old-app.com");
  const AppId old_app =
      test::InstallDummyWebApp(profile(), "old_app", old_app_url);
  auto shortcut_info = std::make_unique<ShortcutInfo>();
  shortcut_info->url = old_app_url;
  shortcut_manager()->SetShortcutInfoForApp(old_app, std::move(shortcut_info));
  ShortcutLocations shortcut_locations;
  shortcut_locations.on_desktop = false;
  shortcut_locations.in_quick_launch_bar = true;
  shortcut_locations.in_startup = true;
  shortcut_manager()->SetAppExistingShortcuts(old_app_url, shortcut_locations);

  ExternalInstallOptions options(kInstallUrl, mojom::UserDisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  options.uninstall_and_replace = {old_app};

  base::test::TestFuture<const AppId&, webapps::InstallResultCode, bool> future;
  provider()->scheduler().InstallPlaceholder(
      options, future.GetCallback(), web_contents()->GetWeakPtr()

  );
  EXPECT_EQ(future.Get<webapps::InstallResultCode>(),
            webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(future.Get<bool>());
  const AppId app_id = future.Get<AppId>();
  EXPECT_TRUE(provider()->registrar_unsafe().IsPlaceholderApp(
      app_id, WebAppManagement::kPolicy));

  auto last_install_options =
      fake_os_integration_manager().get_last_install_options();
  EXPECT_FALSE(last_install_options->add_to_desktop);
  EXPECT_TRUE(last_install_options->add_to_quick_launch_bar);
  EXPECT_TRUE(last_install_options->os_hooks[OsHookType::kRunOnOsLogin]);
  if (AreOsIntegrationSubManagersEnabled()) {
    absl::optional<proto::WebAppOsIntegrationState> os_state =
        provider()->registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
    ASSERT_TRUE(os_state.has_value());
    EXPECT_TRUE(os_state->has_shortcut());
    EXPECT_TRUE(os_state->has_run_on_os_login());
    EXPECT_EQ(os_state->run_on_os_login().run_on_os_login_mode(),
              proto::RunOnOsLoginMode::WINDOWED);
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace
}  // namespace web_app
