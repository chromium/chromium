// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/browser/page_settled_monitor.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/page_content_annotations/content/mojom/page_stability.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace page_content_annotations {

namespace {

using ::testing::_;
using ::testing::ByMove;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::WithArg;

class MockPageStabilityMonitor : public mojom::PageStabilityMonitor {
 public:
  MockPageStabilityMonitor() = default;
  ~MockPageStabilityMonitor() override = default;

  void NotifyWhenStable(base::TimeDelta start_delay,
                        NotifyWhenStableCallback callback) override {
    callback_ = std::move(callback);
  }

  void OnStable() {
    if (callback_) {
      std::move(callback_).Run();
    }
  }

  mojo::PendingRemote<mojom::PageStabilityMonitor> Bind() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  NotifyWhenStableCallback callback_;
  mojo::Receiver<mojom::PageStabilityMonitor> receiver_{this};
};

class MockPageSettledMonitorDelegate : public PageSettledMonitor::Delegate {
 public:
  MockPageSettledMonitorDelegate()
      : PageSettledMonitor::Delegate(PageSettledMonitor::PageStabilityConfig{
            .supports_paint_stability = false}) {}

  MOCK_METHOD(mojo::PendingRemote<mojom::PageStabilityMonitor>,
              CreatePageStabilityMonitor,
              (content::RenderFrameHost*),
              (override));
  MOCK_METHOD(void, WillMoveToState, (PageSettledMonitor::State), (override));
  MOCK_METHOD(void,
              OnMilestoneReached,
              (PageSettledMonitor::Milestone, base::OnceClosure),
              (override));
  MOCK_METHOD(void, OnEvent, (PageSettledMonitor::Event), (override));
};

class PageSettledMonitorBrowserTest : public content::ContentBrowserTest {
 public:
  ~PageSettledMonitorBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* web_contents() { return shell()->web_contents(); }

  content::RenderFrameHost* main_rfh() {
    return web_contents()->GetPrimaryMainFrame();
  }
};

IN_PROC_BROWSER_TEST_F(PageSettledMonitorBrowserTest, BasicFlow) {
  MockPageStabilityMonitor stability_monitor;
  auto delegate_ptr =
      std::make_unique<NiceMock<MockPageSettledMonitorDelegate>>();
  auto* delegate = delegate_ptr.get();

  EXPECT_CALL(*delegate, CreatePageStabilityMonitor)
      .WillOnce(Return(ByMove(stability_monitor.Bind())));

  // Ensure milestones resume immediately.
  ON_CALL(*delegate, OnMilestoneReached)
      .WillByDefault(WithArg<1>([](base::OnceClosure resume_callback) {
        std::move(resume_callback).Run();
      }));
  {
    InSequence s;

    // Set expectations for major milestones and events.
    EXPECT_CALL(
        *delegate,
        WillMoveToState(PageSettledMonitor::State::kWaitForPageStability));
    EXPECT_CALL(
        *delegate,
        OnMilestoneReached(PageSettledMonitor::Milestone::kPageStability, _));

    EXPECT_CALL(
        *delegate,
        WillMoveToState(PageSettledMonitor::State::kWaitForLoadCompletion));
    EXPECT_CALL(
        *delegate,
        OnMilestoneReached(PageSettledMonitor::Milestone::kLoadCompletion, _));

    EXPECT_CALL(
        *delegate,
        WillMoveToState(PageSettledMonitor::State::kWaitForVisualStateUpdate));
    EXPECT_CALL(*delegate,
                OnMilestoneReached(
                    PageSettledMonitor::Milestone::kVisualStateUpdate, _));

    EXPECT_CALL(*delegate,
                WillMoveToState(PageSettledMonitor::State::kMaybeDelayForLcp));
    EXPECT_CALL(*delegate, OnMilestoneReached(
                               PageSettledMonitor::Milestone::kLcpSettled, _));

    EXPECT_CALL(*delegate, WillMoveToState(PageSettledMonitor::State::kDone));
  }

  PageSettledMonitor monitor(main_rfh(), std::move(delegate_ptr));

  base::test::TestFuture<void> settled_future;
  monitor.Wait(web_contents(), settled_future.GetCallback());

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("/title1.html")));

  stability_monitor.OnStable();

  EXPECT_TRUE(settled_future.Wait());
}

IN_PROC_BROWSER_TEST_F(PageSettledMonitorBrowserTest, Timeout) {
  auto delegate_ptr =
      std::make_unique<NiceMock<MockPageSettledMonitorDelegate>>();
  auto* delegate = delegate_ptr.get();

  EXPECT_CALL(*delegate, CreatePageStabilityMonitor)
      .WillOnce(Return(ByMove(mojo::NullRemote())));

  // Hang stability milestone.
  base::OnceClosure hang_callback;
  EXPECT_CALL(*delegate, OnMilestoneReached(
                             PageSettledMonitor::Milestone::kPageStability, _))
      .WillOnce(WithArg<1>([&hang_callback](base::OnceClosure resume_callback) {
        hang_callback = std::move(resume_callback);
      }));

  EXPECT_CALL(*delegate, OnEvent(PageSettledMonitor::Event::kPageStabilized))
      .Times(0);
  EXPECT_CALL(*delegate, WillMoveToState(
                             PageSettledMonitor::State::kWaitForLoadCompletion))
      .Times(0);

  {
    InSequence s;
    EXPECT_CALL(
        *delegate,
        WillMoveToState(PageSettledMonitor::State::kWaitForPageStability));
    EXPECT_CALL(*delegate,
                WillMoveToState(PageSettledMonitor::State::kDidTimeout));
    EXPECT_CALL(*delegate, WillMoveToState(PageSettledMonitor::State::kDone));
  }

  PageSettledMonitor monitor(main_rfh(), std::move(delegate_ptr));

  base::test::TestFuture<void> settled_future;
  monitor.Wait(web_contents(), settled_future.GetCallback());

  EXPECT_TRUE(settled_future.Wait());
}

}  // namespace

}  // namespace page_content_annotations
