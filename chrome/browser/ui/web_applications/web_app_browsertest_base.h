// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_BROWSERTEST_BASE_H_

#include "base/auto_reset.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/ash/test_util.h"
#endif

class Profile;

namespace base {
class CommandLine;
}

namespace content {
class WebContents;
}

namespace web_app {

class OsIntegrationTestOverrideImpl;
class WebAppProvider;

#if BUILDFLAG(IS_CHROMEOS)
using WebAppBrowserTestBaseParent = ChromeOSBrowserUITest;
#else
using WebAppBrowserTestBaseParent = MixinBasedInProcessBrowserTest;
#endif

// This is the recommended base class for Web App browsertests. It provides
// essential baseline functionality, including:
// 1. Automatic faking of OS integration (e.g. creating desktop shortcuts) via
//   `OsIntegrationTestOverrideImpl` (inspect via `os_integration_override()`).
// 2. Dynamic replacement of $PORT in static test data via
//    RegisterPortReplacementHandler
// 3. Dynamically serving .well-known file for app migration and scope
//    extensions using RegisterAssociatedOriginWellKnownHandler.
// 4. Waiting for the WebAppProvider system to fully start in SetUpOnMainThread.
//
// Like all browsertests, this runs a real browser, meaning the full
// `WebAppProvider` system starts automatically during profile initialization.
// Faking dependencies is rarely needed and generally discouraged in
// browsertests. To fake a dependency, use a `FakeWebAppProviderCreator` in the
// test fixture's constructor to intercept the provider creation before the
// system starts. For more details, see:
// chrome/browser/web_applications/docs/testing.md
class WebAppBrowserTestBase : public WebAppBrowserTestBaseParent {
 public:
  WebAppBrowserTestBase();
  WebAppBrowserTestBase(const WebAppBrowserTestBase&) = delete;
  WebAppBrowserTestBase& operator=(const WebAppBrowserTestBase&) = delete;
  ~WebAppBrowserTestBase() override = 0;

  WebAppProvider& provider();

  // Returns the profile from the browser() object, during test set up.
  Profile* profile();

  webapps::AppId InstallPWA(const GURL& app_url);

  webapps::AppId InstallWebApp(std::unique_ptr<WebAppInstallInfo> web_app_info);

  void UninstallWebApp(const webapps::AppId& app_id);

  // Launches the app as a window and returns the browser.
  Browser* LaunchWebAppBrowser(const webapps::AppId&);

  // Launches the app, waits for the app url to load.
  Browser* LaunchWebAppBrowserAndWait(const webapps::AppId&);

  // Launches the app, waits for it to load and finish the installability check.
  Browser* LaunchWebAppBrowserAndAwaitInstallabilityCheck(
      const webapps::AppId&);

  // Launches the app as a tab and returns the browser.
  Browser* LaunchBrowserForWebAppInTab(const webapps::AppId&);

  // Simulates a page navigating itself to an URL and waits for the
  // navigation.
  [[nodiscard]] bool NavigateInRenderer(content::WebContents* contents,
                                        const GURL& url);

  // Returns whether the installable check passed.
  static bool NavigateAndAwaitInstallabilityCheck(Browser* browser,
                                                  const GURL& url);

  Browser* NavigateInNewWindowAndAwaitInstallabilityCheck(const GURL&);

  std::optional<webapps::AppId> FindAppWithUrlInScope(const GURL& url);

  // Opens |url| in a new popup window with the dimensions |popup_size|.
  Browser* OpenPopupAndWait(Browser* browser,
                            const GURL& url,
                            const gfx::Size& popup_size);

  OsIntegrationTestOverrideImpl& os_integration_override();

  // Registers a request handler with the `https_server_` that intercepts
  // requests for files ending with "manifest.replaceport.json". It dynamically
  // replaces all occurrences of the string "$PORT" in the file's content
  // with the actual port number the server is running on.
  void RegisterPortReplacementHandler();

  // Registers a request handler with the `https_server_` that intercepts
  // requests for the `/.well-known/web-app-origin-association` endpoint.
  // It responds to requests exactly matching the `source_host` domain
  // and returns a valid Web App Origin Association JSON authorizing the
  // `target_manifest_id_template` (with $PORT replaced by the active port)
  // to migrate from the source app.
  // Note: This can currently only be called once per host (or origin).
  // TODO(crbug.com/494641551): Create a map of origin -> target manifests and
  // have a single handler look at that map instead of registering multiple
  // handlers.
  void RegisterAssociatedOriginWellKnownHandler(
      const std::string& source_host,
      const std::string& target_manifest_id_template,
      const std::string& allowed_scope = "/");

 protected:
  WebAppBrowserTestBase(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features);

  content::WebContents* OpenApplication(const webapps::AppId&);


  virtual void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) {}

  GURL GetInstallableAppURL();
  GURL GetAppURLWithManifest(const std::string& manifest_url);
  static const char* GetInstallableAppName();

  // InProcessBrowserTest:
  void SetUp() override;
  void TearDown() override;
  void SetUpInProcessBrowserTestFixture() override;

  std::unique_ptr<net::test_server::HttpResponse> HandlePortReplacement(
      const net::test_server::HttpRequest& request);

  void TearDownInProcessBrowserTestFixture() override;
  void TearDownOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void PreRunTestOnMainThread() override;
  void SetUpOnMainThread() override;

 private:
  // TODO(https://crbug.com/423465927): Explore a better approach to make the
  // existing tests run with the prewarm feature enabled.
  ::test::ScopedPrewarmFeatureList prewarm_feature_list_{
      ::test::ScopedPrewarmFeatureList::PrewarmState::kDisabled};

  OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
  base::TimeTicks start_time_ = base::TimeTicks::Now();

  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription create_services_subscription_;

  // Similar to net::MockCertVerifier, but also updates the CertVerifier
  // used by the NetworkService.
  content::ContentMockCertVerifier cert_verifier_;
  // Store separately instead of accessing directly from `browser()`, as some
  // tests close that browser (and thus make it a UAF).
  base::WeakPtr<Profile> browser_profile_;
  base::AutoReset<std::optional<AppIdentityUpdate>> update_dialog_scope_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_BROWSERTEST_BASE_H_
