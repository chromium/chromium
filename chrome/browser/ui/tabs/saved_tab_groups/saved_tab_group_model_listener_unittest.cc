// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"

#include <memory>

#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_types.h"

class SavedTabGroupModelListenerTest : public BrowserWithTestWindowTest {
 public:
  SavedTabGroupModelListenerTest() = default;
  SavedTabGroupModelListenerTest(const SavedTabGroupModelListenerTest&) =
      delete;
  SavedTabGroupModelListenerTest& operator=(
      const SavedTabGroupModelListenerTest&) = delete;

  Browser* AddBrowser() {
    Browser::CreateParams native_params(profile_.get(), true);
    native_params.initial_show_state = ui::SHOW_STATE_DEFAULT;
    std::unique_ptr<Browser> browser =
        CreateBrowserWithTestWindowForParams(native_params);
    Browser* browser_ptr = browser.get();
    browsers_.emplace_back(std::move(browser));
    return browser_ptr;
  }

  void AddTabToBrowser(Browser* browser, int index) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                          nullptr);
    browser->tab_strip_model()->AddWebContents(
        std::move(web_contents), index,
        ui::PageTransition::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);
  }

  SavedTabGroupModelListener* listener() { return listener_.get(); }

 private:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    model_ = std::make_unique<SavedTabGroupModel>();
    listener_ = std::make_unique<SavedTabGroupModelListener>(model_.get(),
                                                             profile_.get());
  }
  void TearDown() override {
    for (auto& browser : browsers_) {
      browser->tab_strip_model()->CloseAllTabs();
    }
  }

  content::RenderViewHostTestEnabler rvh_test_enabler_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<SavedTabGroupModel> model_;
  std::unique_ptr<SavedTabGroupModelListener> listener_;

  std::vector<std::unique_ptr<Browser>> browsers_;
};

TEST_F(SavedTabGroupModelListenerTest, CreatesRemovesBrowserListener) {
  Browser* browser_1 = AddBrowser();
  Browser* browser_2 = AddBrowser();

  EXPECT_EQ(listener()->GetBrowserListenerMapForTesting().count(browser_1), 1u);
  EXPECT_EQ(listener()->GetBrowserListenerMapForTesting().count(browser_2), 1u);

  listener()->OnBrowserRemoved(browser_1);
  EXPECT_EQ(listener()->GetBrowserListenerMapForTesting().count(browser_1), 0u);
  EXPECT_EQ(listener()->GetBrowserListenerMapForTesting().count(browser_2), 1u);
}

TEST_F(SavedTabGroupModelListenerTest, GetTabStripModelWithTabGroupId) {
  Browser* browser_1 = AddBrowser();

  EXPECT_TRUE(listener()->GetBrowserListenerMapForTesting().count(browser_1) >
              0);

  // Create a new tab and add it to a group.
  ASSERT_EQ(0, browser_1->tab_strip_model()->count());
  AddTabToBrowser(browser_1, 0);
  ASSERT_EQ(1, browser_1->tab_strip_model()->count());
  tab_groups::TabGroupId group_id =
      browser_1->tab_strip_model()->AddToNewGroup({0});

  EXPECT_EQ(browser_1->tab_strip_model(),
            listener()->GetTabStripModelWithTabGroupId(group_id));
}
