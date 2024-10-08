// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_navigation_throttle.h"

#include <memory>
#include <utility>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "components/pdf/browser/fake_pdf_stream_delegate.h"
#include "components/pdf/browser/pdf_stream_delegate.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace pdf {

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class PdfNavigationThrottleTest : public content::RenderViewHostTestHarness {
 protected:
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
    navigation_handle_->set_source_site_instance(
        render_frame_host->GetSiteInstance());
  }

  std::unique_ptr<PdfNavigationThrottle> CreateNavigationThrottle(
      const GURL& url,
      content::RenderFrameHost* frame) {
    InitializeNavigationHandle(url, frame);
    return std::make_unique<PdfNavigationThrottle>(navigation_handle_.get(),
                                                   std::move(stream_delegate_));
  }

  GURL stream_url() const {
    return GURL(FakePdfStreamDelegate::kDefaultStreamUrl);
  }

  GURL original_url() const {
    return GURL(FakePdfStreamDelegate::kDefaultOriginalUrl);
  }

  std::unique_ptr<FakePdfStreamDelegate> stream_delegate_ =
      std::make_unique<FakePdfStreamDelegate>();

  std::unique_ptr<NiceMock<content::MockNavigationHandle>> navigation_handle_;
};

}  // namespace

TEST_F(PdfNavigationThrottleTest, WillStartRequest) {
  auto navigation_throttle =
      CreateNavigationThrottle(stream_url(), CreateChildFrame());
  NiceMock<content::MockWebContentsObserver> web_contents_observer(
      web_contents());

  EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            navigation_throttle->WillStartRequest().action());

  EXPECT_CALL(web_contents_observer, DidStartLoading());
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  auto navigation_simulator = content::NavigationSimulator::CreateFromPending(
      web_contents()->GetController());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      ui::PAGE_TRANSITION_AUTO_SUBFRAME,
      navigation_simulator->GetNavigationHandle()->GetPageTransition()));

  EXPECT_CALL(web_contents_observer, DidFinishLoad(_, original_url()));
  navigation_simulator->Commit();
}

TEST_F(PdfNavigationThrottleTest, WillStartRequestForMainFrame) {
  auto navigation_throttle = CreateNavigationThrottle(stream_url(), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_throttle->WillStartRequest().action());
}

TEST_F(PdfNavigationThrottleTest, WillStartRequestDeleteContents) {
  auto navigation_throttle =
      CreateNavigationThrottle(stream_url(), CreateChildFrame());
  NiceMock<content::MockWebContentsObserver> web_contents_observer(
      web_contents());

  EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            navigation_throttle->WillStartRequest().action());
  DeleteContents();

  EXPECT_CALL(web_contents_observer, DidStartLoading()).Times(0);
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(PdfNavigationThrottleTest, WillStartRequestNoStreamInfo) {
  stream_delegate_->clear_stream_info();
  auto navigation_throttle =
      CreateNavigationThrottle(stream_url(), CreateChildFrame());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_throttle->WillStartRequest().action());
}

TEST_F(PdfNavigationThrottleTest,
       WillStartRequestShouldAllowPdfFrameNavigationFalse) {
  stream_delegate_->clear_stream_info();
  stream_delegate_->set_should_allow_pdf_frame_navigation(false);
  auto navigation_throttle =
      CreateNavigationThrottle(stream_url(), CreateChildFrame());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST,
            navigation_throttle->WillStartRequest().action());
}

TEST_F(PdfNavigationThrottleTest, WillStartRequestOtherUrl) {
  auto navigation_throttle = CreateNavigationThrottle(
      GURL("https://example.test"), CreateChildFrame());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_throttle->WillStartRequest().action());
}

}  // namespace pdf
