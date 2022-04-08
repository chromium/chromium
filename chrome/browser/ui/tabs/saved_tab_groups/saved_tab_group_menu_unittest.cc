// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_menu.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/button_menu_item_model.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button.h"
#include "url/gurl.h"

namespace {
void test_callback(const content::OpenURLParams& params) {}
}  // anonymous namespace

class TestMenuButton : public views::MenuButton {
 public:
  TestMenuButton() = default;
  TestMenuButton(const TestMenuButton&) = delete;
  TestMenuButton& operator=(const TestMenuButton&) = delete;

 private:
  void ButtonPressed(const ui::Event& event) { return; }
};

// Serves to test the functions in SavedTabGroupMenu.
class SavedTabGroupMenuTest : public ChromeViewsTestBase {
 protected:
  SavedTabGroupMenuTest() = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    test_menu_button_ = std::make_unique<TestMenuButton>();
    test_saved_tab_group_ = std::make_unique<SavedTabGroup>(
        SavedTabGroup(tab_groups::TabGroupId::GenerateNew(), u"Group Title",
                      tab_groups::TabGroupColorId::kGreen,
                      {CreateSavedTabGroupTab("Test_Link", u"Test Tab",
                                              favicon::GetDefaultFavicon())}));
    saved_tab_group_menu_ =
        std::make_unique<SavedTabGroupMenu>(test_saved_tab_group_.get());
    saved_tab_group_menu_->RunMenu(CreateTestWidget().get(),
                                   button_controller(), gfx::Rect(),
                                   base::BindOnce(test_callback));

    ASSERT_EQ(test_saved_tab_group_->saved_tabs.size(),
              static_cast<size_t>(saved_tab_group_menu_->GetItemCount()));
  }

  void TearDown() override {
    ChromeViewsTestBase::TearDown();
    test_saved_tab_group_.reset();
    saved_tab_group_menu_.reset();
  }

  SavedTabGroupTab CreateSavedTabGroupTab(const std::string& url,
                                          const std::u16string& tab_title,
                                          const gfx::Image& favicon) {
    return SavedTabGroupTab(GURL(url), tab_title, favicon);
  }

  void TestWithMultipleTabs() {
    test_saved_tab_group_.reset();
    saved_tab_group_menu_.reset();
    test_saved_tab_group_ = std::make_unique<SavedTabGroup>(
        SavedTabGroup(tab_groups::TabGroupId::GenerateNew(), u"Group Title",
                      tab_groups::TabGroupColorId::kGreen,
                      {CreateSavedTabGroupTab("1_link", u"First Tab",
                                              favicon::GetDefaultFavicon()),
                       CreateSavedTabGroupTab("2_link", u"Second Tab",
                                              favicon::GetDefaultFavicon()),
                       CreateSavedTabGroupTab("3_link", u"Third Tab",
                                              favicon::GetDefaultFavicon())}));
    saved_tab_group_menu_ =
        std::make_unique<SavedTabGroupMenu>(test_saved_tab_group_.get());
    saved_tab_group_menu_->RunMenu(CreateTestWidget().get(),
                                   button_controller(), gfx::Rect(),
                                   base::BindOnce(test_callback));
  }

  views::MenuButtonController* button_controller() {
    return test_menu_button_->button_controller();
  }

  std::unique_ptr<SavedTabGroupMenu> saved_tab_group_menu_;
  std::unique_ptr<SavedTabGroup> test_saved_tab_group_;
  std::unique_ptr<TestMenuButton> test_menu_button_;
};

// Verifies that each item in our menu holds the correct information.
TEST_F(SavedTabGroupMenuTest, MenuItemHoldsCorrectInformation) {
  EXPECT_EQ(test_saved_tab_group_->saved_tabs[0].tab_title,
            saved_tab_group_menu_->GetLabelAt(0));
  EXPECT_EQ(
      ui::ImageModel::FromImage(test_saved_tab_group_->saved_tabs[0].favicon),
      saved_tab_group_menu_->GetIconAt(0));

  TestWithMultipleTabs();
  for (size_t i = 0; i < test_saved_tab_group_->saved_tabs.size(); i++) {
    EXPECT_EQ(test_saved_tab_group_->saved_tabs[i].tab_title,
              saved_tab_group_menu_->GetLabelAt(i));
    EXPECT_EQ(
        ui::ImageModel::FromImage(test_saved_tab_group_->saved_tabs[i].favicon),
        saved_tab_group_menu_->GetIconAt(i));
  }
}

// Verifies that the items in our menu corresponds to the correct command_id.
TEST_F(SavedTabGroupMenuTest, MenuItemsHoldCorrectCommandIds) {
  int command_id = saved_tab_group_menu_->GetCommandIdAt(0);
  EXPECT_EQ(0, command_id);
  EXPECT_TRUE(saved_tab_group_menu_->IsCommandIdEnabled(command_id));

  TestWithMultipleTabs();
  for (size_t i = 0; i < test_saved_tab_group_->saved_tabs.size(); i++) {
    int command_id = saved_tab_group_menu_->GetCommandIdAt(i);
    EXPECT_EQ(static_cast<int>(i), command_id);
    EXPECT_TRUE(saved_tab_group_menu_->IsCommandIdEnabled(command_id));
  }
}

// Verifies that calling ExecuteCommand calls our callback and can only be
// called once.
TEST_F(SavedTabGroupMenuTest, ExecuteCommandCanOnlyBeCalledOnce) {
  static bool opened;
  struct OpenUrl {
    static void callback(const content::OpenURLParams& params) {
      opened = true;
    }
  };

  saved_tab_group_menu_->RunMenu(CreateTestWidget().get(), button_controller(),
                                 gfx::Rect(),
                                 base::BindOnce(OpenUrl::callback));
  saved_tab_group_menu_->ExecuteCommand(0, 0);
  EXPECT_TRUE(opened);
}
