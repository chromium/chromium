// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/security_interstitial_tab_helper.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace security_interstitials {

const char kTestSslMetricsName[] = "test_blocking_page";

std::unique_ptr<security_interstitials::MetricsHelper> CreateTestMetricsHelper(
    content::WebContents* web_contents) {
  MetricsHelper::ReportDetails report_details;
  report_details.metric_prefix = kTestSslMetricsName;
  return std::make_unique<security_interstitials::MetricsHelper>(
      GURL(), report_details, nullptr);
}

class TestInterstitialPage : public SecurityInterstitialPage {
 public:
  // |*destroyed_tracker| is set to true in the destructor.
  TestInterstitialPage(content::WebContents* web_contents,
                       const GURL& request_url,
                       bool* destroyed_tracker)
      : SecurityInterstitialPage(
            web_contents,
            request_url,
            std::make_unique<SecurityInterstitialControllerClient>(
                web_contents,
                CreateTestMetricsHelper(web_contents),
                nullptr,
                base::i18n::GetConfiguredLocale(),
                GURL(),
                /* settings_page_helper*/ nullptr)),
        destroyed_tracker_(destroyed_tracker) {}

  ~TestInterstitialPage() override { *destroyed_tracker_ = true; }

  void OnInterstitialClosing() override {}

 protected:
  void PopulateInterstitialStrings(base::Value::Dict& load_time_data) override {
  }

 private:
  raw_ptr<bool> destroyed_tracker_;
};

class SecurityInterstitialTabHelperTest
    : public content::RenderViewHostTestHarness {
 protected:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
  }

  // Creates a navigation handle for the main frame.
  std::unique_ptr<content::MockNavigationHandle> CreateHandle(
      bool committed,
      bool is_same_document) {
    return CreateHandle(main_rfh(), committed, is_same_document);
  }

  // Creates a navigation handle for a subframe.
  std::unique_ptr<content::MockNavigationHandle> CreateSubframeHandle(
      bool committed,
      bool is_same_document) {
    content::RenderFrameHost* subframe =
        content::RenderFrameHostTester::For(main_rfh())
            ->AppendChild("subframe");
    return CreateHandle(subframe, committed, is_same_document);
  }

  // Creates a navigation handle for the given frame.
  std::unique_ptr<content::MockNavigationHandle> CreateHandle(
      content::RenderFrameHost* frame,
      bool committed,
      bool is_same_document) {
    std::unique_ptr<content::MockNavigationHandle> handle =
        std::make_unique<content::MockNavigationHandle>(GURL(), frame);
    handle->set_has_committed(committed);
    handle->set_is_same_document(is_same_document);
    return handle;
  }

  // The lifetime of the blocking page is managed by the
  // SecurityInterstitialTabHelper for the test's web_contents.
  // |destroyed_tracker| will be set to true when the corresponding blocking
  // page is destroyed.
  void CreateAssociatedBlockingPage(content::NavigationHandle* handle,
                                    bool* destroyed_tracker) {
    SecurityInterstitialTabHelper::AssociateBlockingPage(
        handle, std::make_unique<TestInterstitialPage>(web_contents(), GURL(),
                                                       destroyed_tracker));
  }
};

