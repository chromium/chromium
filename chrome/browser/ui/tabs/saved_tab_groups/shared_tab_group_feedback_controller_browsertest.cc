// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/shared_tab_group_feedback_controller.h"

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/data_sharing/public/features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

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
      tab_groups::SharedTabGroupFeedbackController::From(browser());
  ASSERT_NE(controller, nullptr);

  controller->UpdateFeedbackButtonVisibility(/*should_show_button=*/true);

  // Verify the resulting pinned toolbar action button has element identifier
  // kSharedTabGroupFeedbackElementId as IPH will later anchor to it.
  auto* browser_elements = BrowserElements::From(browser());
  ASSERT_NE(browser_elements, nullptr);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return browser_elements->GetElement(kSharedTabGroupFeedbackElementId) !=
           nullptr;
  }));
}

}  // namespace tab_groups
