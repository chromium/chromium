// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/lens/lens_side_panel_navigation_helper.h"

#include "base/logging.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/mock_navigation_handle.h"
#include "url/gurl.h"

namespace lens {
namespace {

class LensSidePanelNavigationHelperTest
    : public ChromeRenderViewHostTestHarness {
 private:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
  }
};

}  // namespace

TEST_F(LensSidePanelNavigationHelperTest,
       DontInitializeThrottleWithoutNavigationHelperInitialized) {
  content::MockNavigationHandle handle(GURL("http://www.foo.com/"), main_rfh());

  EXPECT_FALSE(LensSidePanelNavigationHelper::MaybeCreateThrottleFor(&handle));
}

TEST_F(LensSidePanelNavigationHelperTest,
       InitializeThrottleWhenNavigationHelperInitialized) {
  content::MockNavigationHandle handle(GURL("http://www.foo.com/"), main_rfh());
  LensSidePanelNavigationHelper::CreateForWebContents(
      handle.GetWebContents(), nullptr, "http://www.foo.com/");

  EXPECT_TRUE(LensSidePanelNavigationHelper::MaybeCreateThrottleFor(&handle));
}

TEST_F(LensSidePanelNavigationHelperTest,
       NavigationBlocksWhenUserClickOnDifferentDomain) {
  content::MockNavigationHandle handle(GURL("https://www.bar.com/"),
                                       main_rfh());
  LensSidePanelNavigationHelper::CreateForWebContents(
      handle.GetWebContents(), nullptr, "http://www.foo.com/");
  handle.set_page_transition(ui::PageTransition::PAGE_TRANSITION_LINK);

  auto throttle =
      LensSidePanelNavigationHelper::MaybeCreateThrottleFor(&handle);
  ASSERT_NE(throttle, nullptr);
  ASSERT_EQ(throttle.get()->WillStartRequest().action(),
            content::NavigationThrottle::CANCEL);
}

TEST_F(LensSidePanelNavigationHelperTest,
       NavigationContinuesWhenUserClicksOnSameDomain) {
  content::MockNavigationHandle handle(GURL("http://www.foo.com/bar/"),
                                       main_rfh());
  LensSidePanelNavigationHelper::CreateForWebContents(
      handle.GetWebContents(), nullptr, "http://www.foo.com/");
  handle.set_page_transition(ui::PageTransition::PAGE_TRANSITION_LINK);

  auto throttle =
      LensSidePanelNavigationHelper::MaybeCreateThrottleFor(&handle);
  ASSERT_NE(throttle, nullptr);
  ASSERT_EQ(throttle.get()->WillStartRequest().action(),
            content::NavigationThrottle::PROCEED);
}

TEST_F(LensSidePanelNavigationHelperTest,
       NavigationContinuesWhenUserClicksOnSameSubdomain) {
  content::MockNavigationHandle handle(GURL("http://bar.foo.com/"), main_rfh());
  LensSidePanelNavigationHelper::CreateForWebContents(
      handle.GetWebContents(), nullptr, "http://www.foo.com/");
  handle.set_page_transition(ui::PageTransition::PAGE_TRANSITION_LINK);

  auto throttle =
      LensSidePanelNavigationHelper::MaybeCreateThrottleFor(&handle);
  ASSERT_NE(throttle, nullptr);
  ASSERT_EQ(throttle.get()->WillStartRequest().action(),
            content::NavigationThrottle::PROCEED);
}

TEST_F(LensSidePanelNavigationHelperTest,
       NavigationBlocksWhenNonUserClickOnDifferentDomain) {
  content::MockNavigationHandle handle(GURL("http://www.bar.com/"), main_rfh());
  LensSidePanelNavigationHelper::CreateForWebContents(
      handle.GetWebContents(), nullptr, "http://www.foo.com/");
  handle.set_page_transition(ui::PageTransition::PAGE_TRANSITION_AUTO_SUBFRAME);

  auto throttle =
      LensSidePanelNavigationHelper::MaybeCreateThrottleFor(&handle);
  ASSERT_NE(throttle, nullptr);
  ASSERT_EQ(throttle.get()->WillStartRequest().action(),
            content::NavigationThrottle::CANCEL);
}

TEST_F(LensSidePanelNavigationHelperTest,
       NavigationContinuesWhenNonUserClickOnDifferentDomainButHasExpectation) {
  content::MockNavigationHandle handle(
      GURL("https://www.googleusercontent.com/"), main_rfh());
  LensSidePanelNavigationHelper::CreateForWebContents(
      handle.GetWebContents(), nullptr, "http://www.foo.com/");
  handle.set_page_transition(ui::PageTransition::PAGE_TRANSITION_AUTO_SUBFRAME);

  auto throttle =
      LensSidePanelNavigationHelper::MaybeCreateThrottleFor(&handle);
  ASSERT_NE(throttle, nullptr);
  ASSERT_EQ(throttle.get()->WillStartRequest().action(),
            content::NavigationThrottle::PROCEED);
}
}  // namespace lens