// Tests that the helper properly handles the lifetime of a single blocking
// page, interleaved with other navigations.
TEST_F(SecurityInterstitialTabHelperTest, SingleBlockingPage) {
  std::unique_ptr<content::MockNavigationHandle> blocking_page_handle =
      CreateHandle(true, false);
  bool blocking_page_destroyed = false;
  CreateAssociatedBlockingPage(blocking_page_handle.get(),
                               &blocking_page_destroyed);
  SecurityInterstitialTabHelper* helper =
      SecurityInterstitialTabHelper::FromWebContents(web_contents());

  // Test that a same-document navigation doesn't destroy the blocking page if
  // its navigation hasn't committed yet.
  std::unique_ptr<content::MockNavigationHandle> same_document_handle =
      CreateHandle(true, true);
  helper->DidFinishNavigation(same_document_handle.get());
  EXPECT_FALSE(blocking_page_destroyed);

  // Test that a committed (non-same-document) navigation doesn't destroy the
  // blocking page if its navigation hasn't committed yet.
  std::unique_ptr<content::MockNavigationHandle> committed_handle1 =
      CreateHandle(true, false);
  helper->DidFinishNavigation(committed_handle1.get());
  EXPECT_FALSE(blocking_page_destroyed);

  // Simulate comitting the interstitial.
  helper->DidFinishNavigation(blocking_page_handle.get());
  EXPECT_FALSE(blocking_page_destroyed);

  // Test that a subsequent committed navigation releases the blocking page
  // stored for the currently committed navigation.
  std::unique_ptr<content::MockNavigationHandle> committed_handle2 =
      CreateHandle(true, false);
  helper->DidFinishNavigation(committed_handle2.get());
  EXPECT_TRUE(blocking_page_destroyed);
}

// Tests that the helper properly handles the lifetime of multiple blocking
// pages for a single FrameTreeNode, committed in a different order than they
// are created.
TEST_F(SecurityInterstitialTabHelperTest, MultipleBlockingPages) {
  // Simulate associating the first interstitial.
  std::unique_ptr<content::MockNavigationHandle> handle1 =
      CreateHandle(true, false);
  bool blocking_page1_destroyed = false;
  CreateAssociatedBlockingPage(handle1.get(), &blocking_page1_destroyed);

  // We can directly retrieve the helper for testing once
  // CreateAssociatedBlockingPage() was called.
  SecurityInterstitialTabHelper* helper =
      SecurityInterstitialTabHelper::FromWebContents(web_contents());

  // Simulate committing the first interstitial.
  helper->DidFinishNavigation(handle1.get());
  EXPECT_FALSE(blocking_page1_destroyed);
  EXPECT_TRUE(helper->IsDisplayingInterstitial());

  // Associate the second interstitial.
  std::unique_ptr<content::MockNavigationHandle> handle2 =
      CreateHandle(true, false);
  bool blocking_page2_destroyed = false;
  CreateAssociatedBlockingPage(handle2.get(), &blocking_page2_destroyed);
  EXPECT_FALSE(blocking_page1_destroyed);
  EXPECT_FALSE(blocking_page2_destroyed);

  // Associate the third interstitial.
  std::unique_ptr<content::MockNavigationHandle> handle3 =
      CreateHandle(true, false);
  bool blocking_page3_destroyed = false;
  CreateAssociatedBlockingPage(handle3.get(), &blocking_page3_destroyed);
  EXPECT_FALSE(blocking_page1_destroyed);
  EXPECT_FALSE(blocking_page2_destroyed);
  EXPECT_FALSE(blocking_page3_destroyed);

  // Simulate committing the third interstitial.
  helper->DidFinishNavigation(handle3.get());
  EXPECT_TRUE(blocking_page1_destroyed);
  EXPECT_FALSE(blocking_page2_destroyed);
  EXPECT_FALSE(blocking_page3_destroyed);

  // Simulate commiting the second interstitial.
  helper->DidFinishNavigation(handle2.get());
  EXPECT_TRUE(blocking_page1_destroyed);
  EXPECT_FALSE(blocking_page2_destroyed);
  EXPECT_TRUE(blocking_page3_destroyed);

  // Test that a subsequent committed navigation releases the last blocking
  // page.
  std::unique_ptr<content::MockNavigationHandle> committed_handle4 =
      CreateHandle(true, false);
  helper->DidFinishNavigation(committed_handle4.get());
  EXPECT_TRUE(blocking_page2_destroyed);
}

