// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_model.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"
#include "url/url_constants.h"

class TabModelBrowserTest : public InProcessBrowserTest {
 public:
  TabModelBrowserTest() {
    feature_list_.InitWithFeatures({features::kSideBySide}, {});
  }

  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }

  void AddTabs(int num_tabs) {
    for (int i = 0; i < num_tabs; i++) {
      chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), 1, false);
    }
  }

  struct MockCallbackOwner {
    base::MockRepeatingCallback<void(tabs::TabInterface*)> callback;
  };

  base::CallbackListSubscription ExpectDidActivateCallbackCount(int index,
                                                                int count) {
    callbacks_.push_back(std::make_unique<MockCallbackOwner>());
    EXPECT_CALL(callbacks_.back()->callback, Run).Times(count);
    return tab_strip_model()->GetTabAtIndex(index)->RegisterDidActivate(
        callbacks_.back()->callback.Get());
  }

  base::CallbackListSubscription ExpectDidBecomeVisibleCallbackCount(
      int index,
      int count) {
    callbacks_.push_back(std::make_unique<MockCallbackOwner>());
    EXPECT_CALL(callbacks_.back()->callback, Run).Times(count);
    return tab_strip_model()->GetTabAtIndex(index)->RegisterDidBecomeVisible(
        callbacks_.back()->callback.Get());
  }

  void VerifyAndClearCallbacks() {
    for (const auto& callback_owner : callbacks_) {
      testing::Mock::VerifyAndClearExpectations(&callback_owner->callback);
    }
  }

 private:
  std::vector<std::unique_ptr<MockCallbackOwner>> callbacks_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabModelBrowserTest, VisibilityCallbacks) {
  // Tab strip has 3 tabs with the first two in a split view.
  AddTabs(2);
  EXPECT_EQ(3, tab_strip_model()->count());
  tab_strip_model()->ActivateTabAt(0);
  tab_strip_model()->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);
  tab_strip_model()->ActivateTabAt(2);

  auto s0 = ExpectDidActivateCallbackCount(0, 0);
  auto s1 = ExpectDidBecomeVisibleCallbackCount(0, 1);
  auto s2 = ExpectDidActivateCallbackCount(1, 1);
  auto s3 = ExpectDidBecomeVisibleCallbackCount(1, 1);

  // Set the selection to be a tab in the split.
  tab_strip_model()->SelectTabAt(1);
  ASSERT_EQ(1, tab_strip_model()->active_index());
  ASSERT_TRUE(content::WaitForLoadStop(tab_strip_model()->GetWebContentsAt(1)));

  // Verify that the callbacks were called for each tab.
  VerifyAndClearCallbacks();
}
