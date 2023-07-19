// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/commands/fetch_manifest_and_install_command.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/test/service_worker_registration_waiter.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

namespace {

void AutoAcceptDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    WebAppInstallationAcceptanceCallback acceptance_callback) {
  web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
  std::move(acceptance_callback)
      .Run(
          /*user_accepted=*/true, std::move(web_app_info));
}

}  // namespace

AppId InstallWebAppFromPage(Browser* browser, const GURL& app_url) {
  NavigateToURLAndWait(browser, app_url);

  AppId app_id;
  base::RunLoop run_loop;

  auto* provider = WebAppProvider::GetForTest(browser->profile());
  DCHECK(provider);
  test::WaitUntilReady(provider);
  provider->scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      browser->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      /*bypass_service_worker_check=*/false,
      base::BindOnce(&AutoAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&run_loop, &app_id](const AppId& installed_app_id,
                               webapps::InstallResultCode code) {
            DCHECK_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            app_id = installed_app_id;
            run_loop.Quit();
          }),
      /*use_fallback=*/true);

  run_loop.Run();
  return app_id;
}

AppId InstallWebAppFromPageAndCloseAppBrowser(Browser* browser,
                                              const GURL& app_url) {
  // Create new tab to navigate, install, automatically pop out and then
  // close. This sequence avoids altering the browser window state it started
  // with.
  chrome::AddTabAt(browser, app_url, /*index=*/-1,
                   /*foreground=*/true);

  ui_test_utils::BrowserChangeObserver observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  AppId app_id = InstallWebAppFromPage(browser, app_url);

  Browser* app_browser = observer.Wait();
  DCHECK_NE(app_browser, browser);
  DCHECK(AppBrowserController::IsForWebApp(app_browser, app_id));
  chrome::CloseWindow(app_browser);

  return app_id;
}

AppId InstallWebAppFromManifest(Browser* browser, const GURL& app_url) {
  ServiceWorkerRegistrationWaiter registration_waiter(browser->profile(),
                                                      app_url);
  NavigateToURLAndWait(browser, app_url);
  registration_waiter.AwaitRegistration();

  AppId app_id;
  base::RunLoop run_loop;

  auto* provider = WebAppProvider::GetForTest(browser->profile());
  DCHECK(provider);
  test::WaitUntilReady(provider);
  provider->scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      browser->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
      /*bypass_service_worker_check=*/false,
      base::BindOnce(&AutoAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&run_loop, &app_id](const AppId& installed_app_id,
                               webapps::InstallResultCode code) {
            DCHECK_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
            app_id = installed_app_id;
            run_loop.Quit();
          }),
      /*use_fallback=*/true);

  run_loop.Run();
  return app_id;
}

Browser* LaunchWebAppBrowser(Profile* profile,
                             const AppId& app_id,
                             WindowOpenDisposition disposition) {
  content::WebContents* web_contents =
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->BrowserAppLauncher()
          ->LaunchAppWithParamsForTesting(apps::AppLaunchParams(
              app_id, apps::LaunchContainer::kLaunchContainerWindow,
              disposition, apps::LaunchSource::kFromTest));
  EXPECT_TRUE(web_contents);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  EXPECT_TRUE(AppBrowserController::IsForWebApp(browser, app_id));
  return browser;
}

// Launches the app, waits for the app url to load.
Browser* LaunchWebAppBrowserAndWait(Profile* profile,
                                    const AppId& app_id,
                                    WindowOpenDisposition disposition) {
  ui_test_utils::UrlLoadObserver url_observer(
      WebAppProvider::GetForTest(profile)->registrar_unsafe().GetAppLaunchUrl(
          app_id),
      content::NotificationService::AllSources());
  Browser* const app_browser =
      LaunchWebAppBrowser(profile, app_id, disposition);
  url_observer.Wait();
  return app_browser;
}

