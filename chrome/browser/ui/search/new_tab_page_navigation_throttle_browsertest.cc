// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/ntp_test_utils.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
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
    ntp_test_utils::SetUserSelectedDefaultSearchProvider(
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
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                             GURL(chrome::kChromeUINewTabURL)));
    return web_contents()->GetController().GetLastCommittedEntry()->GetURL();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
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
  const GURL instant_ntp_url =
      https_test_server()->GetURL("/instant_extended.html");
  SetNewTabPage(instant_ntp_url.spec());
  ASSERT_TRUE(https_test_server()->ShutdownAndWaitUntilComplete());

  // Helper to assert that the failed request to `instant_ntp_url` never commits
  // an error page. This doesn't simply use `TestNavigationManager` since that
  // automatically pauses navigations, which is not needed or useful here.
  class FailedRequestObserver : public content::WebContentsObserver {
   public:
    explicit FailedRequestObserver(content::WebContents* contents,
                                   const GURL& instant_ntp_url)
        : WebContentsObserver(contents), instant_ntp_url_(instant_ntp_url) {}

    // WebContentsObserver overrides:
    void DidFinishNavigation(content::NavigationHandle* handle) override {
      if (handle->GetURL() != instant_ntp_url_)
        return;

      did_finish_ = true;
      did_commit_ = handle->HasCommitted();
    }

    bool did_finish() const { return did_finish_; }
    bool did_commit() const { return did_commit_; }

   private:
    const GURL instant_ntp_url_;
    bool did_finish_ = false;
    bool did_commit_ = false;
  };

  FailedRequestObserver observer(web_contents(), instant_ntp_url);
  // Failed navigation makes a redirect to the 3P WebUI NTP.
  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL, NavigateToNewTabPage());
  EXPECT_TRUE(observer.did_finish());
  EXPECT_FALSE(observer.did_commit());
}

IN_PROC_BROWSER_TEST_F(NewTabPageNavigationThrottleTest, LocalNewTabPage) {
  ASSERT_TRUE(https_test_server()->Start());
  // This URL is not https so it will default to the 3P WebUI NTP.
  SetNewTabPage(chrome::kChromeUINewTabPageThirdPartyURL);
  // Already going to the 3P WebUI NTP, so we should arrive there as expected.
  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL, NavigateToNewTabPage());
}

IN_PROC_BROWSER_TEST_F(NewTabPageNavigationThrottleTest, 404Throttle) {
  ASSERT_TRUE(https_test_server()->Start());
  SetNewTabPage(https_test_server()->GetURL("/page404.html").spec());
  // 404 makes a redirect to the 3P WebUI NTP.
  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL, NavigateToNewTabPage());
}

IN_PROC_BROWSER_TEST_F(NewTabPageNavigationThrottleTest, 204Throttle) {
  ASSERT_TRUE(https_test_server()->Start());
  SetNewTabPage(https_test_server()->GetURL("/page204.html").spec());
  // 204 makes a redirect to the 3P WebUI NTP.
  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL, NavigateToNewTabPage());
}

class OverrideNavigationParamsObserver : public content::WebContentsObserver {
 public:
  explicit OverrideNavigationParamsObserver(content::WebContents* contents)
      : WebContentsObserver(contents) {}

  // WebContentsObserver overrides:
  void DidFinishNavigation(content::NavigationHandle* handle) override {
    EXPECT_TRUE(handle);

    // Check the values that are changed in OverrideNavigationParams.
    EXPECT_EQ(std::nullopt, handle->GetInitiatorOrigin());
    EXPECT_FALSE(handle->IsRendererInitiated());
    ui::PageTransitionCoreTypeIs(handle->GetPageTransition(),
                                 ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  }
};

// Check that ChromeContentBrowserClient::OverrideNavigationParams behaves
// correctly when navigating from a custom 3P NTP with an HTTPS scheme.
// OverrideNavigationParams changes the params on renderer initiated navigations
// from the NTP. It identifies a page as an NTP by using the site URL, not the
// lock URL, of the initiator process.
IN_PROC_BROWSER_TEST_F(NewTabPageNavigationThrottleTest,
                       OverrideNavigationParams_ThirdPartyNTP) {
  ASSERT_TRUE(https_test_server()->Start());
  std::string ntp_url =
      https_test_server()->GetURL("/instant_extended.html").spec();
  SetNewTabPage(ntp_url);
  EXPECT_EQ(ntp_url, NavigateToNewTabPage());

  const GURL page_url = https_test_server()->GetURL("/simple.html");
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  OverrideNavigationParamsObserver observer(web_contents);
  EXPECT_TRUE(content::NavigateToURLFromRenderer(web_contents, page_url));
}

// Check that ChromeContentBrowserClient::OverrideNavigationParams behaves
// correctly when navigating from a chrome:// NTP.
// OverrideNavigationParams changes the params on renderer initiated navigations
// from the NTP. It identifies a page as an NTP by using the site URL, not the
// lock URL, of the initiator process. This test uses a chrome:// URL for the
// NTP, so the lock and site URLs are the same.
IN_PROC_BROWSER_TEST_F(NewTabPageNavigationThrottleTest,
                       OverrideNavigationParams_ChromeURLNTP) {
  ASSERT_TRUE(https_test_server()->Start());
  SetNewTabPage(chrome::kChromeUINewTabPageThirdPartyURL);
  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL, NavigateToNewTabPage());

