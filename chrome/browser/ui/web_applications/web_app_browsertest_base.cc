// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/test/debug_info_printer.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "base/containers/extend.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace web_app {

WebAppBrowserTestBase::WebAppBrowserTestBase()
    : WebAppBrowserTestBase({}, {}) {}

WebAppBrowserTestBase::WebAppBrowserTestBase(
    const std::vector<base::test::FeatureRef>& enabled_features,
    const std::vector<base::test::FeatureRef>& disabled_features)
    // TODO(crbug.com/40874949): Fix the manifest update process by ensuring
    // during test installs, an app is installed from the manifest so that the
    // identity update dialog is not triggered after navigation. This will
    // ensure removal of update_dialog_scope_.
    : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
      update_dialog_scope_(SetIdentityUpdateDialogActionForTesting(
          AppIdentityUpdate::kSkipped)) {
  std::vector<base::test::FeatureRef> all_disabled_features = disabled_features;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::Extend(all_disabled_features,
               ash::standalone_browser::GetFeatureRefs());
#endif
  scoped_feature_list_.InitWithFeatures(enabled_features,
                                        all_disabled_features);
}

WebAppBrowserTestBase::~WebAppBrowserTestBase() = default;

WebAppProvider& WebAppBrowserTestBase::provider() {
  auto* provider = WebAppProvider::GetForTest(profile());
  DCHECK(provider);
  return *provider;
}

Profile* WebAppBrowserTestBase::profile() {
  return browser()->profile();
}

webapps::AppId WebAppBrowserTestBase::InstallPWA(const GURL& start_url) {
  auto web_app_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  web_app_info->scope = start_url.GetWithoutFilename();
  web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
  web_app_info->title = u"A Web App";
  return web_app::test::InstallWebApp(profile(), std::move(web_app_info));
}

webapps::AppId WebAppBrowserTestBase::InstallWebApp(
    std::unique_ptr<WebAppInstallInfo> web_app_info) {
  return web_app::test::InstallWebApp(profile(), std::move(web_app_info));
}

void WebAppBrowserTestBase::UninstallWebApp(
    const webapps::AppId& app_id) {
  web_app::test::UninstallWebApp(profile(), app_id);
}

Browser* WebAppBrowserTestBase::LaunchWebAppBrowser(
    const webapps::AppId& app_id) {
  return web_app::LaunchWebAppBrowser(profile(), app_id);
}

Browser* WebAppBrowserTestBase::LaunchWebAppBrowserAndWait(
    const webapps::AppId& app_id) {
  return web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
}

Browser*
WebAppBrowserTestBase::LaunchWebAppBrowserAndAwaitInstallabilityCheck(
    const webapps::AppId& app_id) {
  Browser* browser = web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
  webapps::TestAppBannerManagerDesktop::FromWebContents(
      browser->tab_strip_model()->GetActiveWebContents())
      ->WaitForInstallableCheck();
  return browser;
}

Browser* WebAppBrowserTestBase::LaunchBrowserForWebAppInTab(
    const webapps::AppId& app_id) {
  return web_app::LaunchBrowserForWebAppInTab(profile(), app_id);
}

content::WebContents* WebAppBrowserTestBase::OpenWindow(
    content::WebContents* contents,
    const GURL& url) {
  content::WebContentsAddedObserver tab_added_observer;
  EXPECT_TRUE(content::ExecJs(contents, "window.open('" + url.spec() + "');"));
  content::WebContents* new_contents = tab_added_observer.GetWebContents();
  EXPECT_TRUE(new_contents);
  WaitForLoadStop(new_contents);

  EXPECT_EQ(url, contents->GetController().GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(
      content::PAGE_TYPE_NORMAL,
      new_contents->GetController().GetLastCommittedEntry()->GetPageType());
  EXPECT_EQ(contents->GetPrimaryMainFrame()->GetSiteInstance(),
            new_contents->GetPrimaryMainFrame()->GetSiteInstance());

  return new_contents;
}

bool WebAppBrowserTestBase::NavigateInRenderer(
    content::WebContents* contents,
    const GURL& url) {
  EXPECT_TRUE(
      content::ExecJs(contents, "window.location = '" + url.spec() + "';"));
  bool success = content::WaitForLoadStop(contents);
  EXPECT_EQ(url, contents->GetController().GetLastCommittedEntry()->GetURL());
  return success;
}

// static
bool WebAppBrowserTestBase::NavigateAndAwaitInstallabilityCheck(
    Browser* browser,
    const GURL& url) {
  auto* manager = webapps::TestAppBannerManagerDesktop::FromWebContents(
      browser->tab_strip_model()->GetActiveWebContents());
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, url));
  return manager->WaitForInstallableCheck();
}

