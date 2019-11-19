// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_CONTROLLER_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_CONTROLLER_BROWSERTEST_H_

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/web_application_info.h"
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

enum class ControllerType {
  kHostedAppController,
  kUnifiedControllerWithBookmarkApp,
  kUnifiedControllerWithWebApp,
};

std::string ControllerTypeParamToString(
    const ::testing::TestParamInfo<ControllerType>& controller_type);

// Base class for tests of user interface support for web applications.
// ControllerType selects between use of WebAppBrowserController and
// HostedAppBrowserController.
class WebAppControllerBrowserTestBase
    : public extensions::ExtensionBrowserTest,
      public ::testing::WithParamInterface<ControllerType> {
 public:
  WebAppControllerBrowserTestBase();
  ~WebAppControllerBrowserTestBase() = 0;

  AppId InstallPWA(const GURL& app_url);

  AppId InstallWebApp(std::unique_ptr<WebApplicationInfo> web_app_info);

  // Launches the app as a window and returns the browser.
  Browser* LaunchWebAppBrowser(const AppId&);

  // Launches the app as a tab and returns the browser.
  Browser* LaunchBrowserForWebAppInTab(const AppId&);

  base::Optional<AppId> FindAppWithUrlInScope(const GURL& url);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(WebAppControllerBrowserTestBase);
};

class WebAppControllerBrowserTest : public WebAppControllerBrowserTestBase {
 public:
  WebAppControllerBrowserTest();
  ~WebAppControllerBrowserTest() = 0;

  // ExtensionBrowserTest:
  void SetUp() override;

 protected:
  content::WebContents* OpenApplication(const AppId&);

  net::EmbeddedTestServer* https_server() { return &https_server_; }

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

  DISALLOW_COPY_AND_ASSIGN(WebAppControllerBrowserTest);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_CONTROLLER_BROWSERTEST_H_
