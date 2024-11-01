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
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
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

webapps::AppId InstallDummyWebApp(
    Profile* profile,
    const std::string& app_name,
    const GURL& start_url,
    const webapps::WebappInstallSource install_source) {
  auto web_app_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  web_app_info->scope = start_url.GetWithoutFilename();
  web_app_info->title = base::UTF8ToUTF16(app_name);
  web_app_info->description = base::UTF8ToUTF16(app_name);
  web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
  web_app_info->install_url = start_url;

  return InstallWebApp(profile, std::move(web_app_info),
                       /*overwrite_existing_manifest_fields=*/true,
                       install_source);
}

webapps::AppId InstallWebApp(Profile* profile,
                             std::unique_ptr<WebAppInstallInfo> web_app_info,
                             bool overwrite_existing_manifest_fields,
                             webapps::WebappInstallSource install_source) {
  // Use InstallShortcut for Create Shortcut install source.
  CHECK_NE(install_source, webapps::WebappInstallSource::MENU_CREATE_SHORTCUT);

  // The sync system requires that sync entity name is never empty.
  if (web_app_info->title.empty())
    web_app_info->title = u"WebAppInstallInfo App Name";

  webapps::AppId app_id;
  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      future;
  auto* provider = WebAppProvider::GetForTest(profile);
  DCHECK(provider);
  WaitUntilReady(provider);

  // In unit tests, we do not have Browser or WebContents instances. Hence we
  // use `InstallFromInfoCommand` instead of `FetchManifestAndInstallCommand` or
  // `WebAppInstallCommand` to install the web app.

  // If OS integration is being handed by the test (as in, it is suppressing it
  // in the OsIntegrationManager or it is using the OsIntegrationTestOverride),
  // then we can safely install with os integration. Otherwise, keep it off
  // until we can solve this better later. Note: On ChromeOS, we never need to
  // skip OS integration, so always keep it enabled.
  // TODO(https://crbug.com/328524602): Have all installs do OS integration, and
  // ensure that all browsertest / unit tests classes have the execution of the
  // integration suppressed or overridden.
  WebAppInstallParams params;
#if !BUILDFLAG(IS_CHROMEOS)
  auto does_test_handle_os_integration = []() {
    return OsIntegrationTestOverride::Get() ||
           OsIntegrationManager::AreOsHooksSuppressedForTesting();
  };

  if (!does_test_handle_os_integration()) {
    params.install_state =
        proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION;
    params.add_to_applications_menu = false;
    params.add_to_desktop = false;
    params.add_to_quick_launch_bar = false;
  }
#endif
  provider->scheduler().InstallFromInfoWithParams(
      std::move(web_app_info), overwrite_existing_manifest_fields,
      install_source, future.GetCallback(), params);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            future.Get<webapps::InstallResultCode>());
  // Allow updates to be published to App Service listeners.
  base::RunLoop().RunUntilIdle();

  return future.Get<webapps::AppId>();
}

webapps::AppId InstallWebAppWithoutOsIntegration(
    Profile* profile,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    bool overwrite_existing_manifest_fields,
    webapps::WebappInstallSource install_source) {
  // Use InstallShortcut for Create Shortcut install source.
  CHECK_NE(install_source, webapps::WebappInstallSource::MENU_CREATE_SHORTCUT);

  // The sync system requires that sync entity name is never empty.
  if (web_app_info->title.empty()) {
    web_app_info->title = u"WebAppInstallInfo App Name";
  }

  webapps::AppId app_id;
  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      future;
  auto* provider = WebAppProvider::GetForTest(profile);
  DCHECK(provider);
  WaitUntilReady(provider);
  provider->scheduler().InstallFromInfoNoIntegrationForTesting(
      std::move(web_app_info), overwrite_existing_manifest_fields,
      install_source, future.GetCallback());

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            future.Get<webapps::InstallResultCode>());
  // Allow updates to be published to App Service listeners.
  base::RunLoop().RunUntilIdle();

  return future.Get<webapps::AppId>();
}