  const GURL page_url = https_test_server()->GetURL("/simple.html");
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  OverrideNavigationParamsObserver observer(web_contents);
  EXPECT_TRUE(content::NavigateToURLFromRenderer(web_contents, page_url));
}

class NewTabPageNavigationThrottlePrerenderTest
    : public NewTabPageNavigationThrottleTest {
 public:
  NewTabPageNavigationThrottlePrerenderTest()
      : prerender_test_helper_(base::BindRepeating(
            &NewTabPageNavigationThrottlePrerenderTest::web_contents,
            base::Unretained(this))) {}
  ~NewTabPageNavigationThrottlePrerenderTest() override = default;
  NewTabPageNavigationThrottlePrerenderTest(
      const NewTabPageNavigationThrottlePrerenderTest&) = delete;

  NewTabPageNavigationThrottlePrerenderTest& operator=(
      const NewTabPageNavigationThrottlePrerenderTest&) = delete;

  void SetUp() override {
    prerender_test_helper_.RegisterServerRequestMonitor(https_test_server());
    NewTabPageNavigationThrottleTest::SetUp();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_test_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_test_helper_;
};

IN_PROC_BROWSER_TEST_F(NewTabPageNavigationThrottlePrerenderTest,
                       PrerenderingShouldNotAffectTitle) {
  ASSERT_TRUE(https_test_server()->Start());
  GURL ntp_url = https_test_server()->GetURL("/instant_extended.html");

  GURL title_url = https_test_server()->GetURL("/title2.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), title_url));
  EXPECT_EQ(u"Title Of Awesomeness", web_contents()->GetTitle());

  // Load a page in the prerendering.
  const content::FrameTreeNodeId host_id =
      prerender_test_helper().AddPrerender(ntp_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  EXPECT_FALSE(host_observer.was_activated());

  // Prerendering should not change the title of the web contents.
  EXPECT_EQ(u"Title Of Awesomeness", web_contents()->GetTitle());

  SetNewTabPage(ntp_url.spec());

  // Now `ntp_url` has an effective URL
  // (chrome-search://remote-ntp/instant_extended.html), so this navigation
  // should not activate the prerendered page.
  prerender_test_helper().NavigatePrimaryPage(ntp_url);
  EXPECT_FALSE(host_observer.was_activated());

  // The title should be changed after navigation.
  EXPECT_NE(u"Title Of Awesomeness", web_contents()->GetTitle());
}

class NewTabPageNavigationThrottleFencedFrameTest
    : public NewTabPageNavigationThrottleTest {
 public:
  NewTabPageNavigationThrottleFencedFrameTest() = default;
  ~NewTabPageNavigationThrottleFencedFrameTest() override = default;
  NewTabPageNavigationThrottleFencedFrameTest(
      const NewTabPageNavigationThrottleFencedFrameTest&) = delete;

  NewTabPageNavigationThrottleFencedFrameTest& operator=(
      const NewTabPageNavigationThrottleFencedFrameTest&) = delete;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(NewTabPageNavigationThrottleFencedFrameTest,
                       FencedFrameDoesNotResetNewTabStartTime) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(https_test_server()->Start());
  GURL ntp_url = https_test_server()->GetURL("/instant_extended.html");
  SetNewTabPage(ntp_url.spec());

  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(web_contents());
  core_tab_helper->set_new_tab_start_time(base::TimeTicks().Now());

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), ntp_url));
  EXPECT_TRUE(core_tab_helper->new_tab_start_time().is_null());

  core_tab_helper->set_new_tab_start_time(base::TimeTicks().Now());
  GURL fenced_frame_url =
      https_test_server()->GetURL("/fenced_frames/title1.html");
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), fenced_frame_url);
  EXPECT_NE(nullptr, fenced_frame_host);
  EXPECT_FALSE(core_tab_helper->new_tab_start_time().is_null());
}

IN_PROC_BROWSER_TEST_F(NewTabPageNavigationThrottleFencedFrameTest,
                       FencedFrameShouldNotAffectTitle) {
  ASSERT_TRUE(https_test_server()->Start());
  GURL ntp_url = https_test_server()->GetURL("/instant_extended.html");
  SetNewTabPage(ntp_url.spec());

  GURL title_url = https_test_server()->GetURL("/title2.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), title_url));
  EXPECT_EQ(u"Title Of Awesomeness", web_contents()->GetTitle());

  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), ntp_url);
  EXPECT_NE(nullptr, fenced_frame_host);
  // Fenced frames should not update the title of the web contents.
  EXPECT_EQ(u"Title Of Awesomeness", web_contents()->GetTitle());
}

}  // namespace
