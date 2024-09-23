// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_WEB_APP_NAVIGATION_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_WEB_APP_NAVIGATION_BROWSERTEST_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "url/gurl.h"

class Browser;
class Profile;

namespace content {
class TestNavigationObserver;
class WebContents;
}  // namespace content

namespace web_app {

class WebAppNavigationBrowserTest : public WebAppBrowserTestBase {
 public:
  enum class LinkTarget {
    SELF,
    BLANK,
  };

  // Various string constants for in/out-of-scope URLs.
  static const char* GetLaunchingPageHost();
  static const char* GetLaunchingPagePath();
  static const char* GetAppUrlHost();
  static const char* GetOtherAppUrlHost();
  static const char* GetAppScopePath();
  static const char* GetAppUrlPath();
  static const char* GetInScopeUrlPath();
  static const char* GetOutOfScopeUrlPath();
  static const char* GetAppName();

  static std::string CreateServerRedirect(const GURL& target_url);

  static std::unique_ptr<content::TestNavigationObserver>
  GetTestNavigationObserver(const GURL& target_url);

 protected:
  // Creates an <a> element, sets its href and target to |link_url| and |target|
  // respectively, adds it to the DOM, and clicks on it with |modifiers|.
  // |modifiers| should be based on blink::WebInputEvent::Modifiers.
  static void ClickLink(
      content::WebContents* web_contents,
      const GURL& link_url,
      LinkTarget target = LinkTarget::SELF,
      const std::string& rel = "",
      int modifiers = blink::WebInputEvent::Modifiers::kNoModifiers,
      blink::WebMouseEvent::Button button =
          blink::WebMouseEvent::Button::kLeft);

  // Creates an <a> element, sets its href and target to |link_url| and |target|
  // respectively, adds it to the DOM, and clicks on it with |modifiers|.
  // Returns once |target_url| has loaded. |modifiers| should be based on
  // blink::WebInputEvent::Modifiers.
  static void ClickLinkWithModifiersAndWaitForURL(
      content::WebContents* web_contents,
      const GURL& link_url,
      const GURL& target_url,
      LinkTarget target,
      const std::string& rel,
      int modifiers,
      blink::WebMouseEvent::Button button =
          blink::WebMouseEvent::Button::kLeft);

  // Creates an <a> element, sets its href and target to |link_url| and |target|
  // respectively, adds it to the DOM, and clicks on it. Returns once
  // |target_url| has loaded.
  static void ClickLinkAndWaitForURL(content::WebContents* web_contents,
                                     const GURL& link_url,
                                     const GURL& target_url,
                                     LinkTarget target,
                                     const std::string& rel);

  // Creates an <a> element, sets its href and target to |link_url| and |target|
  // respectively, adds it to the DOM, and clicks on it. Returns once the link
  // has loaded.
  static void ClickLinkAndWait(content::WebContents* web_contents,
                               const GURL& link_url,
                               LinkTarget target,
                               const std::string& rel);

  WebAppNavigationBrowserTest();
  ~WebAppNavigationBrowserTest() override;

  void SetUp() override;
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownInProcessBrowserTestFixture() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  Profile* profile();

  void InstallTestWebApp();
  webapps::AppId InstallTestWebApp(const std::string& app_host,
                                   const std::string& app_scope);

  Browser* OpenTestWebApp();

  // Navigates the active tab in |browser| to the launching page.
  void NavigateToLaunchingPage(Browser* browser);

  // Checks that no new windows are opened after clicking on a link to the given
  // `target_url` in the current active web contents of the `browser`.
  bool ExpectLinkClickNotCapturedIntoAppBrowser(Browser* browser,
                                                const GURL& target_url,
                                                const std::string& rel = "");

  net::EmbeddedTestServer& https_server() { return https_server_; }

  const webapps::AppId& test_web_app_id() const { return test_web_app_; }

  const GURL& test_web_app_start_url();

 private:
  net::EmbeddedTestServer https_server_;
  // Similar to net::MockCertVerifier, but also updates the CertVerifier
  // used by the NetworkService.
  content::ContentMockCertVerifier cert_verifier_;
  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;
  webapps::AppId test_web_app_;
  base::HistogramTester histogram_tester_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_WEB_APP_NAVIGATION_BROWSERTEST_H_
