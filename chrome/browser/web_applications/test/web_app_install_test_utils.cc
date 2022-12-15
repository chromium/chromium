// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"

#include "base/command_line.h"
#include "base/containers/enum_set.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sources.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {
namespace test {
namespace {
using WebAppSourcesSet = base::EnumSet<WebAppManagement::Type,
                                       WebAppManagement::kMinValue,
                                       WebAppManagement::kMaxValue>;
}

void WaitUntilReady(WebAppProvider* provider) {
  if (provider->on_registry_ready().is_signaled())
    return;

  base::RunLoop run_loop;
  provider->on_registry_ready().Post(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

void WaitUntilWebAppProviderAndSubsystemsReady(WebAppProvider* provider) {
  WaitUntilReady(provider);

  if (provider->on_external_managers_synchronized().is_signaled()) {
    return;
  }

  base::RunLoop run_loop;
  provider->on_external_managers_synchronized().Post(FROM_HERE,
                                                     run_loop.QuitClosure());
  run_loop.Run();
}

void AwaitStartWebAppProviderAndSubsystems(Profile* profile) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableDefaultApps);
  FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile);
  DCHECK(provider);
  provider->StartWithSubsystems();
  WaitUntilWebAppProviderAndSubsystemsReady(provider);
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
  provider->scheduler().InstallFromInfo(
      std::move(web_app_info), overwrite_existing_manifest_fields,
      install_source,
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, webapps::InstallResultCode code) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
            app_id = installed_app_id;
            run_loop.Quit();
          }));

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

bool UninstallAllWebApps(Profile* profile) {
  bool success = true;
  auto* provider = WebAppProvider::GetForTest(profile);
  if (!provider)
    return false;
  std::vector<AppId> app_ids = provider->registrar().GetAppIds();
  for (auto& app_id : app_ids) {
    const WebApp* app = provider->registrar().GetAppById(app_id);
    WebAppSourcesSet sources =
        WebAppSourcesSet::FromEnumBitmask(app->GetSources().to_ullong());

    // Non-user installs first, as they block user uninstalls.
    for (WebAppManagement::Type source : sources) {
      if (source == WebAppManagement::kSync)
        continue;
      base::test::TestFuture<webapps::UninstallResultCode> result;
      provider->install_finalizer().UninstallExternalWebApp(
          app_id, source, webapps::WebappUninstallSource::kTestCleanup,
          result.GetCallback());
      if (!result.Wait() ||
          result.Get() == webapps::UninstallResultCode::kError) {
        LOG(ERROR) << "Error uninstalling " << app_id;
        success = false;
      }
    }

    // User uninstalls now, which should be unblocked now.
    for (WebAppManagement::Type source : sources) {
      if (source != WebAppManagement::kSync)
        continue;
      base::test::TestFuture<webapps::UninstallResultCode> result;
      provider->install_finalizer().UninstallWebApp(
          app_id, webapps::WebappUninstallSource::kTestCleanup,
          result.GetCallback());
      if (!result.Wait() ||
          result.Get() == webapps::UninstallResultCode::kError) {
        LOG(ERROR) << "Error uninstalling " << app_id;
        success = false;
      }
    }
  }
  return success;
}

}  // namespace test
}  // namespace web_app