Browser* LaunchBrowserForWebAppInTab(Profile* profile, const AppId& app_id) {
  content::WebContents* web_contents =
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->BrowserAppLauncher()
          ->LaunchAppWithParamsForTesting(apps::AppLaunchParams(
              app_id, apps::LaunchContainer::kLaunchContainerTab,
              WindowOpenDisposition::NEW_FOREGROUND_TAB,
              apps::LaunchSource::kFromTest));
  DCHECK(web_contents);

  EXPECT_EQ(app_id, *WebAppTabHelper::GetAppId(web_contents));

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  EXPECT_EQ(browser, chrome::FindLastActive());
  EXPECT_EQ(web_contents, browser->tab_strip_model()->GetActiveWebContents());
  return browser;
}

Browser* LaunchWebAppToURL(Profile* profile,
                           const AppId& app_id,
                           const GURL& url) {
  apps::AppLaunchParams params(
      app_id, apps::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromCommandLine);
  params.override_url = url;
  content::WebContents* const web_contents =
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->BrowserAppLauncher()
          ->LaunchAppWithParamsForTesting(std::move(params));
  EXPECT_TRUE(web_contents);

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  EXPECT_TRUE(AppBrowserController::IsForWebApp(browser, app_id));
  return browser;
}

ExternalInstallOptions CreateInstallOptions(
    const GURL& url,
    const ExternalInstallSource& source) {
  ExternalInstallOptions install_options(
      url, mojom::UserDisplayMode::kStandalone, source);
  // Avoid creating real shortcuts in tests.
  install_options.add_to_applications_menu = false;
  install_options.add_to_desktop = false;
  install_options.add_to_quick_launch_bar = false;

  return install_options;
}

ExternallyManagedAppManager::InstallResult ExternallyManagedAppManagerInstall(
    Profile* profile,
    ExternalInstallOptions install_options) {
  DCHECK(profile);
  auto* provider = WebAppProvider::GetForTest(profile);
  DCHECK(provider);
  test::WaitUntilReady(provider);
  base::RunLoop run_loop;
  ExternallyManagedAppManager::InstallResult result;

  provider->externally_managed_app_manager().Install(
      std::move(install_options),
      base::BindLambdaForTesting(
          [&result, &run_loop](
              const GURL& provided_url,
              ExternallyManagedAppManager::InstallResult install_result) {
            result = install_result;
            run_loop.Quit();
          }));
  run_loop.Run();
  return result;
}

void NavigateToURLAndWait(Browser* browser,
                          const GURL& url,
                          bool proceed_through_interstitial) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  {
    content::TestNavigationObserver observer(
        web_contents, content::MessageLoopRunner::QuitMode::DEFERRED);
    NavigateParams params(browser, url, ui::PAGE_TRANSITION_LINK);
    ui_test_utils::NavigateToURL(&params);
    observer.WaitForNavigationFinished();
  }

  if (!proceed_through_interstitial)
    return;

  {
    // Need a second TestNavigationObserver; the above one is spent.
    content::TestNavigationObserver observer(
        web_contents, content::MessageLoopRunner::QuitMode::DEFERRED);
    security_interstitials::SecurityInterstitialTabHelper* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            browser->tab_strip_model()->GetActiveWebContents());
    ASSERT_TRUE(
        helper &&
        helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting());
    std::string javascript = "window.certificateErrorPageController.proceed();";
    ASSERT_TRUE(content::ExecJs(web_contents, javascript));
    observer.Wait();
  }
}

// Performs a navigation and then checks that the toolbar visibility is as
// expected.
void NavigateAndCheckForToolbar(Browser* browser,
                                const GURL& url,
                                bool expected_visibility,
                                bool proceed_through_interstitial) {
  NavigateToURLAndWait(browser, url, proceed_through_interstitial);
  EXPECT_EQ(expected_visibility,
            browser->app_controller()->ShouldShowCustomTabBar());
}

