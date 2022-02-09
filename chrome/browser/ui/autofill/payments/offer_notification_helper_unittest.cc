// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/offer_notification_helper.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace autofill {

namespace {

class TestOfferNotificationHelper : public OfferNotificationHelper {
 public:
  static void CreateForTesting(content::WebContents* web_contents) {
    web_contents->SetUserData(
        UserDataKey(),
        std::make_unique<TestOfferNotificationHelper>(web_contents));
  }

  explicit TestOfferNotificationHelper(content::WebContents* web_contents)
      : OfferNotificationHelper(web_contents) {}
};

}  // namespace

class OfferNotificationHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    TestOfferNotificationHelper::CreateForTesting(web_contents());
  }

 protected:
  TestOfferNotificationHelper* helper() {
    return static_cast<TestOfferNotificationHelper*>(
        TestOfferNotificationHelper::FromWebContents(web_contents()));
  }
};

TEST_F(OfferNotificationHelperTest, DisplayOfferNotificationForNewTab) {
  GURL originalUrl = GURL("https://www.example.com/first/");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             originalUrl);

  // For a newly created tab, since the existing set of origins will be empty,
  // always show the offer notification.
  EXPECT_FALSE(helper()->OfferNotificationHasAlreadyBeenShown());
}

TEST_F(OfferNotificationHelperTest,
       DoNotDisplayOfferNotificationForSameOrigin) {
  GURL originalUrl = GURL("https://www.example.com/first/");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             originalUrl);
  EXPECT_FALSE(helper()->OfferNotificationHasAlreadyBeenShown());
  // Notify the helper that the offer notification was displayed for the
  // originalUrl.
  helper()->OnDisplayOfferNotification(
      {originalUrl.DeprecatedGetOriginAsURL()});

  // Navigate to another URL with the same origin as the originalUrl
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.example.com/second/"));

  EXPECT_TRUE(helper()->OfferNotificationHasAlreadyBeenShown());
}

TEST_F(OfferNotificationHelperTest, DoNotDisplayOfferNotificationForReload) {
  GURL originalUrl = GURL("https://www.example.com/first/");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             originalUrl);
  EXPECT_FALSE(helper()->OfferNotificationHasAlreadyBeenShown());
  // Notify the helper that the offer notification was displayed for the
  // originalUrl.
  helper()->OnDisplayOfferNotification(
      {originalUrl.DeprecatedGetOriginAsURL()});

  // Reload the current page.
  content::NavigationSimulator::Reload(web_contents());

  EXPECT_TRUE(helper()->OfferNotificationHasAlreadyBeenShown());
}

class OfferNotificationHelperPrerenderTest
    : public OfferNotificationHelperTest {
 public:
  OfferNotificationHelperPrerenderTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kPrerender2},
        // This feature is to run test on any bot.
        {blink::features::kPrerender2MemoryControls});
  }
  ~OfferNotificationHelperPrerenderTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(OfferNotificationHelperPrerenderTest,
       DoNotDisplayOfferNotificationForPrerender) {
  const GURL& original_url = GURL("https://www.example.com/first/");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             original_url);
  EXPECT_FALSE(helper()->OfferNotificationHasAlreadyBeenShown());
  // Notify the helper that the offer notification was displayed for the
  // primary page
  helper()->OnDisplayOfferNotification(
      {original_url.DeprecatedGetOriginAsURL()});

  // Start a prerender and navigate the test page.
  const GURL& prerender_url = GURL("https://www.example.com/second/");
  content::RenderFrameHost* prerender_frame =
      content::WebContentsTester::For(web_contents())
          ->AddPrerenderAndCommitNavigation(prerender_url);
  ASSERT_EQ(prerender_frame->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kPrerendering);

  // Ensure the prerender navigation cannot show the offer notification.
  EXPECT_TRUE(helper()->OfferNotificationHasAlreadyBeenShown());

  // Activate the prerendered page.
  content::NavigationSimulator::CreateRendererInitiated(
      prerender_url, web_contents()->GetMainFrame())
      ->Commit();

  // Ensure the activated page cannot show the offer notification since it has
  // the same-origin as the original url. Prerender doesn't allow the
  // cross-origin for now.
  EXPECT_TRUE(helper()->OfferNotificationHasAlreadyBeenShown());
}

}  // namespace autofill