// Tests that the helper properly handles a navigation that finishes without
// committing.
TEST_F(SecurityInterstitialTabHelperTest, NavigationDoesNotCommit) {
  std::unique_ptr<content::MockNavigationHandle> committed_handle =
      CreateHandle(true, false);
  bool committed_blocking_page_destroyed = false;
  CreateAssociatedBlockingPage(committed_handle.get(),
                               &committed_blocking_page_destroyed);
  SecurityInterstitialTabHelper* helper =
      SecurityInterstitialTabHelper::FromWebContents(web_contents());
  helper->DidFinishNavigation(committed_handle.get());
  EXPECT_FALSE(committed_blocking_page_destroyed);

  // Simulate a navigation that does not commit.
  std::unique_ptr<content::MockNavigationHandle> non_committed_handle =
      CreateHandle(false, false);
  helper->DidFinishNavigation(non_committed_handle.get());

  // The blocking page for the non-committed navigation should have been cleaned
  // up, but the one for the previous committed navigation should still be
  // around.
  EXPECT_FALSE(committed_blocking_page_destroyed);

  // When a navigation does commit, the previous one should be cleaned up.
  std::unique_ptr<content::MockNavigationHandle> next_committed_handle =
      CreateHandle(true, false);
  helper->DidFinishNavigation(next_committed_handle.get());
  EXPECT_TRUE(committed_blocking_page_destroyed);
}

// Tests that main frame committed blocking interstitial is cleaned up on tab
// closing.
TEST_F(SecurityInterstitialTabHelperTest, ClosingTabForMainFrameInterstitial) {
  std::unique_ptr<content::MockNavigationHandle> committed_handle =
      CreateHandle(true, false);
  bool committed_blocking_page_destroyed = false;
  CreateAssociatedBlockingPage(committed_handle.get(),
                               &committed_blocking_page_destroyed);
  SecurityInterstitialTabHelper* helper =
      SecurityInterstitialTabHelper::FromWebContents(web_contents());
  helper->DidFinishNavigation(committed_handle.get());
  EXPECT_FALSE(committed_blocking_page_destroyed);

  // Only subframe interstitials are destroyed on frame deletion.
  helper->FrameDeleted(main_rfh()->GetFrameTreeNodeId());
  EXPECT_FALSE(committed_blocking_page_destroyed);

  helper->WebContentsDestroyed();
  EXPECT_TRUE(committed_blocking_page_destroyed);
}

// Tests that multiple FrameTreeNodes can display blocking interstitials.
TEST_F(SecurityInterstitialTabHelperTest, MultipleFramesBlocked) {
  // Simulate associating the first interstitial.
  std::unique_ptr<content::MockNavigationHandle> handle1 =
      CreateSubframeHandle(true, false);
  bool blocking_page1_destroyed = false;
  CreateAssociatedBlockingPage(handle1.get(), &blocking_page1_destroyed);

  // Simulate associating the second interstitial to another frame.
  std::unique_ptr<content::MockNavigationHandle> handle2 =
      CreateSubframeHandle(true, false);
  bool blocking_page2_destroyed = false;
  CreateAssociatedBlockingPage(handle2.get(), &blocking_page2_destroyed);

  // We can directly retrieve the helper for testing once
  // CreateAssociatedBlockingPage() was called.
  SecurityInterstitialTabHelper* helper =
      SecurityInterstitialTabHelper::FromWebContents(web_contents());

  // Simulate committing the first interstitial.
  helper->DidFinishNavigation(handle1.get());
  EXPECT_FALSE(blocking_page1_destroyed);
  EXPECT_FALSE(blocking_page2_destroyed);

  // The interstitial at frame 1 moved from pending to commit state.
  EXPECT_TRUE(
      helper->IsInterstitialCommittedForFrame(handle1->GetFrameTreeNodeId()));
  EXPECT_FALSE(
      helper->IsInterstitialCommittedForFrame(handle2->GetFrameTreeNodeId()));
  EXPECT_FALSE(
      helper->IsInterstitialPendingForNavigation(handle1->GetNavigationId()));
  EXPECT_TRUE(
      helper->IsInterstitialPendingForNavigation(handle2->GetNavigationId()));

  // Simulate committing the second interstitial.
  helper->DidFinishNavigation(handle2.get());
  EXPECT_FALSE(blocking_page1_destroyed);
  EXPECT_FALSE(blocking_page2_destroyed);

  // The interstitial at frame 2 moved from pending to commit state.
  EXPECT_TRUE(
      helper->IsInterstitialCommittedForFrame(handle1->GetFrameTreeNodeId()));
  EXPECT_TRUE(
      helper->IsInterstitialCommittedForFrame(handle2->GetFrameTreeNodeId()));
  EXPECT_FALSE(
      helper->IsInterstitialPendingForNavigation(handle1->GetNavigationId()));
  EXPECT_FALSE(
      helper->IsInterstitialPendingForNavigation(handle2->GetNavigationId()));

  helper->FrameDeleted(handle1->GetFrameTreeNodeId());
  EXPECT_TRUE(blocking_page1_destroyed);

  helper->FrameDeleted(handle2->GetFrameTreeNodeId());
  EXPECT_TRUE(blocking_page2_destroyed);
}