AppMenuCommandState GetAppMenuCommandState(int command_id, Browser* browser) {
  DCHECK(!browser->app_controller())
      << "This check only applies to regular browser windows.";
  auto app_menu_model = std::make_unique<AppMenuModel>(nullptr, browser);
  app_menu_model->Init();
  ui::MenuModel* model = app_menu_model.get();
  size_t index = 0;
  if (!app_menu_model->GetModelAndIndexForCommandId(command_id, &model,
                                                    &index)) {
    return kNotPresent;
  }
  return model->IsEnabledAt(index) ? kEnabled : kDisabled;
}

Browser* FindWebAppBrowser(Profile* profile, const AppId& app_id) {
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != profile)
      continue;

    if (AppBrowserController::IsForWebApp(browser, app_id))
      return browser;
  }

  return nullptr;
}

void CloseAndWait(Browser* browser) {
  BrowserWaiter waiter(browser);
  browser->window()->Close();
  waiter.AwaitRemoved();
}

bool IsBrowserOpen(const Browser* test_browser) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser == test_browser)
      return true;
  }
  return false;
}

absl::optional<AppId> ForceInstallWebApp(Profile* profile, GURL url) {
  web_app::ExternalInstallOptions install_options(
      url, web_app::mojom::UserDisplayMode::kStandalone,
      web_app::ExternalInstallSource::kExternalPolicy);
  auto result =
      ExternallyManagedAppManagerInstall(profile, std::move(install_options));
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  const auto& registrar =
      WebAppProvider::GetForTest(profile)->registrar_unsafe();
  absl::optional<web_app::AppId> policy_app_id =
      registrar.LookupExternalAppId(url);
  EXPECT_TRUE(policy_app_id.has_value());
  EXPECT_TRUE(
      registrar.GetAppById(policy_app_id.value())->IsPolicyInstalledApp());
  return policy_app_id;
}

BrowserWaiter::BrowserWaiter(Browser* filter) : filter_(filter) {
  BrowserList::AddObserver(this);
}

BrowserWaiter::~BrowserWaiter() {
  BrowserList::RemoveObserver(this);
}

Browser* BrowserWaiter::AwaitAdded(const base::Location& location) {
  added_run_loop_.Run(location);
  return added_browser_;
}

Browser* BrowserWaiter::AwaitRemoved(const base::Location& location) {
  if (!removed_browser_)
    removed_run_loop_.Run(location);
  return removed_browser_;
}

void BrowserWaiter::OnBrowserAdded(Browser* browser) {
  if (filter_ && browser != filter_)
    return;
  added_browser_ = browser;
  added_run_loop_.Quit();
}
void BrowserWaiter::OnBrowserRemoved(Browser* browser) {
  if (filter_ && browser != filter_)
    return;
  removed_browser_ = browser;
  removed_run_loop_.Quit();
}

UpdateAwaiter::UpdateAwaiter(WebAppInstallManager& install_manager) {
  scoped_observation_.Observe(&install_manager);
}

void UpdateAwaiter::OnWebAppInstallManagerDestroyed() {
  scoped_observation_.Reset();
}

UpdateAwaiter::~UpdateAwaiter() = default;

void UpdateAwaiter::AwaitUpdate(const base::Location& location) {
  run_loop_.Run(location);
}

void UpdateAwaiter::OnWebAppManifestUpdated(const AppId& app_id,
                                            base::StringPiece old_name) {
  run_loop_.Quit();
}

base::FilePath CreateTestFileWithExtension(base::StringPiece extension) {
  // CreateTemporaryFile blocks, temporarily allow blocking.
  base::ScopedAllowBlockingForTesting allow_blocking;

  // In order to test file handling, we need to be able to supply a file
  // extension for the temp file.
  base::FilePath test_file_path;
  base::CreateTemporaryFile(&test_file_path);
  base::FilePath new_file_path = test_file_path.AddExtensionASCII(extension);
  EXPECT_TRUE(base::ReplaceFile(test_file_path, new_file_path, nullptr));
  return new_file_path;
}

}  // namespace web_app
