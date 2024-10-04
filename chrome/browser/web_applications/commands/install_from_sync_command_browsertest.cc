// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_from_sync_command.h"

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_observer_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {
namespace {

class InstallFromSyncCommandTest : public WebAppBrowserTestBase {
 public:
  InstallFromSyncCommandTest() = default;
  ~InstallFromSyncCommandTest() override = default;
};

IN_PROC_BROWSER_TEST_F(InstallFromSyncCommandTest, SimpleInstall) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  webapps::AppId id = GenerateAppId(std::nullopt, test_url);

  auto* provider = WebAppProvider::GetForTest(profile());
  base::RunLoop loop;
  InstallFromSyncCommand::Params params = InstallFromSyncCommand::Params(
      id, GenerateManifestIdFromStartUrlOnly(test_url), /*start_url=*/test_url,
      "Test Title",
      /*scope=*/https_server()->GetURL("/banners/"),
      /*theme_color=*/std::nullopt, mojom::UserDisplayMode::kStandalone,
      {apps::IconInfo(https_server()->GetURL("/banners/launcher-icon-2x.png"),
                      96)});
  provider->command_manager().ScheduleCommand(
      std::make_unique<InstallFromSyncCommand>(
          profile(), params,
          base::BindLambdaForTesting([&](const webapps::AppId& app_id,
                                         webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            EXPECT_EQ(app_id, id);
            loop.Quit();
          })));
  loop.Run();
  EXPECT_TRUE(provider->registrar_unsafe().IsInstalled(id));
  EXPECT_EQ(AreAppsLocallyInstalledBySync(),
            provider->registrar_unsafe().IsInstallState(
                id, {proto::INSTALLED_WITHOUT_OS_INTEGRATION,
                     proto::INSTALLED_WITH_OS_INTEGRATION}));

  SkColor icon_color =
      IconManagerReadAppIconPixel(provider->icon_manager(), id, 96);
  EXPECT_THAT(icon_color, testing::Eq(SkColorSetARGB(255, 0, 51, 102)));
}

IN_PROC_BROWSER_TEST_F(InstallFromSyncCommandTest, TwoInstalls) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  webapps::AppId id = GenerateAppId(std::nullopt, test_url);
  GURL other_test_url = https_server()->GetURL(
      "/banners/"
      "manifest_no_service_worker.html");
  webapps::AppId other_id = GenerateAppId(std::nullopt, test_url);

  auto* provider = WebAppProvider::GetForTest(profile());
  base::RunLoop loop;
  {
    InstallFromSyncCommand::Params params = InstallFromSyncCommand::Params(
        id, GenerateManifestIdFromStartUrlOnly(test_url),
        /*start_url=*/test_url, "Test Title",
        /*scope=*/https_server()->GetURL("/banners/"),
        /*theme_color=*/std::nullopt, mojom::UserDisplayMode::kStandalone,
        {apps::IconInfo(https_server()->GetURL("/banners/launcher-icon-2x.png"),
                        96)});
    provider->command_manager().ScheduleCommand(
        std::make_unique<InstallFromSyncCommand>(
            profile(), params,
            base::BindLambdaForTesting([&](const webapps::AppId& app_id,
                                           webapps::InstallResultCode code) {
              EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
              EXPECT_EQ(app_id, id);
            })));
  }
  {
    InstallFromSyncCommand::Params params = InstallFromSyncCommand::Params(
        other_id, GenerateManifestIdFromStartUrlOnly(other_test_url),
        /*start_url=*/other_test_url, "Test Title",
        /*scope=*/https_server()->GetURL("/banners/"),
        /*theme_color=*/std::nullopt, mojom::UserDisplayMode::kStandalone,
        {apps::IconInfo(https_server()->GetURL("/banners/launcher-icon-2x.png"),
                        96)});
    provider->command_manager().ScheduleCommand(
        std::make_unique<InstallFromSyncCommand>(
            profile(), params,
            base::BindLambdaForTesting([&](const webapps::AppId& app_id,
                                           webapps::InstallResultCode code) {
              EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
              EXPECT_EQ(app_id, other_id);
              loop.Quit();
            })));
  }
  loop.Run();
  // Check first install.
  {
    EXPECT_TRUE(provider->registrar_unsafe().IsInstalled(id));
    EXPECT_EQ(AreAppsLocallyInstalledBySync(),
              provider->registrar_unsafe().IsInstallState(
                  id, {proto::INSTALLED_WITHOUT_OS_INTEGRATION,
                       proto::INSTALLED_WITH_OS_INTEGRATION}));

    SkColor icon_color =
        IconManagerReadAppIconPixel(provider->icon_manager(), id, 96);
    EXPECT_THAT(icon_color, testing::Eq(SkColorSetARGB(255, 0, 51, 102)));
  }
  // Check second install.
  {
    EXPECT_TRUE(provider->registrar_unsafe().IsInstalled(other_id));
    EXPECT_EQ(AreAppsLocallyInstalledBySync(),
              provider->registrar_unsafe().IsInstallState(
                  other_id, {proto::INSTALLED_WITHOUT_OS_INTEGRATION,
                             proto::INSTALLED_WITH_OS_INTEGRATION}));

    SkColor icon_color =
        IconManagerReadAppIconPixel(provider->icon_manager(), other_id, 96);
    EXPECT_THAT(icon_color, testing::Eq(SkColorSetARGB(255, 0, 51, 102)));
  }
}