webapps::AppId InstallShortcut(Profile* profile,
                               const std::string& shortcut_name,
                               const GURL& start_url,
                               bool create_default_icon,
                               bool is_policy_install) {
  auto web_app_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  // Explicitly clear the scope, because this is a shortcut.
  web_app_info->scope = GURL();
  web_app_info->title = base::UTF8ToUTF16(shortcut_name);
  web_app_info->user_display_mode = mojom::UserDisplayMode::kBrowser;
  if (create_default_icon) {
    const GeneratedIconsInfo icon_info(
        IconPurpose::ANY, {web_app::icon_size::k32}, {SK_ColorBLACK});
    web_app::AddIconsToWebAppInstallInfo(web_app_info.get(), start_url,
                                         {icon_info});
  }
  // The sync system requires that sync entity name is never empty.
  if (web_app_info->title.empty()) {
    web_app_info->title = u"WebAppInstallInfo Shortcut Name";
  }

  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      future;
  auto* provider = WebAppProvider::GetForTest(profile);
  DCHECK(provider);
  WaitUntilReady(provider);

  WebAppInstallParams params;
#if !BUILDFLAG(IS_CHROMEOS)
  params.install_state = proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION;
  params.add_to_applications_menu = false;
  params.add_to_desktop = false;
  params.add_to_quick_launch_bar = false;
#endif

  // In unit tests, we do not have Browser or WebContents instances. Hence we
  // use `InstallFromInfoCommand` instead of `FetchManifestAndInstallCommand` or
  // `WebAppInstallCommand` to install the web app.
  provider->scheduler().InstallFromInfoWithParams(
      std::move(web_app_info), /*overwrite_existing_manifest_fields =*/true,
      is_policy_install ? webapps::WebappInstallSource::EXTERNAL_POLICY
                        : webapps::WebappInstallSource::MENU_CREATE_SHORTCUT,
      future.GetCallback(), params);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            future.Get<webapps::InstallResultCode>());

  // Allow updates to be published to App Service listeners.
  base::RunLoop().RunUntilIdle();

  CHECK(
      provider->registrar_unsafe().IsShortcutApp(future.Get<webapps::AppId>()));
  return future.Get<webapps::AppId>();
}

void UninstallWebApp(Profile* profile,
                     const webapps::AppId& app_id,
                     webapps::WebappUninstallSource uninstall_source) {
  WebAppProvider* const provider = WebAppProvider::GetForTest(profile);
  base::test::TestFuture<webapps::UninstallResultCode> future;

  DCHECK(provider->registrar_unsafe().CanUserUninstallWebApp(app_id));
  provider->scheduler().RemoveUserUninstallableManagements(
      app_id, uninstall_source, future.GetCallback());

  EXPECT_TRUE(UninstallSucceeded(future.Get()));
  // Allow updates to be published to App Service listeners.
  base::RunLoop().RunUntilIdle();
}

bool UninstallAllWebApps(Profile* profile) {
  bool success = true;
  auto* provider = WebAppProvider::GetForTest(profile);
  if (!provider)
    return false;
  // Wait for any existing commands to complete before reading the registrar.
  provider->command_manager().AwaitAllCommandsCompleteForTesting();
  std::vector<webapps::AppId> app_ids =
      provider->registrar_unsafe().GetAppIds();
  for (auto& app_id : app_ids) {
    const WebApp* app = provider->registrar_unsafe().GetAppById(app_id);
    WebAppManagementTypes sources = app->GetSources();

    // Non-user installs first, as they block user uninstalls.
    for (WebAppManagement::Type source : sources) {
      if (source == WebAppManagement::kSync)
        continue;
      base::test::TestFuture<webapps::UninstallResultCode> result;
      provider->scheduler().RemoveInstallManagementMaybeUninstall(
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
      provider->scheduler().RemoveInstallManagementMaybeUninstall(
          app_id, source, webapps::WebappUninstallSource::kTestCleanup,
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
