// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/policy/pre_redirection_url_observer.h"

#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace {

const char kURL1[] = "https://gurl1.example.test";
const char kURL2[] = "https://gurl2.example.test";
const char kURL3[] = "https://gurl3.example.test";

}  // namespace

namespace webapps {

class PreRedirectionURLObserverTest : public BrowserWithTestWindowTest {
 public:
  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* main_frame() const {
    return web_contents()->GetPrimaryMainFrame();
  }

  PreRedirectionURLObserver* observer() const {
    return PreRedirectionURLObserver::FromWebContents(web_contents());
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    // Create a test `WebContents`. This does not use `AddTab` as that creates a
    // real `WebContents` (with an observer) and navigate it. That will
    // interfere with this test's mock navigations.
    browser()->tab_strip_model()->AppendWebContents(
        content::WebContentsTester::CreateTestWebContents(profile(),
                                                          /*instance=*/nullptr),
        /*foreground=*/true);
    PreRedirectionURLObserver::CreateForWebContents(web_contents());
  }
};

TEST_F(PreRedirectionURLObserverTest, NoNavigation) {
  EXPECT_TRUE(observer()->last_url().is_empty());
}

TEST_F(PreRedirectionURLObserverTest, ThreeNavigations) {
  GURL url1(kURL1);
  content::MockNavigationHandle handle(url1, main_frame());
  handle.set_redirect_chain(std::vector<GURL>{url1});
  handle.set_is_in_primary_main_frame(true);
  handle.set_is_same_document(false);
  observer()->DidFinishNavigation(&handle);
  EXPECT_EQ(observer()->last_url(), url1);
  GURL url2(kURL2);
  handle.set_url(url2);
  handle.set_redirect_chain(std::vector<GURL>{url2});
  observer()->DidFinishNavigation(&handle);
  EXPECT_EQ(observer()->last_url(), url2);
  GURL url3(kURL3);
  handle.set_url(url3);
  handle.set_redirect_chain(std::vector<GURL>{url3});
  observer()->DidFinishNavigation(&handle);
  EXPECT_EQ(observer()->last_url(), url3);
}

TEST_F(PreRedirectionURLObserverTest, OneNavigationTwoRedirects) {
  GURL url1(kURL1);
  GURL url2(kURL2);
  GURL url3(kURL3);
  content::MockNavigationHandle handle(url1, main_frame());
  handle.set_redirect_chain(std::vector<GURL>{url1});
  handle.set_is_in_primary_main_frame(true);
  handle.set_is_same_document(false);
  observer()->DidFinishNavigation(&handle);
  EXPECT_EQ(observer()->last_url(), url1);

  handle.set_url(url2);
  handle.set_redirect_chain(std::vector<GURL>{url1, url2});
  observer()->DidFinishNavigation(&handle);
  EXPECT_EQ(observer()->last_url(), url1);

  handle.set_url(url3);
  handle.set_redirect_chain(std::vector<GURL>{url1, url2, url3});
  observer()->DidFinishNavigation(&handle);
  EXPECT_EQ(observer()->last_url(), url1);
}

TEST_F(PreRedirectionURLObserverTest, OneNavigationTwoSubframeNavigations) {
  GURL url1(kURL1);
  GURL url2(kURL2);
  GURL url3(kURL3);
  content::MockNavigationHandle handle(url1, main_frame());
  handle.set_redirect_chain(std::vector<GURL>{url1});
  handle.set_is_in_primary_main_frame(true);
  handle.set_is_same_document(false);
  observer()->DidFinishNavigation(&handle);
  EXPECT_EQ(observer()->last_url(), url1);

  handle.set_url(url2);
  handle.set_redirect_chain(std::vector<GURL>{url2});
  handle.set_is_in_primary_main_frame(false);
  observer()->DidFinishNavigation(&handle);
  EXPECT_EQ(observer()->last_url(), url1);

  handle.set_url(url3);
  handle.set_redirect_chain(std::vector<GURL>{url3});
  handle.set_is_in_primary_main_frame(false);
  observer()->DidFinishNavigation(&handle);
  EXPECT_EQ(observer()->last_url(), url1);
}

TEST_F(PreRedirectionURLObserverTest, OneNavigationTwoSameDocumentNavigations) {
  GURL url1(kURL1);
  GURL url2(kURL2);
  GURL url3(kURL3);
  content::MockNavigationHandle handle(url1, main_frame());
  handle.set_redirect_chain(std::vector<GURL>{url1});
  handle.set_is_in_primary_main_frame(true);
  handle.set_is_same_document(false);
  observer()->DidFinishNavigation(&handle);
  EXPECT_EQ(observer()->last_url(), url1);

  handle.set_url(url2);
  handle.set_redirect_chain(std::vector<GURL>{url2});
  handle.set_is_same_document(true);
  observer()->DidFinishNavigation(&handle);
  EXPECT_EQ(observer()->last_url(), url1);

  handle.set_url(url3);
  handle.set_redirect_chain(std::vector<GURL>{url3});
  handle.set_is_same_document(true);
  observer()->DidFinishNavigation(&handle);
  EXPECT_EQ(observer()->last_url(), url1);
}

TEST_F(PreRedirectionURLObserverTest, ManyMixedNavigations) {
  GURL url1(kURL1);
  GURL url2(kURL2);
  GURL url3(kURL3);
  content::MockNavigationHandle handle(url1, main_frame());
  handle.set_redirect_chain(std::vector<GURL>{url1});
  handle.set_is_in_primary_main_frame(true);
  handle.set_is_same_document(false);
  observer()->DidFinishNavigation(&handle);
  EXPECT_EQ(observer()->last_url(), url1);

  handle.set_url(url2);
  handle.set_redirect_chain(std::vector<GURL>{url2});
  handle.set_is_in_primary_main_frame(false);
  handle.set_is_same_document(false);
  observer()->DidFinishNavigation(&handle);
  EXPECT_EQ(observer()->last_url(), url1);

  handle.set_url(url2);
  handle.set_redirect_chain(std::vector<GURL>{url2});
  handle.set_is_in_primary_main_frame(true);
  handle.set_is_same_document(false);
  observer()->DidFinishNavigation(&handle);
  EXPECT_EQ(observer()->last_url(), url2);

  handle.set_url(url3);
  handle.set_redirect_chain(std::vector<GURL>{url3});
  handle.set_is_in_primary_main_frame(true);
  handle.set_is_same_document(false);
  observer()->DidFinishNavigation(&handle);
  EXPECT_EQ(observer()->last_url(), url3);

  handle.set_url(url3);
  handle.set_redirect_chain(std::vector<GURL>{url1, url2, url3});
  handle.set_is_in_primary_main_frame(true);
  handle.set_is_same_document(false);
  observer()->DidFinishNavigation(&handle);
  EXPECT_EQ(observer()->last_url(), url1);

  handle.set_url(url3);
  handle.set_redirect_chain(std::vector<GURL>{url3});
  handle.set_is_in_primary_main_frame(true);
  handle.set_is_same_document(true);
  observer()->DidFinishNavigation(&handle);
  EXPECT_EQ(observer()->last_url(), url1);

  handle.set_url(url3);
  handle.set_redirect_chain(std::vector<GURL>{url3});
  handle.set_is_in_primary_main_frame(true);
  handle.set_is_same_document(false);
  observer()->DidFinishNavigation(&handle);
  EXPECT_EQ(observer()->last_url(), url3);
}

}  // namespace webapps
