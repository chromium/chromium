// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_from_sync_command.h"

#include "base/test/bind.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {
namespace {

class InstallFromSyncCommandTest : public WebAppControllerBrowserTest {
 public:
  InstallFromSyncCommandTest() = default;
  ~InstallFromSyncCommandTest() override = default;
};

IN_PROC_BROWSER_TEST_F(InstallFromSyncCommandTest, SimpleInstall) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  AppId id = GenerateAppId(absl::nullopt, test_url);

  auto url_loader = std::make_unique<WebAppUrlLoader>();
  auto* provider = WebAppProvider::GetForTest(profile());
  base::RunLoop loop;
  InstallFromSyncCommand::Params params = InstallFromSyncCommand::Params(
      id, GenerateManifestIdFromStartUrlOnly(test_url), /*start_url=*/test_url,
      "Test Title",
      /*scope=*/https_server()->GetURL("/banners/"),
      /*theme_color=*/absl::nullopt, mojom::UserDisplayMode::kStandalone,
      {apps::IconInfo(https_server()->GetURL("/banners/launcher-icon-2x.png"),
                      96)});
  provider->command_manager().ScheduleCommand(
      std::make_unique<InstallFromSyncCommand>(
          url_loader.get(), profile(), std::make_unique<WebAppDataRetriever>(),
          params,
          base::BindLambdaForTesting(
              [&](const AppId& app_id, webapps::InstallResultCode code) {
                EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
                EXPECT_EQ(app_id, id);
                loop.Quit();
              })));
  loop.Run();
  EXPECT_TRUE(provider->registrar_unsafe().IsInstalled(id));
  EXPECT_EQ(AreAppsLocallyInstalledBySync(),
            provider->registrar_unsafe().IsLocallyInstalled(id));

  SkColor icon_color =
      IconManagerReadAppIconPixel(provider->icon_manager(), id, 96);
  EXPECT_THAT(icon_color, testing::Eq(SkColorSetARGB(255, 0, 51, 102)));
}

IN_PROC_BROWSER_TEST_F(InstallFromSyncCommandTest, TwoInstalls) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  AppId id = GenerateAppId(absl::nullopt, test_url);
  GURL other_test_url = https_server()->GetURL(
      "/banners/"
      "manifest_no_service_worker.html");
  AppId other_id = GenerateAppId(absl::nullopt, test_url);

  auto url_loader = std::make_unique<WebAppUrlLoader>();
  auto* provider = WebAppProvider::GetForTest(profile());
  base::RunLoop loop;
  {
    InstallFromSyncCommand::Params params = InstallFromSyncCommand::Params(
        id, GenerateManifestIdFromStartUrlOnly(test_url),
        /*start_url=*/test_url, "Test Title",
        /*scope=*/https_server()->GetURL("/banners/"),
        /*theme_color=*/absl::nullopt, mojom::UserDisplayMode::kStandalone,
        {apps::IconInfo(https_server()->GetURL("/banners/launcher-icon-2x.png"),
                        96)});
    provider->command_manager().ScheduleCommand(
        std::make_unique<InstallFromSyncCommand>(
            url_loader.get(), profile(),
            std::make_unique<WebAppDataRetriever>(), params,
            base::BindLambdaForTesting([&](const AppId& app_id,
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
        /*theme_color=*/absl::nullopt, mojom::UserDisplayMode::kStandalone,
        {apps::IconInfo(https_server()->GetURL("/banners/launcher-icon-2x.png"),
                        96)});
    provider->command_manager().ScheduleCommand(
        std::make_unique<InstallFromSyncCommand>(
            url_loader.get(), profile(),
            std::make_unique<WebAppDataRetriever>(), params,
            base::BindLambdaForTesting([&](const AppId& app_id,
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
              provider->registrar_unsafe().IsLocallyInstalled(id));

    SkColor icon_color =
        IconManagerReadAppIconPixel(provider->icon_manager(), id, 96);
    EXPECT_THAT(icon_color, testing::Eq(SkColorSetARGB(255, 0, 51, 102)));
  }
  // Check second install.
  {
    EXPECT_TRUE(provider->registrar_unsafe().IsInstalled(other_id));
    EXPECT_EQ(AreAppsLocallyInstalledBySync(),
              provider->registrar_unsafe().IsLocallyInstalled(other_id));

    SkColor icon_color =
        IconManagerReadAppIconPixel(provider->icon_manager(), other_id, 96);
    EXPECT_THAT(icon_color, testing::Eq(SkColorSetARGB(255, 0, 51, 102)));
  }
}

}  // namespace
}  // namespace web_app