// Tests that navigation events associated to one FrameTreeNode do not affect
// the pending interstitial for another FrameTreeNode.
TEST_F(SecurityInterstitialTabHelperTest,
       NavigationForSubframeWithPendingInterstitial) {
  content::RenderFrameHost* subframe1 =
      content::RenderFrameHostTester::For(main_rfh())
          ->AppendChild("subframe 1");
  content::RenderFrameHost* subframe2 =
      content::RenderFrameHostTester::For(main_rfh())
          ->AppendChild("subframe 2");

  std::unique_ptr<content::MockNavigationHandle> committed_handle =
      CreateHandle(subframe1, true, false);
  bool committed_blocking_page_destroyed = false;
  CreateAssociatedBlockingPage(committed_handle.get(),
                               &committed_blocking_page_destroyed);

  std::unique_ptr<content::MockNavigationHandle> pending_handle =
      CreateHandle(subframe2, true, false);
  bool pending_blocking_page_destroyed = false;
  CreateAssociatedBlockingPage(pending_handle.get(),
                               &pending_blocking_page_destroyed);

  // Simulate committing the interstitial for frame 1.
  SecurityInterstitialTabHelper* helper =
      SecurityInterstitialTabHelper::FromWebContents(web_contents());
  helper->DidFinishNavigation(committed_handle.get());
  EXPECT_FALSE(committed_blocking_page_destroyed);
  EXPECT_FALSE(pending_blocking_page_destroyed);

  // The interstitial at frame 1 moved from pending to commit state.
  EXPECT_TRUE(helper->IsInterstitialCommittedForFrame(
      committed_handle->GetFrameTreeNodeId()));
  EXPECT_FALSE(helper->IsInterstitialCommittedForFrame(
      pending_handle->GetFrameTreeNodeId()));
  EXPECT_FALSE(helper->IsInterstitialPendingForNavigation(
      committed_handle->GetNavigationId()));
  EXPECT_TRUE(helper->IsInterstitialPendingForNavigation(
      pending_handle->GetNavigationId()));

  // Simulate a navigation that does not commit.
  std::unique_ptr<content::MockNavigationHandle> non_committed_handle =
      CreateHandle(subframe1, false, false);
  helper->DidFinishNavigation(non_committed_handle.get());

  // The non-committed navigation should not affect the state of committed or
  // pending blocking pages.
  EXPECT_FALSE(committed_blocking_page_destroyed);
  EXPECT_FALSE(pending_blocking_page_destroyed);
  EXPECT_TRUE(helper->IsInterstitialCommittedForFrame(
      committed_handle->GetFrameTreeNodeId()));
  EXPECT_FALSE(helper->IsInterstitialCommittedForFrame(
      pending_handle->GetFrameTreeNodeId()));
  EXPECT_FALSE(helper->IsInterstitialPendingForNavigation(
      committed_handle->GetNavigationId()));
  EXPECT_TRUE(helper->IsInterstitialPendingForNavigation(
      pending_handle->GetNavigationId()));

  // When a navigation does commit, the previous one should be cleaned up.
  std::unique_ptr<content::MockNavigationHandle> next_committed_handle =
      CreateHandle(subframe1, true, false);
  helper->DidFinishNavigation(next_committed_handle.get());
  EXPECT_TRUE(committed_blocking_page_destroyed);
  EXPECT_FALSE(helper->IsInterstitialCommittedForFrame(
      committed_handle->GetFrameTreeNodeId()));
  EXPECT_FALSE(helper->IsInterstitialPendingForNavigation(
      committed_handle->GetNavigationId()));

  // The pending interstitial should still be present.
  EXPECT_FALSE(pending_blocking_page_destroyed);
  EXPECT_TRUE(helper->IsInterstitialPendingForNavigation(
      pending_handle->GetNavigationId()));
  EXPECT_FALSE(helper->IsInterstitialCommittedForFrame(
      pending_handle->GetFrameTreeNodeId()));

  // Frame deletions only affect committed interstitials.
  helper->FrameDeleted(pending_handle->GetFrameTreeNodeId());
  EXPECT_FALSE(pending_blocking_page_destroyed);

  // Delete pending interstitials via web contents destruction.
  DeleteContents();
  EXPECT_TRUE(pending_blocking_page_destroyed);
}

