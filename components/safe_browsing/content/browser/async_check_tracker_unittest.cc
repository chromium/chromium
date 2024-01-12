// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/async_check_tracker.h"

#include "components/safe_browsing/content/browser/base_ui_manager.h"
#include "components/safe_browsing/content/browser/url_checker_on_sb.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"

namespace safe_browsing {

namespace {

using security_interstitials::UnsafeResource;

class MockUIManager : public BaseUIManager {
 public:
  MockUIManager() : BaseUIManager() {}

  MockUIManager(const MockUIManager&) = delete;
  MockUIManager& operator=(const MockUIManager&) = delete;

  void DisplayBlockingPage(const UnsafeResource& resource) override {
    display_blocking_page_called_times_++;
    displayed_resource_ = resource;
  }

  int DisplayBlockingPageCalledTimes() {
    return display_blocking_page_called_times_;
  }

  UnsafeResource GetDisplayedResource() { return displayed_resource_; }

 protected:
  ~MockUIManager() override {}

 private:
  int display_blocking_page_called_times_ = 0;
  UnsafeResource displayed_resource_;
};

}  // namespace

class AsyncCheckTrackerTest : public content::RenderViewHostTestHarness {
 protected:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    url_ = GURL("https://example.com/");
    ui_manager_ = base::MakeRefCounted<MockUIManager>();
    tracker_ = AsyncCheckTracker::GetOrCreateForWebContents(web_contents(),
                                                            ui_manager_.get());
  }

  void TearDown() override {
    tracker_ = nullptr;
    content::RenderViewHostTestHarness::TearDown();
  }

  void CallDidFinishNavigation(bool has_committed) {
    content::MockNavigationHandle handle(url_, main_rfh());
    handle.set_has_committed(has_committed);
    tracker_->DidFinishNavigation(&handle);
  }

  void CallPendingCheckerCompleted(bool proceed,
                                   bool has_post_commit_interstitial_skipped) {
    if (!proceed) {
      // This mocks how BaseUIManager caches unsafe resource if
      // load_post_commit_error_page is false.
      UnsafeResource resource;
      resource.url = url_;
      resource.threat_type = SB_THREAT_TYPE_URL_PHISHING;
      ui_manager_->AddUnsafeResource(url_, resource);
    }
    UrlCheckerOnSB::OnCompleteCheckResult result(
        proceed, /*showed_interstitial=*/true,
        has_post_commit_interstitial_skipped,
        SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck);
    tracker_->PendingCheckerCompleted(result);
  }

  GURL url_;
  scoped_refptr<MockUIManager> ui_manager_;
  raw_ptr<AsyncCheckTracker> tracker_;
};

TEST_F(AsyncCheckTrackerTest,
       DisplayBlockingPageNotCalled_PendingCheckNotCompleted) {
  CallDidFinishNavigation(/*has_committed=*/true);
  EXPECT_EQ(ui_manager_->DisplayBlockingPageCalledTimes(), 0);
}

TEST_F(AsyncCheckTrackerTest,
       DisplayBlockingPageNotCalled_PendingCheckProceed) {
  CallPendingCheckerCompleted(/*proceed=*/true,
                              /*has_post_commit_interstitial_skipped=*/false);
  CallDidFinishNavigation(/*has_committed=*/true);
  EXPECT_EQ(ui_manager_->DisplayBlockingPageCalledTimes(), 0);
}

TEST_F(AsyncCheckTrackerTest,
       DisplayBlockingPageNotCalled_PostCommitInterstitialNotSkipped) {
  CallPendingCheckerCompleted(/*proceed=*/false,
                              /*has_post_commit_interstitial_skipped=*/false);
  CallDidFinishNavigation(/*has_committed=*/true);
  EXPECT_EQ(ui_manager_->DisplayBlockingPageCalledTimes(), 0);
}

TEST_F(AsyncCheckTrackerTest,
       DisplayBlockingPageNotCalled_NavigationNotCommitted) {
  CallPendingCheckerCompleted(/*proceed=*/false,
                              /*has_post_commit_interstitial_skipped=*/true);
  CallDidFinishNavigation(/*has_committed=*/false);
  EXPECT_EQ(ui_manager_->DisplayBlockingPageCalledTimes(), 0);
}

TEST_F(AsyncCheckTrackerTest, DisplayBlockingPageCalled) {
  CallPendingCheckerCompleted(/*proceed=*/false,
                              /*has_post_commit_interstitial_skipped=*/true);
  CallDidFinishNavigation(/*has_committed=*/true);
  EXPECT_EQ(ui_manager_->DisplayBlockingPageCalledTimes(), 1);
  UnsafeResource resource = ui_manager_->GetDisplayedResource();
  EXPECT_EQ(resource.threat_type, SB_THREAT_TYPE_URL_PHISHING);
  EXPECT_EQ(resource.url, url_);
  EXPECT_EQ(resource.render_process_id, main_rfh()->GetGlobalId().child_id);
  EXPECT_EQ(resource.render_frame_token, main_rfh()->GetFrameToken().value());
}

}  // namespace safe_browsing
