// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"

#include <memory>

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_types.h"

class SavedTabGroupKeyedServiceUnitTest : public BrowserWithTestWindowTest {
 public:
  SavedTabGroupKeyedServiceUnitTest() = default;
  SavedTabGroupKeyedServiceUnitTest(const SavedTabGroupKeyedServiceUnitTest&) =
      delete;
  SavedTabGroupKeyedServiceUnitTest& operator=(
      const SavedTabGroupKeyedServiceUnitTest&) = delete;

  Browser* AddBrowser() {
    Browser::CreateParams native_params(profile_.get(), true);
    native_params.initial_show_state = ui::SHOW_STATE_DEFAULT;
    std::unique_ptr<Browser> browser =
        CreateBrowserWithTestWindowForParams(native_params);
    Browser* browser_ptr = browser.get();
    browsers_.emplace_back(std::move(browser));
    return browser_ptr;
  }

  content::WebContents* AddTabToBrowser(Browser* browser, int index) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                          nullptr);

    content::WebContents* web_contents_ptr = web_contents.get();

    browser->tab_strip_model()->AddWebContents(
        std::move(web_contents), index,
        ui::PageTransition::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);

    return web_contents_ptr;
  }

  SavedTabGroupKeyedService* service() { return service_.get(); }

 private:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    service_ = std::make_unique<SavedTabGroupKeyedService>(profile_.get());
  }
  void TearDown() override {
    for (auto& browser : browsers_) {
      browser->tab_strip_model()->CloseAllTabs();
    }
  }

  content::RenderViewHostTestEnabler rvh_test_enabler_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<SavedTabGroupKeyedService> service_;

  std::vector<std::unique_ptr<Browser>> browsers_;
};

TEST_F(SavedTabGroupKeyedServiceUnitTest, GetBrowserWithTabGroupId) {
  Browser* browser_1 = AddBrowser();

  // Create a new tab and add it to a group.
  ASSERT_EQ(0, browser_1->tab_strip_model()->count());
  AddTabToBrowser(browser_1, 0);
  ASSERT_EQ(1, browser_1->tab_strip_model()->count());
  tab_groups::TabGroupId group_id =
      browser_1->tab_strip_model()->AddToNewGroup({0});

  EXPECT_EQ(browser_1,
            service()->listener()->GetBrowserWithTabGroupId(group_id));
}

TEST_F(SavedTabGroupKeyedServiceUnitTest,
       UngroupingStopsListeningToWebContents) {
  Browser* browser_1 = AddBrowser();

  // Create a new tab and add it to a group.
  ASSERT_EQ(0, browser_1->tab_strip_model()->count());
  AddTabToBrowser(browser_1, 0);
  content::WebContents* web_contents_ptr = AddTabToBrowser(browser_1, 1);
  ASSERT_EQ(2, browser_1->tab_strip_model()->count());
  tab_groups::TabGroupId group_id =
      browser_1->tab_strip_model()->AddToNewGroup({0, 1});

  auto& group_listener_map =
      service()->listener()->GetLocalTabGroupListenerMapForTesting();

  // Expect that the group isn't being listened to yet.
  EXPECT_EQ(0u, group_listener_map.count(group_id));

  // Save the group.
  service()->SaveGroup(group_id);

  // Now the group should be listened to.
  EXPECT_EQ(1u, group_listener_map.count(group_id));

  // Expect that the listener map is listening to two tabs, including
  // `web_contents_ptr`.
  auto& tab_token_mapping =
      group_listener_map.at(group_id).GetWebContentsTokenMapForTesting();
  EXPECT_EQ(2u, tab_token_mapping.size());
  EXPECT_EQ(1u, tab_token_mapping.count(web_contents_ptr));

  // Remove `web_contents_ptr`.
  web_contents_ptr->Close();
  ASSERT_EQ(1, browser_1->tab_strip_model()->count());

  // Expect that the group is still listened to since there's still
  // 1 tab in the group.
  EXPECT_EQ(1u, group_listener_map.count(group_id));

  // Expect that `web_contents_ptr` is not being listened to.
  EXPECT_EQ(0u, tab_token_mapping.count(web_contents_ptr));
}

TEST_F(SavedTabGroupKeyedServiceUnitTest, AddedTabIsListenedTo) {
  Browser* browser_1 = AddBrowser();

  // Create a saved tab group with one tab.
  ASSERT_EQ(0, browser_1->tab_strip_model()->count());
  AddTabToBrowser(browser_1, 0);
  ASSERT_EQ(1, browser_1->tab_strip_model()->count());
  tab_groups::TabGroupId group_id =
      browser_1->tab_strip_model()->AddToNewGroup({0});
  service()->SaveGroup(group_id);

  // One tab should be observed in this group.
  auto& tab_token_mapping = service()
                                ->listener()
                                ->GetLocalTabGroupListenerMapForTesting()
                                .at(group_id)
                                .GetWebContentsTokenMapForTesting();
  ASSERT_EQ(1u, tab_token_mapping.size());

  // Add a second tab and expect that it is observed too.
  content::WebContents* added_tab = AddTabToBrowser(browser_1, 1);
  browser_1->tab_strip_model()->AddToExistingGroup({1}, group_id);
  EXPECT_EQ(2u, tab_token_mapping.size());
  EXPECT_TRUE(tab_token_mapping.contains(added_tab));
}
