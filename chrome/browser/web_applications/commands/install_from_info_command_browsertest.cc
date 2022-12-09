// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/test/browser_test.h"
#include "install_from_info_command.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {

class InstallFromInfoCommandTest : public WebAppControllerBrowserTest {
 public:
  InstallFromInfoCommandTest() {
    WebAppProvider::SetOsIntegrationManagerFactoryForTesting(
        [](Profile* profile) -> std::unique_ptr<OsIntegrationManager> {
          return std::make_unique<FakeOsIntegrationManager>(
              profile, nullptr, nullptr, nullptr, nullptr);
        });
  }

  std::map<SquareSizePx, SkBitmap> ReadIcons(const AppId& app_id,
                                             IconPurpose purpose,
                                             const SortedSizesPx& sizes_px) {
    std::map<SquareSizePx, SkBitmap> result;
    base::RunLoop run_loop;
    provider().icon_manager().ReadIcons(
        app_id, purpose, sizes_px,
        base::BindLambdaForTesting(
            [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
              result = std::move(icon_bitmaps);
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }
};

IN_PROC_BROWSER_TEST_F(InstallFromInfoCommandTest, SuccessInstall) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->title = u"Test name";
  info->start_url = GURL("http://test.com/path");

  const webapps::WebappInstallSource install_source =
#if BUILDFLAG(IS_CHROMEOS_ASH)
      webapps::WebappInstallSource::SYSTEM_DEFAULT;
#else
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON;
#endif

  base::RunLoop loop;
  AppId result_app_id;
  provider().scheduler().InstallFromInfo(
      std::move(info),
      /*overwrite_existing_manifest_fields=*/false, install_source,
      base::BindLambdaForTesting(
          [&](const AppId& app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            result_app_id = app_id;
            loop.Quit();
          }));
  loop.Run();

  EXPECT_TRUE(provider().registrar_unsafe().IsActivelyInstalled(result_app_id));
  EXPECT_EQ(provider()
                .os_integration_manager()
                .AsTestOsIntegrationManager()
                ->num_create_shortcuts_calls(),
            0u);

  const WebApp* web_app =
      provider().registrar_unsafe().GetAppById(result_app_id);
  ASSERT_TRUE(web_app);

  std::map<SquareSizePx, SkBitmap> icon_bitmaps =
      ReadIcons(result_app_id, IconPurpose::ANY,
                web_app->downloaded_icon_sizes(IconPurpose::ANY));

  // Make sure that icons have been generated for all sub sizes.
  EXPECT_TRUE(ContainsOneIconOfEachSize(icon_bitmaps));
}

IN_PROC_BROWSER_TEST_F(InstallFromInfoCommandTest, InstallWithParams) {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->title = u"Test name";
  info->start_url = GURL("http://test.com/path");

  WebAppInstallParams install_params;
  install_params.bypass_os_hooks = false;
  install_params.add_to_applications_menu = true;
  install_params.add_to_desktop = true;

  base::RunLoop loop;
  provider().scheduler().InstallFromInfoWithParams(
      std::move(info),
      /*overwrite_existing_manifest_fields=*/false,
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      base::BindLambdaForTesting(
          [&](const AppId& app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            EXPECT_TRUE(
                provider().registrar_unsafe().IsActivelyInstalled(app_id));
            loop.Quit();
          }),
      install_params);
  loop.Run();
  EXPECT_EQ(provider()
                .os_integration_manager()
                .AsTestOsIntegrationManager()
                ->num_create_shortcuts_calls(),
            1u);
}

}  // namespace web_app
