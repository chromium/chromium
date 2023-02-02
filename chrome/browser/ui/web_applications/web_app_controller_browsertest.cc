// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"

#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/common/chrome_features.h"
#endif

namespace web_app {

WebAppControllerBrowserTest::WebAppControllerBrowserTest()
    // TODO(crbug.com/1378355): Fix the manifest update process by ensuring
    // during test installs, an app is installed from the manifest so that the
    // identity update dialog is not triggered after navigation. This will
    // ensure removal of update_dialog_scope_.
    : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
      update_dialog_scope_(SetIdentityUpdateDialogActionForTesting(
          AppIdentityUpdate::kSkipped)) {
  os_hooks_suppress_.emplace();
  scoped_feature_list_.InitWithFeatures({}, {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    features::kWebAppsCrosapi, ash::features::kLacrosPrimary
#endif
  });
}

WebAppControllerBrowserTest::~WebAppControllerBrowserTest() = default;

WebAppProvider& WebAppControllerBrowserTest::provider() {
  auto* provider = WebAppProvider::GetForTest(profile());
  DCHECK(provider);
  return *provider;
}

Profile* WebAppControllerBrowserTest::profile() {
  return browser()->profile();
}

AppId WebAppControllerBrowserTest::InstallPWA(const GURL& start_url) {
  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  web_app_info->start_url = start_url;
  web_app_info->scope = start_url.GetWithoutFilename();
  web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
  web_app_info->title = u"A Web App";
  return web_app::test::InstallWebApp(profile(), std::move(web_app_info));
}

AppId WebAppControllerBrowserTest::InstallWebApp(
    std::unique_ptr<WebAppInstallInfo> web_app_info) {
  return web_app::test::InstallWebApp(profile(), std::move(web_app_info));
}

void WebAppControllerBrowserTest::UninstallWebApp(const AppId& app_id) {
  web_app::test::UninstallWebApp(profile(), app_id);
}

Browser* WebAppControllerBrowserTest::LaunchWebAppBrowser(const AppId& app_id) {
  return web_app::LaunchWebAppBrowser(profile(), app_id);
}

Browser* WebAppControllerBrowserTest::LaunchWebAppBrowserAndWait(
    const AppId& app_id) {
  return web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
}

Browser*
WebAppControllerBrowserTest::LaunchWebAppBrowserAndAwaitInstallabilityCheck(
    const AppId& app_id) {
  Browser* browser = web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
  webapps::TestAppBannerManagerDesktop::FromWebContents(
      browser->tab_strip_model()->GetActiveWebContents())
      ->WaitForInstallableCheck();
  return browser;
}

Browser* WebAppControllerBrowserTest::LaunchBrowserForWebAppInTab(
    const AppId& app_id) {
  return web_app::LaunchBrowserForWebAppInTab(profile(), app_id);
}

content::WebContents* WebAppControllerBrowserTest::OpenWindow(
    content::WebContents* contents,
    const GURL& url) {
  content::WebContentsAddedObserver tab_added_observer;
  EXPECT_TRUE(
      content::ExecuteScript(contents, "window.open('" + url.spec() + "');"));
  content::WebContents* new_contents = tab_added_observer.GetWebContents();
  EXPECT_TRUE(new_contents);
  WaitForLoadStop(new_contents);

  EXPECT_EQ(url, new_contents->GetLastCommittedURL());
  EXPECT_EQ(
      content::PAGE_TYPE_NORMAL,
      new_contents->GetController().GetLastCommittedEntry()->GetPageType());
  EXPECT_EQ(contents->GetPrimaryMainFrame()->GetSiteInstance(),
            new_contents->GetPrimaryMainFrame()->GetSiteInstance());

  return new_contents;
}

bool WebAppControllerBrowserTest::NavigateInRenderer(
    content::WebContents* contents,
    const GURL& url) {
  EXPECT_TRUE(content::ExecuteScript(
      contents, "window.location = '" + url.spec() + "';"));
  bool success = content::WaitForLoadStop(contents);
  EXPECT_EQ(url, contents->GetController().GetLastCommittedEntry()->GetURL());
  return success;
}

// static
bool WebAppControllerBrowserTest::NavigateAndAwaitInstallabilityCheck(
    Browser* browser,
    const GURL& url) {
  auto* manager = webapps::TestAppBannerManagerDesktop::FromWebContents(
      browser->tab_strip_model()->GetActiveWebContents());
  NavigateToURLAndWait(browser, url);
  return manager->WaitForInstallableCheck();
}

Browser*
WebAppControllerBrowserTest::NavigateInNewWindowAndAwaitInstallabilityCheck(
    const GURL& url) {
  Browser* new_browser = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, profile(), true));
  AddBlankTabAndShow(new_browser);
  NavigateAndAwaitInstallabilityCheck(new_browser, url);
  return new_browser;
}

absl::optional<AppId> WebAppControllerBrowserTest::FindAppWithUrlInScope(
    const GURL& url) {
  return provider().registrar_unsafe().FindAppWithUrlInScope(url);
}

content::WebContents* WebAppControllerBrowserTest::OpenApplication(
    const AppId& app_id) {
  ui_test_utils::UrlLoadObserver url_observer(
      provider().registrar_unsafe().GetAppStartUrl(app_id),
      content::NotificationService::AllSources());

  apps::AppLaunchParams params(
      app_id, apps::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest);
  content::WebContents* contents =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->BrowserAppLauncher()
          ->LaunchAppWithParamsForTesting(std::move(params));
  url_observer.Wait();
  return contents;
}

GURL WebAppControllerBrowserTest::GetInstallableAppURL() {
  return https_server()->GetURL("/banners/manifest_test_page.html");
}

// static
const char* WebAppControllerBrowserTest::GetInstallableAppName() {
  return "Manifest test app";
}

void WebAppControllerBrowserTest::SetUp() {
  https_server_.AddDefaultHandlers(GetChromeTestDataDir());
  webapps::TestAppBannerManagerDesktop::SetUp();
  InProcessBrowserTest::SetUp();
}

void WebAppControllerBrowserTest::TearDown() {
  InProcessBrowserTest::TearDown();
}

void WebAppControllerBrowserTest::SetUpInProcessBrowserTestFixture() {
  InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  cert_verifier_.SetUpInProcessBrowserTestFixture();
  create_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
              &WebAppControllerBrowserTest::OnWillCreateBrowserContextServices,
              base::Unretained(this)));
}

void WebAppControllerBrowserTest::TearDownInProcessBrowserTestFixture() {
  InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  cert_verifier_.TearDownInProcessBrowserTestFixture();
}

void WebAppControllerBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Browser will both run and display insecure content.
  command_line->AppendSwitch(switches::kAllowRunningInsecureContent);
  cert_verifier_.SetUpCommandLine(command_line);
}

void WebAppControllerBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(https_server()->Start());

  // By default, all SSL cert checks are valid. Can be overridden in tests.
  cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);

  web_app::test::WaitUntilReady(
      web_app::WebAppProvider::GetForTest(browser()->profile()));
}

}  // namespace web_app