// Tests that navigation events associated to one FrameTreeNode do not affect
// the committed interstitial for another FrameTreeNode.
TEST_F(SecurityInterstitialTabHelperTest,
       NavigationForSubframeWithCommittedInterstitial) {
  content::RenderFrameHost* subframe1 =
      content::RenderFrameHostTester::For(main_rfh())
          ->AppendChild("subframe 1");
  content::RenderFrameHost* subframe2 =
      content::RenderFrameHostTester::For(main_rfh())
          ->AppendChild("subframe 2");

  std::unique_ptr<content::MockNavigationHandle> committed_handle =
      CreateHandle(subframe1, true, false);
  bool committed_blocking_page_destroyed = false;
  CreateAssociatedBlockingPage(committed_handle.get(),
                               &committed_blocking_page_destroyed);

  std::unique_ptr<content::MockNavigationHandle> committed_handle2 =
      CreateHandle(subframe2, true, false);
  bool committed_blocking_page2_destroyed = false;
  CreateAssociatedBlockingPage(committed_handle2.get(),
                               &committed_blocking_page2_destroyed);

  // Simulate committing the interstitial for frame 1.
  SecurityInterstitialTabHelper* helper =
      SecurityInterstitialTabHelper::FromWebContents(web_contents());
  helper->DidFinishNavigation(committed_handle.get());
  EXPECT_FALSE(committed_blocking_page_destroyed);
  EXPECT_FALSE(committed_blocking_page2_destroyed);

  // Simulate committing the interstitial for frame 2.
  helper->DidFinishNavigation(committed_handle2.get());
  EXPECT_FALSE(committed_blocking_page_destroyed);
  EXPECT_FALSE(committed_blocking_page2_destroyed);

  // Both interstitials have moved from pending to commit state.
  EXPECT_TRUE(helper->IsInterstitialCommittedForFrame(
      committed_handle->GetFrameTreeNodeId()));
  EXPECT_TRUE(helper->IsInterstitialCommittedForFrame(
      committed_handle2->GetFrameTreeNodeId()));
  EXPECT_FALSE(helper->IsInterstitialPendingForNavigation(
      committed_handle->GetNavigationId()));
  EXPECT_FALSE(helper->IsInterstitialPendingForNavigation(
      committed_handle2->GetNavigationId()));

  // Simulate a navigation that does not commit.
  std::unique_ptr<content::MockNavigationHandle> non_committed_handle =
      CreateHandle(subframe1, false, false);
  helper->DidFinishNavigation(non_committed_handle.get());

  // The non-committed navigation should not affect the state of committed or
  // pending blocking pages.
  EXPECT_FALSE(committed_blocking_page_destroyed);
  EXPECT_FALSE(committed_blocking_page2_destroyed);
  EXPECT_TRUE(helper->IsInterstitialCommittedForFrame(
      committed_handle->GetFrameTreeNodeId()));
  EXPECT_TRUE(helper->IsInterstitialCommittedForFrame(
      committed_handle2->GetFrameTreeNodeId()));
  EXPECT_FALSE(helper->IsInterstitialPendingForNavigation(
      committed_handle->GetNavigationId()));
  EXPECT_FALSE(helper->IsInterstitialPendingForNavigation(
      committed_handle2->GetNavigationId()));

  // When a navigation does commit, the previous one should be cleaned up.
  std::unique_ptr<content::MockNavigationHandle> next_committed_handle =
      CreateHandle(subframe1, true, false);
  helper->DidFinishNavigation(next_committed_handle.get());
  EXPECT_TRUE(committed_blocking_page_destroyed);
  EXPECT_FALSE(helper->IsInterstitialCommittedForFrame(
      committed_handle->GetFrameTreeNodeId()));
  EXPECT_FALSE(helper->IsInterstitialPendingForNavigation(
      committed_handle->GetNavigationId()));

  // The pending interstitial should still be present.
  EXPECT_FALSE(committed_blocking_page2_destroyed);
  EXPECT_TRUE(helper->IsInterstitialCommittedForFrame(
      committed_handle2->GetFrameTreeNodeId()));

  helper->FrameDeleted(committed_handle2->GetFrameTreeNodeId());
  EXPECT_TRUE(committed_blocking_page2_destroyed);
}

