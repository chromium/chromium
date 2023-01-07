// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/commands/install_from_info_command.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {
namespace test {

void WaitUntilReady(WebAppProvider* provider) {
  if (provider->on_registry_ready().is_signaled())
    return;

  base::RunLoop run_loop;
  provider->on_registry_ready().Post(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

void AwaitStartWebAppProviderAndSubsystems(Profile* profile) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableDefaultApps);
  FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile);
  DCHECK(provider);
  provider->StartWithSubsystems();
  WaitUntilReady(provider);
}

AppId InstallDummyWebApp(Profile* profile,
                         const std::string& app_name,
                         const GURL& start_url,
                         const webapps::WebappInstallSource install_source) {
  auto web_app_info = std::make_unique<WebAppInstallInfo>();

  web_app_info->start_url = start_url;
  web_app_info->scope = start_url;
  web_app_info->title = base::UTF8ToUTF16(app_name);
  web_app_info->description = base::UTF8ToUTF16(app_name);
  web_app_info->user_display_mode = UserDisplayMode::kStandalone;
  web_app_info->install_url = start_url;

  return InstallWebApp(profile, std::move(web_app_info),
                       /*overwrite_existing_manifest_fields=*/true,
                       install_source);
}

AppId InstallWebApp(Profile* profile,
                    std::unique_ptr<WebAppInstallInfo> web_app_info,
                    bool overwrite_existing_manifest_fields,
                    webapps::WebappInstallSource install_source) {
  // The sync system requires that sync entity name is never empty.
  if (web_app_info->title.empty())
    web_app_info->title = u"WebAppInstallInfo App Name";

  AppId app_id;
  base::RunLoop run_loop;
  auto* provider = WebAppProvider::GetForTest(profile);
  DCHECK(provider);
  WaitUntilReady(provider);
  // In unit tests, we do not have Browser or WebContents instances. Hence we
  // use `InstallFromInfoCommand` instead of `FetchManifestAndInstallCommand` or
  // `WebAppInstallCommand` to install the web app.
  provider->command_manager().ScheduleCommand(
      std::make_unique<InstallFromInfoCommand>(
          std::move(web_app_info), &provider->install_finalizer(),
          overwrite_existing_manifest_fields, install_source,
          base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                         webapps::InstallResultCode code) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
            app_id = installed_app_id;
            run_loop.Quit();
          })));

  run_loop.Run();
  // Allow updates to be published to App Service listeners.
  base::RunLoop().RunUntilIdle();
  return app_id;
}

void UninstallWebApp(Profile* profile, const AppId& app_id) {
  WebAppProvider* const provider = WebAppProvider::GetForTest(profile);
  base::RunLoop run_loop;

  DCHECK(provider->install_finalizer().CanUserUninstallWebApp(app_id));
  provider->install_finalizer().UninstallWebApp(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
        EXPECT_EQ(code, webapps::UninstallResultCode::kSuccess);
        run_loop.Quit();
      }));

  run_loop.Run();
  // Allow updates to be published to App Service listeners.
  base::RunLoop().RunUntilIdle();
}

}  // namespace test
}  // namespace web_app
