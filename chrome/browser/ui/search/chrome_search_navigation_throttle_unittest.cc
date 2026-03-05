// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/chrome_search_navigation_throttle.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class ChromeSearchNavigationThrottleUnitTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeSearchNavigationThrottleUnitTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kInstantUsesSpareRenderer, true);
  }

  ChromeSearchNavigationThrottleUnitTest(
      const ChromeSearchNavigationThrottleUnitTest&) = delete;
  ChromeSearchNavigationThrottleUnitTest& operator=(
      const ChromeSearchNavigationThrottleUnitTest&) = delete;

  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents());
  }

  // Runs the throttle's WillStartRequest and returns the action.
  content::NavigationThrottle::ThrottleAction RunWillStartRequest(
      content::RenderFrameHost* host,
      const GURL& url,
      bool is_renderer_initiated = true) {
    content::MockNavigationHandle test_handle(url, host);
    test_handle.set_is_renderer_initiated(is_renderer_initiated);
    test_handle.set_source_site_instance(host->GetSiteInstance());
    content::MockNavigationThrottleRegistry test_registry(&test_handle);
    auto throttle =
        std::make_unique<ChromeSearchNavigationThrottle>(test_registry);
    return throttle->WillStartRequest().action();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Non-chrome-search URLs: throttle is not created, so navigation proceeds.
TEST_F(ChromeSearchNavigationThrottleUnitTest, NonChromeSearchUrlProceeds) {
  const GURL url("http://example.com");
  web_contents_tester()->NavigateAndCommit(url);
  EXPECT_EQ(main_rfh()->GetLastCommittedURL(), url);
}

// Browser-initiated chrome-search: throttle is not created, so navigation
// proceeds.
TEST_F(ChromeSearchNavigationThrottleUnitTest,
       BrowserInitiatedChromeSearchProceeds) {
  web_contents_tester()->NavigateAndCommit(GURL("http://example.com"));
  const GURL chrome_search_url("chrome-search://most-visited/title.html");
  web_contents_tester()->NavigateAndCommit(chrome_search_url);
  EXPECT_EQ(main_rfh()->GetLastCommittedURL(), chrome_search_url);
}

// Renderer-initiated chrome-search from a non-instant process is BLOCKED
TEST_F(ChromeSearchNavigationThrottleUnitTest,
       ChromeSearchFromNonInstantProcessBlocked) {
  web_contents_tester()->NavigateAndCommit(GURL("http://example.com"));
  const GURL chrome_search_url("chrome-search://most-visited/title.html");
  content::NavigationThrottle::ThrottleAction action = RunWillStartRequest(
      main_rfh(), chrome_search_url, /*is_renderer_initiated=*/true);
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST, action);
}

// Renderer-initiated chrome-search from an instant process PROCEEDs.
TEST_F(ChromeSearchNavigationThrottleUnitTest,
       ChromeSearchFromInstantProcessProceeds) {
  web_contents_tester()->NavigateAndCommit(GURL("http://example.com"));
  Profile* profile = Profile::FromBrowserContext(browser_context());
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(instant_service);
  instant_service->AddInstantProcess(main_rfh()->GetProcess());

  const GURL chrome_search_url("chrome-search://most-visited/title.html");
  content::NavigationThrottle::ThrottleAction action = RunWillStartRequest(
      main_rfh(), chrome_search_url, /*is_renderer_initiated=*/true);
  EXPECT_EQ(content::NavigationThrottle::PROCEED, action);
}