// Note: This test may become flaky if the posted task to shutdown the command
// manager happens after the installation is complete. If this is the case, then
// something must be added to stop the install from sync command from
// completing, perhaps by swapping out a system dependency like the
// WebContentsManager to have the load start but never report 'finished' to the
// command.
IN_PROC_BROWSER_TEST_F(InstallFromSyncCommandTest, AbortInstall) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  webapps::AppId id = GenerateAppId(std::nullopt, test_url);

  auto* provider = WebAppProvider::GetForTest(profile());
  InstallFromSyncCommand::Params params = InstallFromSyncCommand::Params(
      id, GenerateManifestIdFromStartUrlOnly(test_url), /*start_url=*/test_url,
      "Test Title",
      /*scope=*/https_server()->GetURL("/banners/"),
      /*theme_color=*/std::nullopt, mojom::UserDisplayMode::kStandalone, {});
  std::unique_ptr<InstallFromSyncCommand> command = std::make_unique<
      InstallFromSyncCommand>(
      profile(), params,
      base::BindLambdaForTesting([&](const webapps::AppId& app_id,
                                     webapps::InstallResultCode code) {
        EXPECT_EQ(
            code,
            webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown);
      }));
  provider->command_manager().ScheduleCommand(std::move(command));

  // Wait until the web contents is created, then listen for navigation and
  // destruction.
  content::WebContents* web_contents = nullptr;
  base::test::TestFuture<void> web_contents_created;
  provider->command_manager().SetOnWebContentsCreatedCallbackForTesting(
      web_contents_created.GetCallback());
  ASSERT_TRUE(web_contents_created.Wait());
  ASSERT_TRUE(provider->command_manager().web_contents_for_testing());
  web_contents = provider->command_manager().web_contents_for_testing();

  content::NavigationStartObserver navigation_started_observer(
      web_contents,
      base::BindLambdaForTesting([&](content::NavigationHandle* handle) {
        if (handle && handle->GetURL() == test_url) {
          // This must be posted as a task because web contents cannot be
          // destroyed inside of a observation method.
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(&WebAppProvider::Shutdown, provider->AsWeakPtr()));
        }
      }));
  content::WebContentsDestroyedWatcher web_contents_destroyed_observer(
      web_contents);
  web_contents_destroyed_observer.Wait();
  EXPECT_FALSE(provider->registrar_unsafe().IsInstalled(id));
}

}  // namespace
}  // namespace web_app
