// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/content/browser/web_contents_top_sites_observer.h"

#include "base/task/cancelable_task_tracker.h"
#include "base/test/scoped_feature_list.h"
#include "components/history/core/browser/top_sites.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

using testing::StrictMock;

namespace history {

class MockTopSites : public TopSites {
 public:
  MOCK_METHOD0(ShutdownOnUIThread, void());
  MOCK_METHOD1(GetMostVisitedURLs, void(GetMostVisitedURLsCallback callback));
  MOCK_METHOD0(SyncWithHistory, void());
  MOCK_CONST_METHOD0(HasBlockedUrls, bool());
  MOCK_METHOD1(AddBlockedUrl, void(const GURL& url));
  MOCK_METHOD1(RemoveBlockedUrl, void(const GURL& url));
  MOCK_METHOD1(IsBlocked, bool(const GURL& url));
  MOCK_METHOD0(ClearBlockedUrls, void());
  MOCK_METHOD0(StartQueryForMostVisited, base::CancelableTaskTracker::TaskId());
  MOCK_METHOD1(IsKnownURL, bool(const GURL& url));
  MOCK_CONST_METHOD1(GetCanonicalURLString,
                     const std::string&(const GURL& url));
  MOCK_METHOD0(IsFull, bool());
  MOCK_CONST_METHOD0(loaded, bool());
  MOCK_METHOD0(GetPrepopulatedPages, history::PrepopulatedPageList());
  MOCK_METHOD1(OnNavigationCommitted, void(const GURL& url));

  // Publicly expose notification to observers, since the implementation cannot
  // be overridden.
  using TopSites::NotifyTopSitesChanged;

 protected:
  ~MockTopSites() override = default;
};

class WebContentsTopSitesObserverTest
    : public content::RenderViewHostTestHarness {
 public:
  WebContentsTopSitesObserverTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }

  WebContentsTopSitesObserverTest(const WebContentsTopSitesObserverTest&) =
      delete;
  WebContentsTopSitesObserverTest& operator=(
      const WebContentsTopSitesObserverTest&) = delete;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    observer_ = base::WrapUnique(
        new WebContentsTopSitesObserver(web_contents(), mock_top_sites_.get()));
  }

  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents());
  }

  MockTopSites* GetMockTopSites() { return mock_top_sites_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<WebContentsTopSitesObserver> observer_;
  scoped_refptr<StrictMock<MockTopSites>> mock_top_sites_ =
      base::MakeRefCounted<StrictMock<MockTopSites>>();
};

TEST_F(WebContentsTopSitesObserverTest,
       DoNotCallOnNavigationCommittedInFencedFrame) {
  GURL page_url = GURL("https://foo.com");

  // Navigate a primary main frame.
  EXPECT_CALL(*GetMockTopSites(), OnNavigationCommitted(testing::_)).Times(1);
  web_contents_tester()->NavigateAndCommit(page_url);
  testing::Mock::VerifyAndClearExpectations(GetMockTopSites());

  // Navigate a fenced frame.
  EXPECT_CALL(*GetMockTopSites(), OnNavigationCommitted(testing::_)).Times(0);
  GURL fenced_frame_url = GURL("https://fencedframe.com");
  content::RenderFrameHost* fenced_frame_root =
      content::RenderFrameHostTester::For(main_rfh())->AppendFencedFrame();
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      fenced_frame_url, fenced_frame_root);
}

}  // namespace history
