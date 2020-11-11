// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_CONTROLLER_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_CONTROLLER_BROWSERTEST_H_

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace base {
class CommandLine;
}

namespace content {
class WebContents;
}

namespace web_app {

class WebAppProviderBase;

// Base class for tests of user interface support for web applications.
class WebAppControllerBrowserTestBase
    : public extensions::ExtensionBrowserTest {
 public:
  WebAppControllerBrowserTestBase();
  WebAppControllerBrowserTestBase(const WebAppControllerBrowserTestBase&) =
      delete;
  WebAppControllerBrowserTestBase& operator=(
      const WebAppControllerBrowserTestBase&) = delete;
  ~WebAppControllerBrowserTestBase() override = 0;

  WebAppProviderBase& provider();

  AppId InstallPWA(const GURL& app_url);

  AppId InstallWebApp(std::unique_ptr<WebApplicationInfo> web_app_info);

  // Launches the app as a window and returns the browser.
  Browser* LaunchWebAppBrowser(const AppId&);

  // Launches the app, waits for the app url to load.
  Browser* LaunchWebAppBrowserAndWait(const AppId&);

  // Launches the app, waits for it to load and finish the installability check.
  Browser* LaunchWebAppBrowserAndAwaitInstallabilityCheck(const AppId&);

  // Launches the app as a tab and returns the browser.
  Browser* LaunchBrowserForWebAppInTab(const AppId&);

  // Returns whether the installable check passed.
  static bool NavigateAndAwaitInstallabilityCheck(Browser* browser,
                                                  const GURL& url);

  Browser* NavigateInNewWindowAndAwaitInstallabilityCheck(const GURL&);

  base::Optional<AppId> FindAppWithUrlInScope(const GURL& url);
};

class WebAppControllerBrowserTest : public WebAppControllerBrowserTestBase {
 public:
  WebAppControllerBrowserTest();
  ~WebAppControllerBrowserTest() override = 0;

  // ExtensionBrowserTest:
  void SetUp() override;

 protected:
  content::WebContents* OpenApplication(const AppId&);

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  GURL GetInstallableAppURL();
  static const char* GetInstallableAppName();

  // ExtensionBrowserTest:
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownInProcessBrowserTestFixture() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;

  // Similar to net::MockCertVerifier, but also updates the CertVerifier
  // used by the NetworkService.
  content::ContentMockCertVerifier cert_verifier_;

  ScopedOsHooksSuppress os_hooks_suppress_;

  DISALLOW_COPY_AND_ASSIGN(WebAppControllerBrowserTest);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_CONTROLLER_BROWSERTEST_H_
