// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/test/bookmark_app_navigation_browsertest.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/escape.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace {

const char kLaunchingPageHost[] = "launching-page.com";
const char kLaunchingPagePath[] = "/index.html";

const char kAppUrlHost[] = "app.com";
const char kOtherAppUrlHost[] = "other-app.com";
const char kAppScopePath[] = "/in_scope/";
const char kAppUrlPath[] = "/in_scope/index.html";
const char kInScopeUrlPath[] = "/in_scope/other.html";
const char kOutOfScopeUrlPath[] = "/out_of_scope/index.html";

const char kAppName[] = "Test app";

}  // anonymous namespace

namespace extensions {
namespace test {

// static
const char* BookmarkAppNavigationBrowserTest::GetLaunchingPageHost() {
  return kLaunchingPageHost;
}

// static
const char* BookmarkAppNavigationBrowserTest::GetLaunchingPagePath() {
  return kLaunchingPagePath;
}

// static
const char* BookmarkAppNavigationBrowserTest::GetAppUrlHost() {
  return kAppUrlHost;
}

// static
const char* BookmarkAppNavigationBrowserTest::GetOtherAppUrlHost() {
  return kOtherAppUrlHost;
}

// static
const char* BookmarkAppNavigationBrowserTest::GetAppScopePath() {
  return kAppScopePath;
}

// static
const char* BookmarkAppNavigationBrowserTest::GetAppUrlPath() {
  return kAppUrlPath;
}

// static
const char* BookmarkAppNavigationBrowserTest::GetInScopeUrlPath() {
  return kInScopeUrlPath;
}

// static
const char* BookmarkAppNavigationBrowserTest::GetOutOfScopeUrlPath() {
  return kOutOfScopeUrlPath;
}

// static
const char* BookmarkAppNavigationBrowserTest::GetAppName() {
  return kAppName;
}

// static
std::string BookmarkAppNavigationBrowserTest::CreateServerRedirect(
    const GURL& target_url) {
  const char* const kServerRedirectBase = "/server-redirect?";
  return kServerRedirectBase +
         net::EscapeQueryParamValue(target_url.spec(), false);
}

// static
std::unique_ptr<content::TestNavigationObserver>
BookmarkAppNavigationBrowserTest::GetTestNavigationObserver(
    const GURL& target_url) {
  auto observer = std::make_unique<content::TestNavigationObserver>(target_url);
  observer->WatchExistingWebContents();
  observer->StartWatchingNewWebContents();
  return observer;
}

// static
void BookmarkAppNavigationBrowserTest::ClickLinkWithModifiersAndWaitForURL(
    content::WebContents* web_contents,
    const GURL& link_url,
    const GURL& target_url,
    BookmarkAppNavigationBrowserTest::LinkTarget target,
    const std::string& rel,
    int modifiers) {
  auto observer = GetTestNavigationObserver(target_url);
  std::string script = base::StringPrintf(
      "(() => {"
      "const link = document.createElement('a');"
      "link.href = '%s';"
      "link.target = '%s';"
      "link.rel = '%s';"
      // Make a click target that covers the whole viewport.
      "const click_target = document.createElement('textarea');"
      "click_target.position = 'absolute';"
      "click_target.top = 0;"
      "click_target.left = 0;"
      "click_target.style.height = '100vh';"
      "click_target.style.width = '100vw';"
      "link.appendChild(click_target);"
      "document.body.appendChild(link);"
      "})();",
      link_url.spec().c_str(), target == LinkTarget::SELF ? "_self" : "_blank",
      rel.c_str());
  ASSERT_TRUE(content::ExecuteScript(web_contents, script));

  content::SimulateMouseClick(web_contents, modifiers,
                              blink::WebMouseEvent::Button::kLeft);

  observer->Wait();
}

// static
void BookmarkAppNavigationBrowserTest::ClickLinkAndWaitForURL(
    content::WebContents* web_contents,
    const GURL& link_url,
    const GURL& target_url,
    BookmarkAppNavigationBrowserTest::LinkTarget target,
    const std::string& rel) {
  ClickLinkWithModifiersAndWaitForURL(
      web_contents, link_url, target_url, target, rel,
      blink::WebInputEvent::Modifiers::kNoModifiers);
}

// static
void BookmarkAppNavigationBrowserTest::ClickLinkAndWait(
    content::WebContents* web_contents,
    const GURL& link_url,
    BookmarkAppNavigationBrowserTest::LinkTarget target,
    const std::string& rel) {
  ClickLinkAndWaitForURL(web_contents, link_url, link_url, target, rel);
}

BookmarkAppNavigationBrowserTest::BookmarkAppNavigationBrowserTest()
    : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

BookmarkAppNavigationBrowserTest::~BookmarkAppNavigationBrowserTest() = default;

void BookmarkAppNavigationBrowserTest::SetUp() {
  https_server_.AddDefaultHandlers(GetChromeTestDataDir());
  // Register a request handler that will return empty pages. Tests are
  // responsible for adding elements and firing events on these empty pages.
  https_server_.RegisterRequestHandler(
      base::BindRepeating([](const net::test_server::HttpRequest& request) {
        // Let the default request handlers handle redirections.
        if (request.GetURL().path() == "/server-redirect" ||
            request.GetURL().path() == "/client-redirect") {
          return std::unique_ptr<net::test_server::HttpResponse>();
        }
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_content_type("text/html");
        response->AddCustomHeader("Access-Control-Allow-Origin", "*");
        return static_cast<std::unique_ptr<net::test_server::HttpResponse>>(
            std::move(response));
      }));

  ExtensionBrowserTest::SetUp();
}

void BookmarkAppNavigationBrowserTest::SetUpInProcessBrowserTestFixture() {
  ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();
  cert_verifier_.SetUpInProcessBrowserTestFixture();
}

void BookmarkAppNavigationBrowserTest::TearDownInProcessBrowserTestFixture() {
  ExtensionBrowserTest::TearDownInProcessBrowserTestFixture();
  cert_verifier_.TearDownInProcessBrowserTestFixture();
}

void BookmarkAppNavigationBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  cert_verifier_.SetUpCommandLine(command_line);
}

void BookmarkAppNavigationBrowserTest::SetUpOnMainThread() {
  ExtensionBrowserTest::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
  // By default, all SSL cert checks are valid. Can be overridden in tests.
  cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
}

void BookmarkAppNavigationBrowserTest::InstallTestBookmarkApp() {
  test_bookmark_app_ =
      InstallTestBookmarkApp(GetAppUrlHost(), GetAppScopePath());
}

void BookmarkAppNavigationBrowserTest::InstallOtherTestBookmarkApp() {
  InstallTestBookmarkApp(GetOtherAppUrlHost(), GetAppScopePath());
}

const Extension* BookmarkAppNavigationBrowserTest::InstallTestBookmarkApp(
    const std::string& app_host,
    const std::string& app_scope) {
  if (!https_server_.Started()) {
    CHECK(https_server_.Start());
  }

  WebApplicationInfo web_app_info;
  web_app_info.app_url = https_server_.GetURL(app_host, GetAppUrlPath());
  web_app_info.scope = https_server_.GetURL(app_host, app_scope);
  web_app_info.title = base::UTF8ToUTF16(GetAppName());
  web_app_info.description = base::UTF8ToUTF16("Test description");
  web_app_info.open_as_window = true;

  return InstallBookmarkApp(web_app_info);
}

Browser* BookmarkAppNavigationBrowserTest::OpenTestBookmarkApp() {
  GURL app_url = https_server_.GetURL(GetAppUrlHost(), GetAppUrlPath());
  auto observer = GetTestNavigationObserver(app_url);
  Browser* app_browser = LaunchAppBrowser(test_bookmark_app_);
  observer->Wait();

  return app_browser;
}

void BookmarkAppNavigationBrowserTest::NavigateToLaunchingPage(
    Browser* browser) {
  ui_test_utils::NavigateToURL(
      browser,
      https_server_.GetURL(GetLaunchingPageHost(), GetLaunchingPagePath()));
}

bool BookmarkAppNavigationBrowserTest::TestActionDoesNotOpenAppWindow(
    Browser* browser,
    const GURL& target_url,
    base::OnceClosure action) {
  content::WebContents* initial_tab =
      browser->tab_strip_model()->GetActiveWebContents();
  int num_tabs = browser->tab_strip_model()->count();
  size_t num_browsers = chrome::GetBrowserCount(browser->profile());

  std::move(action).Run();

  EXPECT_EQ(num_tabs, browser->tab_strip_model()->count());
  EXPECT_EQ(num_browsers, chrome::GetBrowserCount(browser->profile()));
  EXPECT_EQ(browser, chrome::FindLastActive());
  EXPECT_EQ(initial_tab, browser->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(target_url, initial_tab->GetLastCommittedURL());

  return !HasFailure();
}

void BookmarkAppNavigationBrowserTest::TestAppActionOpensForegroundTab(
    Browser* app_browser,
    const GURL& target_url,
    base::OnceClosure action) {
  size_t num_browsers = chrome::GetBrowserCount(profile());
  int num_tabs_browser = browser()->tab_strip_model()->count();
  int num_tabs_app_browser = app_browser->tab_strip_model()->count();

  content::WebContents* app_web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL initial_app_url = app_web_contents->GetLastCommittedURL();
  GURL initial_tab_url = initial_tab->GetLastCommittedURL();

  std::move(action).Run();

  EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));

  EXPECT_EQ(browser(), chrome::FindLastActive());

  EXPECT_EQ(++num_tabs_browser, browser()->tab_strip_model()->count());
  EXPECT_EQ(num_tabs_app_browser, app_browser->tab_strip_model()->count());

  EXPECT_EQ(initial_app_url, app_web_contents->GetLastCommittedURL());

  content::WebContents* new_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(initial_tab, new_tab);
  EXPECT_EQ(target_url, new_tab->GetLastCommittedURL());
}

bool BookmarkAppNavigationBrowserTest::TestTabActionDoesNotOpenAppWindow(
    const GURL& target_url,
    base::OnceClosure action) {
  return TestActionDoesNotOpenAppWindow(browser(), target_url,
                                        std::move(action));
}

}  // namespace test
}  // namespace extensions
