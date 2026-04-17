// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/shared_tab_group_feedback_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/data_sharing/public/features.h"
#include "content/public/test/browser_test.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"

namespace tab_groups {

class SharedTabGroupFeedbackControllerBrowserTest
    : public InProcessBrowserTest {
 public:
  SharedTabGroupFeedbackControllerBrowserTest() {
    feature_list_.InitAndEnableFeature(
        data_sharing::features::kDataSharingFeature);
  }
  ~SharedTabGroupFeedbackControllerBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SharedTabGroupFeedbackControllerBrowserTest,
                       UpdateFeedbackButtonVisibility) {
  SharedTabGroupFeedbackController* controller =
      browser()->GetFeatures().shared_tab_group_feedback_controller();
  ASSERT_NE(controller, nullptr);

  // Call UpdateFeedbackButtonVisibility(true) to show the button.
  controller->UpdateFeedbackButtonVisibility(true);

  // Verify the resulting pinned toolbar action button has element identifier
  // kSharedTabGroupFeedbackElementId as IPH will later anchor to it.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::View* feedback_button =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kSharedTabGroupFeedbackElementId, browser_view->GetElementContext());

  EXPECT_NE(feedback_button, nullptr);
}

}  // namespace tab_groups
