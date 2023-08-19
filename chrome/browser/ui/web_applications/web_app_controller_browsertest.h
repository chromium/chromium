// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_CONTROLLER_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_CONTROLLER_BROWSERTEST_H_

#include "base/auto_reset.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

class Profile;

namespace base {
class CommandLine;
}

namespace content {
class WebContents;
}

namespace web_app {

class WebAppProvider;

// Base class for tests of user interface support for web applications.
class WebAppControllerBrowserTest : public InProcessBrowserTest {
 public:
  WebAppControllerBrowserTest();
  WebAppControllerBrowserTest(const WebAppControllerBrowserTest&) = delete;
  WebAppControllerBrowserTest& operator=(const WebAppControllerBrowserTest&) =
      delete;
  ~WebAppControllerBrowserTest() override = 0;

  WebAppProvider& provider();

  Profile* profile();

  AppId InstallPWA(const GURL& app_url);

  AppId InstallWebApp(std::unique_ptr<WebAppInstallInfo> web_app_info);

  void UninstallWebApp(const AppId& app_id);

  // Launches the app as a window and returns the browser.
  Browser* LaunchWebAppBrowser(const AppId&);

  // Launches the app, waits for the app url to load.
  Browser* LaunchWebAppBrowserAndWait(const AppId&);

  // Launches the app, waits for it to load and finish the installability check.
  Browser* LaunchWebAppBrowserAndAwaitInstallabilityCheck(const AppId&);

  // Launches the app as a tab and returns the browser.
  Browser* LaunchBrowserForWebAppInTab(const AppId&);

  // Simulates a page calling window.open on an URL and waits for the
  // navigation.
  content::WebContents* OpenWindow(content::WebContents* contents,
                                   const GURL& url);

  // Simulates a page navigating itself to an URL and waits for the
  // navigation.
  [[nodiscard]] bool NavigateInRenderer(content::WebContents* contents,
                                        const GURL& url);

  // Returns whether the installable check passed.
  static bool NavigateAndAwaitInstallabilityCheck(Browser* browser,
                                                  const GURL& url);

  Browser* NavigateInNewWindowAndAwaitInstallabilityCheck(const GURL&);

  absl::optional<AppId> FindAppWithUrlInScope(const GURL& url);

 protected:
  WebAppControllerBrowserTest(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features);

  absl::optional<OsIntegrationManager::ScopedSuppressForTesting>
      os_hooks_suppress_;

  content::WebContents* OpenApplication(const AppId&);

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  virtual void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) {}

  GURL GetInstallableAppURL();
  static const char* GetInstallableAppName();

  // InProcessBrowserTest:
  void SetUp() override;
  void TearDown() override;
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownInProcessBrowserTestFixture() override;
  void TearDownOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

 private:
  base::TimeTicks start_time_ = base::TimeTicks::Now();

  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;
  base::CallbackListSubscription create_services_subscription_;

  // Similar to net::MockCertVerifier, but also updates the CertVerifier
  // used by the NetworkService.
  content::ContentMockCertVerifier cert_verifier_;
  base::AutoReset<absl::optional<AppIdentityUpdate>> update_dialog_scope_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_CONTROLLER_BROWSERTEST_H_
