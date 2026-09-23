// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_tracker.h"

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace page_load_metrics {

namespace {

const char kTestUrl[] = "https://a.test/";

struct PageLoadMetricsObserverEvents final {
  size_t ready_to_commit_next_navigation_count = 0;
  bool was_started = false;
  bool was_prerender_started = false;
  bool was_committed = false;
  size_t render_frame_deleted_count = 0;
  size_t sub_frame_deleted_count = 0;
  bool was_prerendered_page_activated = false;
  size_t sub_frame_navigation_count = 0;
};

using content::test::ScopedPrerenderWebContentsDelegate;

class TestPageLoadMetricsObserver final : public PageLoadMetricsObserver {
 public:
  TestPageLoadMetricsObserver(PageLoadMetricsObserverEvents* events)
      : events_(events) {}

  void StopObservingOnPrerender() { stop_on_prerender_ = true; }

 private:
  void ReadyToCommitNextNavigation(
      content::NavigationHandle* navigation_handle) override {
    events_->ready_to_commit_next_navigation_count++;
  }
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override {
    EXPECT_FALSE(events_->was_started);
    events_->was_started = true;
    return CONTINUE_OBSERVING;
  }
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override {
    return STOP_OBSERVING;
  }
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override {
    EXPECT_FALSE(events_->was_prerender_started);
    events_->was_prerender_started = true;
    return stop_on_prerender_ ? STOP_OBSERVING : CONTINUE_OBSERVING;
  }
  ObservePolicy OnCommit(
      content::NavigationHandle* navigation_handle) override {
    EXPECT_FALSE(events_->was_committed);
    events_->was_committed = true;
    return CONTINUE_OBSERVING;
  }
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override {
    events_->sub_frame_navigation_count++;
  }
  void OnRenderFrameDeleted(content::RenderFrameHost* rfh) override {
    events_->render_frame_deleted_count++;
  }
  void OnSubFrameDeleted(content::FrameTreeNodeId frame_tree_node_id) override {
    events_->sub_frame_deleted_count++;
  }
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override {
    EXPECT_FALSE(events_->was_prerendered_page_activated);
    events_->was_prerendered_page_activated = true;

    EXPECT_NE(ukm::kInvalidSourceId, GetDelegate().GetPageUkmSourceId());
  }

  bool stop_on_prerender_ = false;

  // Event records should be owned outside this class as this instance will be
  // automatically destructed on STOP_OBSERVING, and so on.
  raw_ptr<PageLoadMetricsObserverEvents> events_;
};

class PageLoadTrackerTest : public PageLoadMetricsObserverContentTestHarness {
 public:
  PageLoadTrackerTest()
      : observer_(new TestPageLoadMetricsObserver(&events_)) {}

 protected:
  void SetTargetUrl(const std::string& url) { target_url_ = GURL(url); }
  const PageLoadMetricsObserverEvents& GetEvents() const { return events_; }
  ukm::SourceId GetObservedUkmSourceIdFor(const std::string& url) {
    return ukm_source_ids_[url];
  }

  void StopObservingOnPrerender() { observer_->StopObservingOnPrerender(); }

 private:
  void RegisterObservers(PageLoadTracker* tracker) override {
    ukm_source_ids_.emplace(tracker->GetUrl().spec(),
                            tracker->GetPageUkmSourceIdForTesting());

    if (tracker->GetUrl() != target_url_)
      return;

    EXPECT_FALSE(is_observer_passed_);
    tracker->AddObserver(
        std::unique_ptr<PageLoadMetricsObserverInterface>(observer_));
    is_observer_passed_ = true;
  }

  base::flat_map<std::string, ukm::SourceId> ukm_source_ids_;

  PageLoadMetricsObserverEvents events_;
  raw_ptr<TestPageLoadMetricsObserver, DanglingUntriaged> observer_;
  bool is_observer_passed_ = false;

  GURL target_url_;
};

TEST_F(PageLoadTrackerTest, PrimaryPageType) {
  ScopedPrerenderWebContentsDelegate web_contents_delegate(*web_contents());

  // Target URL to monitor the tracker via the test observer.
  SetTargetUrl(kTestUrl);

  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Check observer behaviors.
  EXPECT_TRUE(GetEvents().was_started);
  EXPECT_FALSE(GetEvents().was_prerender_started);
  EXPECT_TRUE(GetEvents().was_committed);

  // Navigate out.
  tester()->NavigateToUntrackedUrl();

  // Check observer behaviors.
  EXPECT_EQ(1u, GetEvents().ready_to_commit_next_navigation_count);

  // Check ukm::SourceId.
  EXPECT_NE(ukm::kInvalidSourceId, GetObservedUkmSourceIdFor(kTestUrl));
}

TEST_F(PageLoadTrackerTest, PrimaryPageTypeDataScheme) {
  // ScopedPrerenderWebContentsDelegate web_contents_delegate(*web_contents());
  // Target URL to monitor the tracker via the test observer.
  SetTargetUrl("data:text/html,Hello world");

  // Navigate in.
  NavigateAndCommit(GURL("data:text/html,Hello world"));

  // Check observer behaviors.
  EXPECT_TRUE(GetEvents().was_started);
  EXPECT_FALSE(GetEvents().was_committed);
}

TEST_F(PageLoadTrackerTest, NotReloadAfterDiscard) {
  SetTargetUrl(kTestUrl);
  NavigateAndCommit(GURL(kTestUrl));
  EXPECT_FALSE(tester()->GetDelegateForCommittedLoad().IsReloadAfterDiscard());
}

TEST_F(PageLoadTrackerTest, ReloadAfterDiscard) {
  SetTargetUrl(kTestUrl);
  web_contents()->SetWasDiscarded(true);
  NavigateAndCommit(GURL(kTestUrl));
  EXPECT_TRUE(tester()->GetDelegateForCommittedLoad().IsReloadAfterDiscard());
}

TEST_F(PageLoadTrackerTest, EventForwarding) {
  ScopedPrerenderWebContentsDelegate web_contents_delegate(*web_contents());

  // In the end, we'll construct frame trees as the following:
  //
  //   A : primary main frame
  //   +- B : iframe

  // Target URL to monitor the tracker via the test observer.
  SetTargetUrl(kTestUrl);

  // A: Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Check observer behaviors.
  EXPECT_TRUE(GetEvents().was_started);
  EXPECT_FALSE(GetEvents().was_prerender_started);
  EXPECT_TRUE(GetEvents().was_committed);

  // B: Add and navigate in.
  content::RenderFrameHost* rfh_b =
      content::RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
          ->AppendChild("b");
  {
    const char kURL[] = "https://a.test/iframe";
    auto simulator = content::NavigationSimulator::CreateRendererInitiated(
        GURL(kURL), rfh_b);
    ASSERT_NE(nullptr, simulator);
    simulator->Commit();

    rfh_b = simulator->GetFinalRenderFrameHost();
  }

  EXPECT_EQ(1u, GetEvents().sub_frame_navigation_count);
  EXPECT_EQ(0u, GetEvents().render_frame_deleted_count);
  EXPECT_EQ(0u, GetEvents().sub_frame_deleted_count);

  // B: Navigate out.
  {
    const char kURL[] = "https://b.test/iframe";
    auto simulator = content::NavigationSimulator::CreateRendererInitiated(
        GURL(kURL), rfh_b);
    ASSERT_NE(nullptr, simulator);
    simulator->Commit();

    rfh_b = simulator->GetFinalRenderFrameHost();
  }

  EXPECT_EQ(2u, GetEvents().sub_frame_navigation_count);

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kDefaultSiteInstanceGroups)) {
    EXPECT_EQ(1u, GetEvents().render_frame_deleted_count);
  } else if (content::WillSameSiteNavigationChangeRenderFrameHosts(
                 /*is_main_frame=*/true)) {
    EXPECT_EQ(0u, GetEvents().render_frame_deleted_count);
  } else {
    EXPECT_EQ(0u, GetEvents().render_frame_deleted_count);
  }
#else
  EXPECT_EQ(1u, GetEvents().render_frame_deleted_count);
#endif

  EXPECT_EQ(0u, GetEvents().sub_frame_deleted_count);

  {
    content::RenderFrameDeletedObserver delete_observer(rfh_b);
    // Remove B.
    content::RenderFrameHostTester::For(rfh_b)->Detach();
    delete_observer.WaitUntilDeleted();
  }

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kDefaultSiteInstanceGroups)) {
    EXPECT_EQ(2u, GetEvents().render_frame_deleted_count);
  } else if (content::WillSameSiteNavigationChangeRenderFrameHosts(
                 /*is_main_frame=*/true)) {
    EXPECT_EQ(1u, GetEvents().render_frame_deleted_count);
  } else {
    EXPECT_EQ(1u, GetEvents().render_frame_deleted_count);
  }
