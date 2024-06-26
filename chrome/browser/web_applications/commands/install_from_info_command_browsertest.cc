// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_from_info_command.h"

#include <map>
#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

using base::BucketsAre;

namespace web_app {

class InstallFromInfoCommandTest : public WebAppBrowserTestBase {
 public:
  InstallFromInfoCommandTest() = default;
  std::map<SquareSizePx, SkBitmap> ReadIcons(const webapps::AppId& app_id,
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
  base::HistogramTester tester;
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("http://test.com/path"));
  info->title = u"Test name";

  const webapps::WebappInstallSource install_source =
#if BUILDFLAG(IS_CHROMEOS_ASH)
      webapps::WebappInstallSource::SYSTEM_DEFAULT;
#else
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON;
#endif

  base::RunLoop loop;
  webapps::AppId result_app_id;
  provider().scheduler().InstallFromInfoWithParams(
      std::move(info),
      /*overwrite_existing_manifest_fields=*/false, install_source,
      base::BindLambdaForTesting(
          [&](const webapps::AppId& app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            result_app_id = app_id;
            loop.Quit();
          }),
      WebAppInstallParams());
  loop.Run();

  EXPECT_TRUE(provider().registrar_unsafe().IsActivelyInstalled(result_app_id));

  // Ensure histogram is only measured once.

  EXPECT_THAT(tester.GetAllSamples("WebApp.Install.Result"),
              BucketsAre(base::Bucket(true, 1)));
  EXPECT_THAT(tester.GetAllSamples("WebApp.Install.Source.Success"),
              BucketsAre(base::Bucket(install_source, 1)));

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
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("http://test.com/path"));
  info->title = u"Test name";

  WebAppInstallParams install_params;
  install_params.add_to_applications_menu = true;
  install_params.add_to_desktop = true;

  base::RunLoop loop;
  webapps::AppId result_app_id;
  provider().scheduler().InstallFromInfoWithParams(
      std::move(info),
      /*overwrite_existing_manifest_fields=*/false,
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      base::BindLambdaForTesting(
          [&](const webapps::AppId& app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            EXPECT_TRUE(
                provider().registrar_unsafe().IsActivelyInstalled(app_id));
            result_app_id = app_id;
            loop.Quit();
          }),
      install_params);
  loop.Run();
  std::optional<proto::WebAppOsIntegrationState> os_state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
          result_app_id);
  ASSERT_TRUE(os_state.has_value());
  EXPECT_TRUE(os_state->has_shortcut());
  EXPECT_EQ(os_state->run_on_os_login().run_on_os_login_mode(),
            proto::RunOnOsLoginMode::NOT_RUN);
}

}  // namespace web_app