class SecurityInterstitialTabHelperFencedFrameTest
    : public SecurityInterstitialTabHelperTest {
 public:
  SecurityInterstitialTabHelperFencedFrameTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }
  ~SecurityInterstitialTabHelperFencedFrameTest() override = default;

  content::RenderFrameHost* CreateFencedFrame(
      content::RenderFrameHost* parent) {
    content::RenderFrameHost* fenced_frame =
        content::RenderFrameHostTester::For(parent)->AppendFencedFrame();
    return fenced_frame;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that a FencedFrame does not close the interstitial page.
TEST_F(SecurityInterstitialTabHelperFencedFrameTest,
       FencedFrameDoesNotCloseInterstitialPage) {
  std::unique_ptr<content::NavigationHandle> blocking_page_handle =
      CreateHandle(true, false);
  bool blocking_page_destroyed = false;
  CreateAssociatedBlockingPage(blocking_page_handle.get(),
                               &blocking_page_destroyed);
  SecurityInterstitialTabHelper* helper =
      SecurityInterstitialTabHelper::FromWebContents(web_contents());
  helper->DidFinishNavigation(blocking_page_handle.get());
  EXPECT_FALSE(blocking_page_destroyed);
  EXPECT_TRUE(helper->IsDisplayingInterstitial());

  // Navigate a fenced frame and the interstitial page should be kept visible on
  // the fenced frame.
  GURL fenced_frame_url = GURL("https://fencedframe.com");
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* fenced_frame_rfh = CreateFencedFrame(main_rfh());
  std::unique_ptr<content::NavigationSimulator> navigation_simulator =
      content::NavigationSimulator::CreateRendererInitiated(fenced_frame_url,
                                                            fenced_frame_rfh);
  navigation_simulator->Commit();
  fenced_frame_rfh = navigation_simulator->GetFinalRenderFrameHost();
  EXPECT_TRUE(fenced_frame_rfh->IsFencedFrameRoot());
  EXPECT_FALSE(blocking_page_destroyed);
  EXPECT_TRUE(helper->IsDisplayingInterstitial());

  // When a navigation does commit, the fenced frame one should be cleaned up.
  std::unique_ptr<content::NavigationHandle> next_committed_handle =
      CreateHandle(true, false);
  helper->DidFinishNavigation(next_committed_handle.get());
  EXPECT_TRUE(blocking_page_destroyed);
  EXPECT_FALSE(helper->IsDisplayingInterstitial());
}

}  // namespace security_interstitials
