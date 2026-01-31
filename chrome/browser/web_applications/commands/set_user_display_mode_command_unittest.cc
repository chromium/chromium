// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/set_user_display_mode_command.h"

#include "base/test/test_future.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

namespace {

class SetUserDisplayModeCommandTest : public WebAppTest {
 public:
  SetUserDisplayModeCommandTest() = default;
  ~SetUserDisplayModeCommandTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();

    base::ScopedAllowBlockingForTesting allow_blocking;
    test_override_ = OsIntegrationTestOverrideImpl::OverrideForTesting();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    test::UninstallAllWebApps(profile());
    test_override_.reset();
    WebAppTest::TearDown();
  }

  WebAppProvider* provider() { return FakeWebAppProvider::Get(profile()); }
  WebAppRegistrar& registrar() { return provider()->registrar_unsafe(); }

  webapps::AppId InstallAppWithoutOSIntegration(const GURL url) {
    std::unique_ptr<WebAppInstallInfo> info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(url);
    info->title = u"Test App";
    info->user_display_mode = mojom::UserDisplayMode::kStandalone;

    return test::InstallWebAppWithoutOsIntegration(
        profile(), std::move(info),
        /*overwrite_existing_manifest_fields=*/true,
        webapps::WebappInstallSource::EXTERNAL_DEFAULT);
  }

  void SetUserDisplayModeAndAwaitCompletion(
      const webapps::AppId& app_id,
      mojom::UserDisplayMode user_display_mode) {
    base::test::TestFuture<void> future;
    provider()->command_manager().ScheduleCommand(
        std::make_unique<SetUserDisplayModeCommand>(app_id, user_display_mode,
                                                    future.GetCallback()));
    EXPECT_TRUE(future.Wait());
  }

 private:
  std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
      test_override_;
};

TEST_F(SetUserDisplayModeCommandTest, SetUserDisplayMode) {
  const webapps::AppId app_id =
      InstallAppWithoutOSIntegration(GURL("https://example.com/"));

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  EXPECT_FALSE(state->has_shortcut());

  SetUserDisplayModeAndAwaitCompletion(app_id,
                                       mojom::UserDisplayMode::kBrowser);
  state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  EXPECT_FALSE(state->has_shortcut());
  EXPECT_EQ(mojom::UserDisplayMode::kBrowser,
            registrar().GetAppUserDisplayMode(app_id));

  SetUserDisplayModeAndAwaitCompletion(app_id,
                                       mojom::UserDisplayMode::kStandalone);
  state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(state->has_shortcut());
  EXPECT_EQ(mojom::UserDisplayMode::kStandalone,
            registrar().GetAppUserDisplayMode(app_id));
}

TEST_F(SetUserDisplayModeCommandTest, NoDisplayModeSetForMigratingApps) {
  auto install_info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/"));
  install_info->title = u"Test App";
  install_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;

  web_app::proto::WebAppMigrationSource source;
  source.set_manifest_id("https://migration.example.com/start.html");
  install_info->migration_sources.push_back(std::move(source));

  WebAppInstallParams params;
  params.install_state = proto::InstallState::SUGGESTED_FROM_MIGRATION;
  params.add_to_applications_menu = false;
  params.add_to_desktop = false;
  params.add_to_quick_launch_bar = false;

  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      result;
  provider()->scheduler().InstallFromInfoWithParams(
      std::move(install_info), /*overwrite_existing_manifest_fields=*/true,
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, result.GetCallback(),
      params);
  ASSERT_TRUE(result.Wait());
  EXPECT_EQ(result.Get<webapps::InstallResultCode>(),
            webapps::InstallResultCode::kSuccessNewInstall);
  const webapps::AppId& app_id = result.Get<webapps::AppId>();

  // User display mode will not be set.
  EXPECT_EQ(mojom::UserDisplayMode::kBrowser,
            registrar().GetAppUserDisplayMode(app_id));
  SetUserDisplayModeAndAwaitCompletion(app_id,
                                       mojom::UserDisplayMode::kStandalone);
  EXPECT_EQ(mojom::UserDisplayMode::kBrowser,
            registrar().GetAppUserDisplayMode(app_id));
}

}  // namespace
}  // namespace web_app
