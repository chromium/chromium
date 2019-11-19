// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace {

class NewTabPageNavigationThrottleTest : public InProcessBrowserTest {
 public:
  NewTabPageNavigationThrottleTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_test_server()->AddDefaultHandlers(GetChromeTestDataDir());
  }

  void SetNewTabPage(const std::string& ntp_url) {
    // Set the new tab page.
    local_ntp_test_utils::SetUserSelectedDefaultSearchProvider(
        browser()->profile(), https_test_server()->base_url().spec(), ntp_url);

    // Ensure we are using the newly set new_tab_url and won't be directed
    // to the local new tab page.
    TemplateURLService* service =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    search_test_utils::WaitForTemplateURLServiceToLoad(service);
    ASSERT_EQ(search::GetNewTabPageURL(browser()->profile()), ntp_url);
  }

  // Navigates to the New Tab Page and then returns the GURL that ultimately was
  // navigated to.
  GURL NavigateToNewTabPage() {
    ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
    content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    return contents->GetController().GetLastCommittedEntry()->GetURL();
  }

  net::EmbeddedTestServer* https_test_server() { return &https_test_server_; }

  net::EmbeddedTestServer https_test_server_;
};

IN_PROC_BROWSER_TEST_F(NewTabPageNavigationThrottleTest, NoThrottle) {
  ASSERT_TRUE(https_test_server()->Start());
  std::string ntp_url =
      https_test_server()->GetURL("/instant_extended.html").spec();
  SetNewTabPage(ntp_url);
  // A correct, 200-OK file works correctly.
  EXPECT_EQ(ntp_url, NavigateToNewTabPage());
}

IN_PROC_BROWSER_TEST_F(NewTabPageNavigationThrottleTest,
                       FailedRequestThrottle) {
  ASSERT_TRUE(https_test_server()->Start());
  SetNewTabPage(https_test_server()->GetURL("/instant_extended.html").spec());
  ASSERT_TRUE(https_test_server()->ShutdownAndWaitUntilComplete());
  // Failed navigation makes a redirect to the local NTP.
  EXPECT_EQ(chrome::kChromeSearchLocalNtpUrl, NavigateToNewTabPage());
}

IN_PROC_BROWSER_TEST_F(NewTabPageNavigationThrottleTest, LocalNewTabPage) {
  ASSERT_TRUE(https_test_server()->Start());
  SetNewTabPage(chrome::kChromeSearchLocalNtpUrl);
  // Already going to the local NTP, so we should arrive there as expected.
  EXPECT_EQ(chrome::kChromeSearchLocalNtpUrl, NavigateToNewTabPage());
}

IN_PROC_BROWSER_TEST_F(NewTabPageNavigationThrottleTest, 404Throttle) {
  ASSERT_TRUE(https_test_server()->Start());
  SetNewTabPage(https_test_server()->GetURL("/page404.html").spec());
  // 404 makes a redirect to the local NTP.
  EXPECT_EQ(chrome::kChromeSearchLocalNtpUrl, NavigateToNewTabPage());
}

IN_PROC_BROWSER_TEST_F(NewTabPageNavigationThrottleTest, 204Throttle) {
  ASSERT_TRUE(https_test_server()->Start());
  SetNewTabPage(https_test_server()->GetURL("/page204.html").spec());
  // 204 makes a redirect to the local NTP.
  EXPECT_EQ(chrome::kChromeSearchLocalNtpUrl, NavigateToNewTabPage());
}

}  // namespace