Browser*
WebAppBrowserTestBase::NavigateInNewWindowAndAwaitInstallabilityCheck(
    const GURL& url) {
  Browser* new_browser = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, profile(), true));
  AddBlankTabAndShow(new_browser);
  NavigateAndAwaitInstallabilityCheck(new_browser, url);
  return new_browser;
}

std::optional<webapps::AppId>
WebAppBrowserTestBase::FindAppWithUrlInScope(const GURL& url) {
  return provider().registrar_unsafe().FindAppWithUrlInScope(url);
}

Browser* WebAppBrowserTestBase::OpenPopupAndWait(
    Browser* browser,
    const GURL& url,
    const gfx::Size& popup_size) {
  content::WebContents* const web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  ui_test_utils::BrowserChangeObserver browser_change_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  std::string open_window_script = base::StringPrintf(
      "window.open('%s', '_blank', 'toolbar=none,width=%i,height=%i')",
      url.spec().c_str(), popup_size.width(), popup_size.height());

  EXPECT_TRUE(content::ExecJs(web_contents, open_window_script));

  // The navigation should happen in a new window.
  Browser* popup_browser = browser_change_observer.Wait();
  EXPECT_NE(browser, popup_browser);

  content::WebContents* popup_contents =
      popup_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(popup_contents));
  EXPECT_EQ(popup_contents->GetLastCommittedURL(), url);

  return popup_browser;
}

OsIntegrationTestOverrideImpl&
WebAppBrowserTestBase::os_integration_override() {
  return faked_os_integration_.test_override();
}

content::WebContents* WebAppBrowserTestBase::OpenApplication(
    const webapps::AppId& app_id) {
  ui_test_utils::UrlLoadObserver url_observer(
      provider().registrar_unsafe().GetAppStartUrl(app_id));

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

GURL WebAppBrowserTestBase::GetInstallableAppURL() {
  return https_server()->GetURL("/banners/manifest_test_page.html");
}

// static
const char* WebAppBrowserTestBase::GetInstallableAppName() {
  return "Manifest test app";
}

void WebAppBrowserTestBase::SetUp() {
  https_server_.AddDefaultHandlers(GetChromeTestDataDir());
  webapps::TestAppBannerManagerDesktop::SetUp();
  WebAppBrowserTestBaseParent::SetUp();
}

void WebAppBrowserTestBase::TearDown() {
  WebAppBrowserTestBaseParent::TearDown();
}

void WebAppBrowserTestBase::SetUpInProcessBrowserTestFixture() {
  WebAppBrowserTestBaseParent::SetUpInProcessBrowserTestFixture();
  cert_verifier_.SetUpInProcessBrowserTestFixture();
  create_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
              &WebAppBrowserTestBase::OnWillCreateBrowserContextServices,
              base::Unretained(this)));
}

void WebAppBrowserTestBase::TearDownInProcessBrowserTestFixture() {
  WebAppBrowserTestBaseParent::TearDownInProcessBrowserTestFixture();
  cert_verifier_.TearDownInProcessBrowserTestFixture();
}

void WebAppBrowserTestBase::TearDownOnMainThread() {
  if (testing::Test::HasFailure()) {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::TimeDelta log_time = base::TimeTicks::Now() - start_time_;
    test::LogDebugInfoToConsole(profile_manager->GetLoadedProfiles(), log_time);
  }
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!chromeos::BrowserParamsProxy::IsCrosapiDisabledForTesting()) {
    // Make sure all ash browser UI are closed before the test tears down.
    CloseAllAshBrowserWindows();
  }
#endif

  WebAppBrowserTestBaseParent::TearDownOnMainThread();
}

void WebAppBrowserTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Browser will both run and display insecure content.
  command_line->AppendSwitch(switches::kAllowRunningInsecureContent);
  cert_verifier_.SetUpCommandLine(command_line);
}

void WebAppBrowserTestBase::SetUpOnMainThread() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!chromeos::BrowserParamsProxy::IsCrosapiDisabledForTesting()) {
    CHECK(IsWebAppsCrosapiEnabled());
  }
#endif

  WebAppBrowserTestBaseParent::SetUpOnMainThread();

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(https_server()->Start());

  // By default, all SSL cert checks are valid. Can be overridden in tests.
  cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);

  web_app::test::WaitUntilReady(
      web_app::WebAppProvider::GetForTest(browser()->profile()));
}

}  // namespace web_app
