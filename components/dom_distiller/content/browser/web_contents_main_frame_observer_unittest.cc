// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/web_contents_main_frame_observer.h"

#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"

namespace dom_distiller {

// Define named constants to make tests a bit easier to read.
const bool kMainFrame = true;
const bool kChildFrame = false;

const bool kSameDocument = true;
const bool kDifferentDocument = false;

class WebContentsMainFrameObserverTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    // This needed to keep the WebContentsObserverSanityChecker checks happy for
    // when AppendChild is called.
    NavigateAndCommit(GURL("about:blank"));
    WebContentsMainFrameObserver::CreateForWebContents(web_contents());
    main_frame_observer_ =
        WebContentsMainFrameObserver::FromWebContents(web_contents());
    EXPECT_FALSE(main_frame_observer_->is_document_loaded_in_main_frame());
  }

  void Navigate(bool main_frame, bool same_document) {
    content::RenderFrameHost* rfh = main_rfh();
    content::RenderFrameHostTester* rfh_tester =
        content::RenderFrameHostTester::For(rfh);
    if (!main_frame)
      rfh = rfh_tester->AppendChild("subframe");
    content::MockNavigationHandle navigation_handle(GURL(), rfh);
    navigation_handle.set_has_committed(true);
    navigation_handle.set_is_same_document(same_document);
    main_frame_observer_->DidFinishNavigation(&navigation_handle);
  }

 protected:
  WebContentsMainFrameObserver* main_frame_observer_;  // weak
};

TEST_F(WebContentsMainFrameObserverTest, ListensForMainFrameNavigation) {
  Navigate(kMainFrame, kDifferentDocument);
  EXPECT_TRUE(main_frame_observer_->is_initialized());
  EXPECT_FALSE(main_frame_observer_->is_document_loaded_in_main_frame());

  main_frame_observer_->DOMContentLoaded(main_rfh());
  EXPECT_TRUE(main_frame_observer_->is_document_loaded_in_main_frame());
}

TEST_F(WebContentsMainFrameObserverTest, DISABLED_IgnoresChildFrameNavigation) {
  Navigate(kChildFrame, kDifferentDocument);
  EXPECT_FALSE(main_frame_observer_->is_initialized());
  EXPECT_FALSE(main_frame_observer_->is_document_loaded_in_main_frame());
}

// Flaky on Win. http://crbug.com/1010354
#if defined(OS_WIN)
#define MAYBE_IgnoresSameDocumentNavigationd \
  DISABLED_IgnoresSameDocumentNavigationd
#else
#define MAYBE_IgnoresSameDocumentNavigationd IgnoresSameDocumentNavigationd
#endif
TEST_F(WebContentsMainFrameObserverTest, IgnoresSameDocumentNavigation) {
  Navigate(kMainFrame, kSameDocument);
  EXPECT_FALSE(main_frame_observer_->is_initialized());
  EXPECT_FALSE(main_frame_observer_->is_document_loaded_in_main_frame());
}

TEST_F(WebContentsMainFrameObserverTest,
       IgnoresSameDocumentavigationUnlessMainFrameLoads) {
  Navigate(kMainFrame, kSameDocument);
  EXPECT_FALSE(main_frame_observer_->is_initialized());
  EXPECT_FALSE(main_frame_observer_->is_document_loaded_in_main_frame());

  // Even if we didn't acknowledge a same-document navigation, if the main
  // frame loads, consider a load complete.
  main_frame_observer_->DOMContentLoaded(main_rfh());
  EXPECT_TRUE(main_frame_observer_->is_document_loaded_in_main_frame());
}

TEST_F(WebContentsMainFrameObserverTest, ResetOnPageNavigation) {
  Navigate(kMainFrame, kDifferentDocument);
  main_frame_observer_->DOMContentLoaded(main_rfh());
  EXPECT_TRUE(main_frame_observer_->is_document_loaded_in_main_frame());

  // Another navigation should result in waiting for a page load.
  Navigate(kMainFrame, kDifferentDocument);
  EXPECT_TRUE(main_frame_observer_->is_initialized());
  EXPECT_FALSE(main_frame_observer_->is_document_loaded_in_main_frame());
}

TEST_F(WebContentsMainFrameObserverTest, DoesNotResetOnInPageNavigation) {
  Navigate(kMainFrame, kDifferentDocument);
  main_frame_observer_->DOMContentLoaded(main_rfh());
  EXPECT_TRUE(main_frame_observer_->is_document_loaded_in_main_frame());

  // Navigating withing the page should not result in waiting for a page load.
  Navigate(kMainFrame, kSameDocument);
  EXPECT_TRUE(main_frame_observer_->is_initialized());
  EXPECT_TRUE(main_frame_observer_->is_document_loaded_in_main_frame());
}

}  // namespace dom_distiller