#else
  EXPECT_EQ(2u, GetEvents().render_frame_deleted_count);
#endif

  EXPECT_EQ(1u, GetEvents().sub_frame_deleted_count);
}

TEST_F(PageLoadTrackerTest, PrerenderPageType) {
  ScopedPrerenderWebContentsDelegate web_contents_delegate(*web_contents());

  // Target URL to monitor the tracker via the test observer.
  const char kPrerenderingUrl[] = "https://a.test/prerender";
  SetTargetUrl(kPrerenderingUrl);

  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Add a prerender page.
  content::WebContentsTester::For(web_contents())
      ->AddPrerenderAndCommitNavigation(GURL(kPrerenderingUrl));

  // Check observer behaviors.
  EXPECT_FALSE(GetEvents().was_started);
  EXPECT_TRUE(GetEvents().was_prerender_started);
  EXPECT_TRUE(GetEvents().was_committed);

  // Check ukm::SourceId.
  EXPECT_NE(ukm::kInvalidSourceId, GetObservedUkmSourceIdFor(kTestUrl));
  EXPECT_EQ(ukm::kInvalidSourceId, GetObservedUkmSourceIdFor(kPrerenderingUrl));
}

TEST_F(PageLoadTrackerTest, StopObservingOnPrerender) {
  ScopedPrerenderWebContentsDelegate web_contents_delegate(*web_contents());

  // Target URL to monitor the tracker via the test observer.
  const char kPrerenderingUrl[] = "https://a.test/prerender";
  SetTargetUrl(kPrerenderingUrl);
  StopObservingOnPrerender();

  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Add a prerender page.
  content::WebContentsTester::For(web_contents())
      ->AddPrerenderAndCommitNavigation(GURL(kPrerenderingUrl));

  // Check observer behaviors.
  EXPECT_FALSE(GetEvents().was_started);
  EXPECT_TRUE(GetEvents().was_prerender_started);
  EXPECT_FALSE(GetEvents().was_committed);
}

