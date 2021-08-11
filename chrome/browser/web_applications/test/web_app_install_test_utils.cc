// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/system_web_apps/test/test_system_web_app_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
#include "chrome/browser/web_applications/components/url_handler_manager.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#endif

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
      switches::kDisablePreinstalledApps);
  TestWebAppProvider* provider = TestWebAppProvider::Get(profile);
  DCHECK(provider);
  provider->SetRunSubsystemStartupTasks(true);
  // Use a TestSystemWebAppManager to skip system web apps being auto-installed
  // on |Start|.
  provider->SetSystemWebAppManager(
      std::make_unique<web_app::TestSystemWebAppManager>(profile));
  provider->Start();
  // Await registry ready.
  base::RunLoop run_loop;
  provider->on_registry_ready().Post(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

AppId InstallDummyWebApp(Profile* profile,
                         const std::string& app_name,
                         const GURL& start_url) {
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);
  WebApplicationInfo web_app_info;

  web_app_info.start_url = start_url;
  web_app_info.scope = start_url;
  web_app_info.title = base::UTF8ToUTF16(app_name);
  web_app_info.description = base::UTF8ToUTF16(app_name);
  web_app_info.open_as_window = true;

  WebAppInstallFinalizer::FinalizeOptions options;
  options.install_source = webapps::WebappInstallSource::EXTERNAL_DEFAULT;

  // In unit tests, we do not have Browser or WebContents instances.
  // Hence we use FinalizeInstall instead of InstallWebAppFromManifest
  // to install the web app.
  base::RunLoop run_loop;
  WebAppProvider::Get(profile)->install_finalizer().FinalizeInstall(
      web_app_info, options,
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(installed_app_id, app_id);
            EXPECT_EQ(code, InstallResultCode::kSuccessNewInstall);
            run_loop.Quit();
          }));
  run_loop.Run();

  return app_id;
}

AppId InstallWebApp(Profile* profile,
                    std::unique_ptr<WebApplicationInfo> web_app_info) {
  // The sync system requires that sync entity name is never empty.
  if (web_app_info->title.empty())
    web_app_info->title = u"WebApplicationInfo App Name";

  AppId app_id;
  base::RunLoop run_loop;
  auto* provider = WebAppProvider::Get(profile);
  DCHECK(provider);
  WaitUntilReady(provider);
  provider->install_manager().InstallWebAppFromInfo(
      std::move(web_app_info), ForInstallableSite::kYes,
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
            app_id = installed_app_id;
            run_loop.Quit();
          }));

  run_loop.Run();
  return app_id;
}

#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
AppId InstallWebAppWithUrlHandlers(
    Profile* profile,
    const GURL& start_url,
    const std::u16string& app_name,
    const std::vector<apps::UrlHandlerInfo>& url_handlers) {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  info->start_url = start_url;
  info->title = app_name;
  info->open_as_window = true;
  info->url_handlers = url_handlers;
  web_app::AppId app_id =
      web_app::test::InstallWebApp(profile, std::move(info));

  auto& url_handler_manager = WebAppProvider::Get(profile)
                                  ->os_integration_manager()
                                  .url_handler_manager_for_testing();

  base::RunLoop run_loop;
  url_handler_manager.RegisterUrlHandlers(
      app_id, base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        run_loop.Quit();
      }));
  run_loop.Run();
  return app_id;
}
#endif

void UninstallWebApp(Profile* profile, const AppId& app_id) {
  WebAppProvider* const provider = WebAppProvider::Get(profile);
  base::RunLoop run_loop;

  DCHECK(provider->install_finalizer().CanUserUninstallWebApp(app_id));
  provider->install_finalizer().UninstallWebApp(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      base::BindLambdaForTesting([&](bool uninstalled) {
        EXPECT_TRUE(uninstalled);
        run_loop.Quit();
      }));

  run_loop.Run();
}

}  // namespace test
}  // namespace web_app
