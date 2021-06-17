// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_navigation_throttle.h"

#include <memory>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "pdf/pdf_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace pdf {

namespace {

class PdfNavigationThrottleTest : public content::RenderViewHostTestHarness {
 protected:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(),
        GURL("chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html"));
  }

  GURL CreateStreamUrl(
      const char* token = "00000000-0000-0000-0000-000000000000") {
    return GURL(base::StrCat(
        {"chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/", token}));
  }

  content::RenderFrameHost* CreateChildFrame() {
    return content::RenderFrameHostTester::For(main_rfh())
        ->AppendChild("subframe");
  }

  void InitializeNavigationHandle(const GURL& url,
                                  content::RenderFrameHost* render_frame_host) {
    navigation_handle_ =
        std::make_unique<content::MockNavigationHandle>(url, render_frame_host);
  }

  std::unique_ptr<PdfNavigationThrottle> CreateNavigationThrottle(
      const GURL& url) {
    InitializeNavigationHandle(url, CreateChildFrame());
    return std::make_unique<PdfNavigationThrottle>(navigation_handle_.get());
  }

  base::test::ScopedFeatureList features_;
  std::unique_ptr<content::MockNavigationHandle> navigation_handle_;
};

class PdfNavigationThrottleUnseasonedDisabledTest
    : public PdfNavigationThrottleTest {
 protected:
  PdfNavigationThrottleUnseasonedDisabledTest() {
    features_.InitAndDisableFeature(chrome_pdf::features::kPdfUnseasoned);
  }
};

class PdfNavigationThrottleUnseasonedEnabledTest
    : public PdfNavigationThrottleTest {
 protected:
  PdfNavigationThrottleUnseasonedEnabledTest() {
    features_.InitAndEnableFeature(chrome_pdf::features::kPdfUnseasoned);
  }
};

}  // namespace

TEST_F(PdfNavigationThrottleUnseasonedDisabledTest, MaybeCreateThrottleFor) {
  InitializeNavigationHandle(CreateStreamUrl(), CreateChildFrame());
  EXPECT_FALSE(
      PdfNavigationThrottle::MaybeCreateThrottleFor(navigation_handle_.get()));
}

TEST_F(PdfNavigationThrottleUnseasonedEnabledTest, MaybeCreateThrottleFor) {
  InitializeNavigationHandle(CreateStreamUrl(), CreateChildFrame());
  EXPECT_TRUE(
      PdfNavigationThrottle::MaybeCreateThrottleFor(navigation_handle_.get()));
}

TEST_F(PdfNavigationThrottleUnseasonedEnabledTest,
       MaybeCreateThrottleForMainFrame) {
  InitializeNavigationHandle(CreateStreamUrl(), main_rfh());
  EXPECT_FALSE(
      PdfNavigationThrottle::MaybeCreateThrottleFor(navigation_handle_.get()));
}

TEST_F(PdfNavigationThrottleUnseasonedEnabledTest, WillStartRequest) {
  auto navigation_throttle = CreateNavigationThrottle(CreateStreamUrl());
  EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            navigation_throttle->WillStartRequest().action());
}

TEST_F(PdfNavigationThrottleUnseasonedEnabledTest, WillStartRequestOtherUrl) {
  auto navigation_throttle =
      CreateNavigationThrottle(GURL("https://example.test"));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_throttle->WillStartRequest().action());
}

TEST_F(PdfNavigationThrottleUnseasonedEnabledTest,
       WillStartRequestOtherExtensionUrl) {
  auto navigation_throttle = CreateNavigationThrottle(
      GURL("chrome-extension://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/"
           "00000000-0000-0000-0000-000000000000"));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_throttle->WillStartRequest().action());
}

TEST_F(PdfNavigationThrottleUnseasonedEnabledTest,
       WillStartRequestMultiplePathComponentUrl) {
  auto navigation_throttle =
      CreateNavigationThrottle(CreateStreamUrl("multiple/components"));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_throttle->WillStartRequest().action());
}

TEST_F(PdfNavigationThrottleUnseasonedEnabledTest,
       WillStartRequestFileExtensionUrl) {
  auto navigation_throttle =
      CreateNavigationThrottle(CreateStreamUrl("index.html"));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_throttle->WillStartRequest().action());
}

}  // namespace pdf
