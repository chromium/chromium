// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_navigation_throttle.h"

#include <memory>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/test_renderer_host.h"
#include "pdf/pdf_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace pdf {

namespace {

using ::testing::NiceMock;

class PdfNavigationThrottleTest : public content::RenderViewHostTestHarness {
 protected:
  GURL CreateStreamUrl(
      const char* token = "00000000-0000-0000-0000-000000000000") {
    return GURL(base::StrCat(
        {"chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/", token}));
  }

  content::RenderFrameHost* CreateChildFrame() {
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
    return content::RenderFrameHostTester::For(main_rfh())
        ->AppendChild("subframe");
  }

  void InitializeNavigationHandle(const GURL& url,
                                  content::RenderFrameHost* render_frame_host) {
    navigation_handle_ =
        std::make_unique<NiceMock<content::MockNavigationHandle>>(
            url, render_frame_host);
    navigation_handle_->set_initiator_origin(
        render_frame_host->GetLastCommittedOrigin());
  }

  std::unique_ptr<PdfNavigationThrottle> CreateNavigationThrottle(
      const GURL& url) {
    InitializeNavigationHandle(url, CreateChildFrame());
    return std::make_unique<PdfNavigationThrottle>(navigation_handle_.get());
  }

  base::test::ScopedFeatureList features_;
  std::unique_ptr<NiceMock<content::MockNavigationHandle>> navigation_handle_;
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
  NiceMock<content::MockWebContentsObserver> web_contents_observer(
      web_contents());

  EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            navigation_throttle->WillStartRequest().action());

  EXPECT_CALL(web_contents_observer, DidStartLoading());
  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(PdfNavigationThrottleUnseasonedEnabledTest,
       WillStartRequestDeleteContents) {
  auto navigation_throttle = CreateNavigationThrottle(CreateStreamUrl());
  NiceMock<content::MockWebContentsObserver> web_contents_observer(
      web_contents());

  EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            navigation_throttle->WillStartRequest().action());
  DeleteContents();

  EXPECT_CALL(web_contents_observer, DidStartLoading()).Times(0);
  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   run_loop.QuitClosure());
  run_loop.Run();
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
