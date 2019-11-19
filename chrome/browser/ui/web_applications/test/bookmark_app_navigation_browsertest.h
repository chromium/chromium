// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_BOOKMARK_APP_NAVIGATION_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_BOOKMARK_APP_NAVIGATION_BROWSERTEST_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "extensions/common/extension.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

class Browser;

namespace content {
class TestNavigationObserver;
class WebContents;
}  // namespace content

namespace extensions {
namespace test {

class BookmarkAppNavigationBrowserTest : public ExtensionBrowserTest {
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
  // Returns once |target_url| has loaded. |modifiers| should be based on
  // blink::WebInputEvent::Modifiers.
  static void ClickLinkWithModifiersAndWaitForURL(
      content::WebContents* web_contents,
      const GURL& link_url,
      const GURL& target_url,
      LinkTarget target,
      const std::string& rel,
      int modifiers);

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

  BookmarkAppNavigationBrowserTest();
  ~BookmarkAppNavigationBrowserTest() override;

  void SetUp() override;
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownInProcessBrowserTestFixture() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

  void InstallTestBookmarkApp();
  void InstallOtherTestBookmarkApp();
  const Extension* InstallTestBookmarkApp(const std::string& app_host,
                                          const std::string& app_scope);

  Browser* OpenTestBookmarkApp();

  // Navigates the active tab in |browser| to the launching page.
  void NavigateToLaunchingPage(Browser* browser);

  // Checks that no new windows are opened after running |action| and that the
  // existing |browser| window is still the active one and navigated to
  // |target_url|. Returns true if there were no errors.
  bool TestActionDoesNotOpenAppWindow(Browser* browser,
                                      const GURL& target_url,
                                      base::OnceClosure action);

  // Checks that a new foreground tab is opened in an existing browser, that the
  // new tab's browser is in the foreground, and that |app_browser| didn't
  // navigate.
  void TestAppActionOpensForegroundTab(Browser* app_browser,
                                       const GURL& target_url,
                                       base::OnceClosure action);

  // Checks that no new windows are opened after running |action| and that the
  // main browser window is still the active one and navigated to |target_url|.
  // Returns true if there were no errors.
  bool TestTabActionDoesNotOpenAppWindow(const GURL& target_url,
                                         base::OnceClosure action);

  const net::EmbeddedTestServer& https_server() { return https_server_; }

 private:
  net::EmbeddedTestServer https_server_;
  // Similar to net::MockCertVerifier, but also updates the CertVerifier
  // used by the NetworkService.
  content::ContentMockCertVerifier cert_verifier_;
  const Extension* test_bookmark_app_;
  base::HistogramTester histogram_tester_;
};

}  // namespace test
}  // namespace extensions

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_BOOKMARK_APP_NAVIGATION_BROWSERTEST_H_