TEST_F(PageLoadTrackerTest, ResumeOnPrerenderActivation) {
  ScopedPrerenderWebContentsDelegate web_contents_delegate(*web_contents());

  // Target URL to monitor the tracker via the test observer.
  const char kPrerenderingUrl[] = "https://a.test/prerender";
  SetTargetUrl(kPrerenderingUrl);

  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Add a prerender page.
  content::WebContentsTester::For(web_contents())
      ->AddPrerenderAndCommitNavigation(GURL(kPrerenderingUrl));

  // Check observer behaviors.
  EXPECT_FALSE(GetEvents().was_started);
  EXPECT_TRUE(GetEvents().was_prerender_started);
  EXPECT_TRUE(GetEvents().was_committed);
  EXPECT_FALSE(GetEvents().was_prerendered_page_activated);

  // Activate the prerendered page.
  content::WebContentsTester::For(web_contents())
      ->ActivatePrerenderedPage(GURL(kPrerenderingUrl));

  EXPECT_TRUE(GetEvents().was_prerendered_page_activated);
}

// Regression test: activating a prerendered page in a non-visible tab should
// set first_background_time_ so that a subsequent PageShown() does not crash.
// Uses WasOccluded() rather than WasHidden() because the prerender host
// registry cancels activation when both initiator and target are HIDDEN.
// OCCLUDED exercises the same code path in DidActivatePrerenderedPage.
TEST_F(PageLoadTrackerTest, PrerenderActivationInBackgroundTab) {
  ScopedPrerenderWebContentsDelegate web_contents_delegate(*web_contents());

  const char kPrerenderingUrl[] = "https://a.test/prerender";
  SetTargetUrl(kPrerenderingUrl);

  // Navigate primary page in foreground.
  NavigateAndCommit(GURL(kTestUrl));

  // Add a prerender page.
  content::WebContentsTester::For(web_contents())
      ->AddPrerenderAndCommitNavigation(GURL(kPrerenderingUrl));

  // Occlude the tab before activation. This simulates activating a prerendered
  // page in a non-foreground tab (e.g., ctrl+click opening a background tab).
  web_contents()->WasOccluded();

  // Activate the prerendered page while the tab is occluded.
  content::WebContentsTester::For(web_contents())
      ->ActivatePrerenderedPage(GURL(kPrerenderingUrl));

  EXPECT_TRUE(GetEvents().was_prerendered_page_activated);

  // Switch to the tab. Without the fix, the DCHECK in PageShown() would fire
  // because first_background_time_ was never set.
  web_contents()->WasShown();
}

}  // namespace

}  // namespace page_load_metrics
