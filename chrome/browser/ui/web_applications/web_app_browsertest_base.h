// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_BROWSERTEST_BASE_H_

#include "base/auto_reset.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
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
#include "chrome/browser/ui/chromeos/test_util.h"
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

// Base class for tests of user interface support for web applications.
class WebAppBrowserTestBase : public WebAppBrowserTestBaseParent {
 public:
  WebAppBrowserTestBase();
  WebAppBrowserTestBase(const WebAppBrowserTestBase&) = delete;
  WebAppBrowserTestBase& operator=(const WebAppBrowserTestBase&) =
      delete;
  ~WebAppBrowserTestBase() override = 0;

  WebAppProvider& provider();

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

  std::optional<webapps::AppId> FindAppWithUrlInScope(const GURL& url);

  // Opens |url| in a new popup window with the dimensions |popup_size|.
  Browser* OpenPopupAndWait(Browser* browser,
                            const GURL& url,
                            const gfx::Size& popup_size);

  OsIntegrationTestOverrideImpl& os_integration_override();

 protected:
  WebAppBrowserTestBase(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features);

  content::WebContents* OpenApplication(const webapps::AppId&);

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
  OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
  base::TimeTicks start_time_ = base::TimeTicks::Now();

  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;
  base::CallbackListSubscription create_services_subscription_;

  // Similar to net::MockCertVerifier, but also updates the CertVerifier
  // used by the NetworkService.
  content::ContentMockCertVerifier cert_verifier_;
  base::AutoReset<std::optional<AppIdentityUpdate>> update_dialog_scope_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_BROWSERTEST_BASE_H_
