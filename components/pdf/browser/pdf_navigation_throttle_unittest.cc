// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_navigation_throttle.h"

#include <memory>
#include <utility>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/pdf/browser/pdf_stream_delegate.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "pdf/pdf_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace pdf {

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class MockPdfStreamDelegate : public PdfStreamDelegate {
 public:
  MOCK_METHOD(absl::optional<StreamInfo>,
              GetStreamInfo,
              (content::WebContents * contents),
              (override));
};

class PdfNavigationThrottleTest : public content::RenderViewHostTestHarness {
 protected:
  PdfNavigationThrottleTest() {
    ON_CALL(*stream_delegate_, GetStreamInfo(_))
        .WillByDefault(Return(stream_info_));
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
    content::RenderFrameHost* child_frame = CreateChildFrame();
    InitializeNavigationHandle(url, child_frame);
    ON_CALL(*navigation_handle_, GetFrameTreeNodeId())
        .WillByDefault(Return(child_frame->GetFrameTreeNodeId()));
    return std::make_unique<PdfNavigationThrottle>(navigation_handle_.get(),
                                                   std::move(stream_delegate_));
  }

  const GURL& stream_url() const { return stream_info_.stream_url; }
  const GURL& original_url() const { return stream_info_.original_url; }

  base::test::ScopedFeatureList features_;

  PdfStreamDelegate::StreamInfo stream_info_{
      .stream_url = GURL("chrome-extension://id/stream-url"),
      .original_url = GURL("https://example.test/fake.pdf"),
  };
  std::unique_ptr<NiceMock<MockPdfStreamDelegate>> stream_delegate_ =
      std::make_unique<NiceMock<MockPdfStreamDelegate>>();

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
  InitializeNavigationHandle(stream_url(), CreateChildFrame());
  EXPECT_FALSE(PdfNavigationThrottle::MaybeCreateThrottleFor(
      navigation_handle_.get(), std::move(stream_delegate_)));
}

TEST_F(PdfNavigationThrottleUnseasonedEnabledTest, MaybeCreateThrottleFor) {
  InitializeNavigationHandle(stream_url(), CreateChildFrame());
  EXPECT_TRUE(PdfNavigationThrottle::MaybeCreateThrottleFor(
      navigation_handle_.get(), std::move(stream_delegate_)));
}

TEST_F(PdfNavigationThrottleUnseasonedEnabledTest,
       MaybeCreateThrottleForMainFrame) {
  InitializeNavigationHandle(stream_url(), main_rfh());
  EXPECT_FALSE(PdfNavigationThrottle::MaybeCreateThrottleFor(
      navigation_handle_.get(), std::move(stream_delegate_)));
}

TEST_F(PdfNavigationThrottleUnseasonedEnabledTest, WillStartRequest) {
  auto navigation_throttle = CreateNavigationThrottle(stream_url());
  NiceMock<content::MockWebContentsObserver> web_contents_observer(
      web_contents());

  EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            navigation_throttle->WillStartRequest().action());

  EXPECT_CALL(web_contents_observer, DidStartLoading());
  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   run_loop.QuitClosure());
  run_loop.Run();

  auto navigation_simulator =
      content::NavigationSimulator::CreateFromPending(web_contents());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      ui::PAGE_TRANSITION_AUTO_SUBFRAME,
      navigation_simulator->GetNavigationHandle()->GetPageTransition()));

  EXPECT_CALL(web_contents_observer, DidFinishLoad(_, original_url()));
  navigation_simulator->Commit();
}

TEST_F(PdfNavigationThrottleUnseasonedEnabledTest,
       WillStartRequestDeleteContents) {
  auto navigation_throttle = CreateNavigationThrottle(stream_url());
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

TEST_F(PdfNavigationThrottleUnseasonedEnabledTest,
       WillStartRequestNoStreamInfo) {
  EXPECT_CALL(*stream_delegate_, GetStreamInfo(_))
      .WillRepeatedly(Return(absl::nullopt));
  auto navigation_throttle = CreateNavigationThrottle(stream_url());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_throttle->WillStartRequest().action());
}

TEST_F(PdfNavigationThrottleUnseasonedEnabledTest, WillStartRequestOtherUrl) {
  auto navigation_throttle =
      CreateNavigationThrottle(GURL("https://example.test"));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_throttle->WillStartRequest().action());
}

}  // namespace pdf
