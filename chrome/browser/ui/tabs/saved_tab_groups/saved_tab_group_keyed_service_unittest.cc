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

TEST_F(SavedTabGroupKeyedServiceUnitTest, CreatesRemovesBrowserListener) {
  Browser* browser_1 = AddBrowser();
  Browser* browser_2 = AddBrowser();

  EXPECT_EQ(
      service()->listener()->GetBrowserListenerMapForTesting().count(browser_1),
      1u);
  EXPECT_EQ(
      service()->listener()->GetBrowserListenerMapForTesting().count(browser_2),
      1u);

  service()->listener()->OnBrowserRemoved(browser_1);
  EXPECT_EQ(
      service()->listener()->GetBrowserListenerMapForTesting().count(browser_1),
      0u);
  EXPECT_EQ(
      service()->listener()->GetBrowserListenerMapForTesting().count(browser_2),
      1u);
}

TEST_F(SavedTabGroupKeyedServiceUnitTest, GetTabStripModelWithTabGroupId) {
  Browser* browser_1 = AddBrowser();

  EXPECT_TRUE(service()->listener()->GetBrowserListenerMapForTesting().count(
                  browser_1) > 0);

  // Create a new tab and add it to a group.
  ASSERT_EQ(0, browser_1->tab_strip_model()->count());
  AddTabToBrowser(browser_1, 0);
  ASSERT_EQ(1, browser_1->tab_strip_model()->count());
  tab_groups::TabGroupId group_id =
      browser_1->tab_strip_model()->AddToNewGroup({0});

  EXPECT_EQ(browser_1->tab_strip_model(),
            service()->listener()->GetTabStripModelWithTabGroupId(group_id));
}

TEST_F(SavedTabGroupKeyedServiceUnitTest,
       UngroupingStopsListeningToWebContents) {
  Browser* browser_1 = AddBrowser();

  EXPECT_TRUE(service()->listener()->GetBrowserListenerMapForTesting().count(
                  browser_1) > 0);

  // Create a new tab and add it to a group.
  ASSERT_EQ(0, browser_1->tab_strip_model()->count());
  AddTabToBrowser(browser_1, 0);
  content::WebContents* web_contents_ptr = AddTabToBrowser(browser_1, 1);
  ASSERT_EQ(2, browser_1->tab_strip_model()->count());
  tab_groups::TabGroupId group_id =
      browser_1->tab_strip_model()->AddToNewGroup({0, 1});

  auto& listener_map = service()->listener()->GetBrowserListenerMapForTesting();
  EXPECT_EQ(1u, listener_map.count(browser_1));
  auto& tab_token_mapping =
      listener_map.at(browser_1).GetWebContentsTokenMapForTesting();

  // Expect that the tabs aren't being listened to yet.
  EXPECT_EQ(0u, tab_token_mapping.count(web_contents_ptr));

  // Save the group.
  service()->SaveGroup(group_id);

  // Expect that the listener map is listening to the 2nd tab before it's
  // closed.
  EXPECT_EQ(1u, tab_token_mapping.count(web_contents_ptr));

  // Remove a tab and expect it is removed from the listener maps.
  web_contents_ptr->Close();
  ASSERT_EQ(1, browser_1->tab_strip_model()->count());

  // Expect that the browser is not removed from the mapping since there's still
  // 1 tab in the group and the browser is not destroyed.
  EXPECT_EQ(1u, listener_map.count(browser_1));

  // Expect that the web_contents ptr was removed from the mapping.
  EXPECT_EQ(0u, tab_token_mapping.count(web_contents_ptr));
}
