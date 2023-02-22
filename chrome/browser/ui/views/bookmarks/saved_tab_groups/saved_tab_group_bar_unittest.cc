// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_bar.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_button.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_overflow_button.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/views/view_utils.h"

namespace {

SavedTabGroup kSavedTabGroup1(std::u16string(u"test_title_1"),
                              tab_groups::TabGroupColorId::kGrey,
                              {});

SavedTabGroup kSavedTabGroup2(std::u16string(u"test_title_2"),
                              tab_groups::TabGroupColorId::kGrey,
                              {});
SavedTabGroup kSavedTabGroup3(std::u16string(u"test_title_3"),
                              tab_groups::TabGroupColorId::kGrey,
                              {});

SavedTabGroup kSavedTabGroup4(std::u16string(u"test_title_4"),
                              tab_groups::TabGroupColorId::kGrey,
                              {});

SavedTabGroup kSavedTabGroup5(std::u16string(u"test_title_5"),
                              tab_groups::TabGroupColorId::kGrey,
                              {});

std::u16string kNewTitle(u"kNewTitle");

tab_groups::TabGroupColorId kNewColor = tab_groups::TabGroupColorId::kRed;

}  // anonymous namespace

class SavedTabGroupBarUnitTest : public ChromeViewsTestBase {
 public:
  SavedTabGroupBarUnitTest()
      : saved_tab_group_model_(std::make_unique<SavedTabGroupModel>()),
        button_padding_(GetLayoutConstant(TOOLBAR_ELEMENT_PADDING)),
        button_height_(GetLayoutConstant(BOOKMARK_BAR_BUTTON_HEIGHT)) {}

  SavedTabGroupBar* saved_tab_group_bar() { return saved_tab_group_bar_.get(); }
  SavedTabGroupModel* saved_tab_group_model() {
    return saved_tab_group_model_.get();
  }

  int button_padding() { return button_padding_; }

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    saved_tab_group_model_ = std::make_unique<SavedTabGroupModel>();
    saved_tab_group_bar_ = std::make_unique<SavedTabGroupBar>(
        nullptr, saved_tab_group_model(), false);
  }

  void TearDown() override {
    saved_tab_group_bar_.reset();
    saved_tab_group_model_.reset();

    ChromeViewsTestBase::TearDown();
  }

  void Add4Groups() {
    saved_tab_group_model_->Add(kSavedTabGroup1);
    saved_tab_group_model_->Add(kSavedTabGroup2);
    saved_tab_group_model_->Add(kSavedTabGroup3);
    saved_tab_group_model_->Add(kSavedTabGroup4);
  }

  int GetWidthOfButtonsAndPadding() {
    // iterate through bubble getting size plus button padding
    // calculated button_sizes + extra_padding
    int size = 0;
    for (const auto* const button : saved_tab_group_bar_->children()) {
      size += button->GetVisible()
                  ? button->GetPreferredSize().width() + button_padding_
                  : 0;
    }

    return size;
  }

 private:
  std::unique_ptr<SavedTabGroupBar> saved_tab_group_bar_;
  std::unique_ptr<SavedTabGroupModel> saved_tab_group_model_;

  const int button_padding_;
  const int button_height_;
};

TEST_F(SavedTabGroupBarUnitTest, AddsButtonFromModelAdd) {
  // Verify the initial count of saved tab group buttons. Even when visibly
  // empty, the SavedTabGroupBar still contains an invisible overflow menu
  // that is invisible.
  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  saved_tab_group_model()->Add(kSavedTabGroup1);
  EXPECT_EQ(2u, saved_tab_group_bar()->children().size());

  saved_tab_group_model()->AddedFromSync(kSavedTabGroup2);
  EXPECT_EQ(3u, saved_tab_group_bar()->children().size());
}

TEST_F(SavedTabGroupBarUnitTest, OverflowMenuVisibleWhenFifthButtonAdded) {
  // The first view should be an invisible overflow menu.
  ASSERT_EQ(1u, saved_tab_group_bar()->children().size());

  const views::View* overflow_button = saved_tab_group_bar()->children()[0];
  EXPECT_FALSE(overflow_button->GetVisible());

  // Verify the overflow button is still hidden.
  Add4Groups();
  EXPECT_FALSE(overflow_button->GetVisible());
  EXPECT_EQ(5u, saved_tab_group_bar()->children().size());

  // Verify that the overflow button is visible when a 5th button is added and
  // that the 5th button is not visible.
  saved_tab_group_model()->Add(kSavedTabGroup5);
  EXPECT_TRUE(overflow_button->GetVisible());
  EXPECT_FALSE(saved_tab_group_bar()->children()[4]->GetVisible());
  EXPECT_EQ(6u, saved_tab_group_bar()->children().size());
}

// Verifies that when a 5th saved group is removed, the overflow menu is not
// visible.
TEST_F(SavedTabGroupBarUnitTest, OverflowMenuHiddenWhenFifthButtonRemoved) {
  // The first view should be an invisible overflow menu.
  ASSERT_EQ(1u, saved_tab_group_bar()->children().size());

  const views::View* overflow_button = saved_tab_group_bar()->children()[0];
  EXPECT_FALSE(overflow_button->GetVisible());

  // Verify that the overflow button is visible when a 5th button is added and
  // that the 5th button is not visible.
  Add4Groups();
  saved_tab_group_model()->Add(kSavedTabGroup5);
  EXPECT_TRUE(overflow_button->GetVisible());
  EXPECT_FALSE(saved_tab_group_bar()->children()[4]->GetVisible());
  EXPECT_EQ(6u, saved_tab_group_bar()->children().size());

  saved_tab_group_model()->Remove(kSavedTabGroup5.saved_guid());

  EXPECT_FALSE(overflow_button->GetVisible());
  EXPECT_EQ(5u, saved_tab_group_bar()->children().size());
}

// Verifies that when a 5th saved group is added and the first group is removed,
// the overflow menu is not visible and the 5th button is visible.
TEST_F(SavedTabGroupBarUnitTest, OverflowMenuHiddenWhenFirstButtonRemoved) {
  // The first view should be an invisible overflow menu.
  ASSERT_EQ(1u, saved_tab_group_bar()->children().size());

  const views::View* overflow_button = saved_tab_group_bar()->children()[0];
  EXPECT_FALSE(overflow_button->GetVisible());

  // Verify that the overflow button is visible when a 5th button is added and
  // that the 5th button is not visible.
  Add4Groups();
  saved_tab_group_model()->Add(kSavedTabGroup5);
  EXPECT_TRUE(overflow_button->GetVisible());
  EXPECT_FALSE(saved_tab_group_bar()->children()[4]->GetVisible());
  EXPECT_EQ(6u, saved_tab_group_bar()->children().size());

  saved_tab_group_model()->Remove(kSavedTabGroup5.saved_guid());

  EXPECT_FALSE(overflow_button->GetVisible());
  EXPECT_TRUE(saved_tab_group_bar()->children()[3]->GetVisible());
  EXPECT_EQ(5u, saved_tab_group_bar()->children().size());
}

TEST_F(SavedTabGroupBarUnitTest, BarsWithSameModelsHaveSameButtons) {
  saved_tab_group_model()->Add(kSavedTabGroup1);

  SavedTabGroupBar another_tab_group_bar_on_same_model(
      nullptr, saved_tab_group_model(), false);

  EXPECT_EQ(saved_tab_group_bar()->children().size(),
            another_tab_group_bar_on_same_model.children().size());
}

TEST_F(SavedTabGroupBarUnitTest, RemoveButtonFromModelRemove) {
  saved_tab_group_model()->Add(kSavedTabGroup1);

  // Remove the group and expect no buttons except the overflow menu.
  saved_tab_group_model()->Remove(kSavedTabGroup1.saved_guid());
  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());
  EXPECT_TRUE(views::IsViewClass<SavedTabGroupOverflowButton>(
      saved_tab_group_bar()->children()[0]));

  saved_tab_group_model()->AddedFromSync(kSavedTabGroup1);

  // Remove the group and expect no buttons.
  saved_tab_group_model()->RemovedFromSync(kSavedTabGroup1.saved_guid());
  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());
  EXPECT_TRUE(views::IsViewClass<SavedTabGroupOverflowButton>(
      saved_tab_group_bar()->children()[0]));
}

TEST_F(SavedTabGroupBarUnitTest, UpdatedVisualDataMakesChangeToSpecificView) {
  saved_tab_group_model()->Add(kSavedTabGroup1);
  saved_tab_group_model()->AddedFromSync(kSavedTabGroup2);

  tab_groups::TabGroupVisualData saved_tab_group_visual_data(kNewTitle,
                                                             kNewColor);

  // Update the visual_data and expect the first button to be updated and the
  // second button to stay the same.
  saved_tab_group_model()->UpdateVisualData(kSavedTabGroup1.saved_guid(),
                                            &saved_tab_group_visual_data);
  saved_tab_group_model()->UpdatedVisualDataFromSync(
      kSavedTabGroup2.saved_guid(), &saved_tab_group_visual_data);

  SavedTabGroupButton* new_button_1 = views::AsViewClass<SavedTabGroupButton>(
      saved_tab_group_bar()->children()[0]);
  SavedTabGroupButton* new_button_2 = views::AsViewClass<SavedTabGroupButton>(
      saved_tab_group_bar()->children()[1]);

  ASSERT_TRUE(!!new_button_1);
  ASSERT_TRUE(!!new_button_2);

  EXPECT_EQ(new_button_1->GetText(), kNewTitle);
  EXPECT_EQ(new_button_1->tab_group_color_id(), kNewColor);
  EXPECT_EQ(new_button_2->GetText(), kNewTitle);
  EXPECT_EQ(new_button_2->tab_group_color_id(), kNewColor);
}

TEST_F(SavedTabGroupBarUnitTest, MoveButtonFromModelMove) {
  saved_tab_group_model()->Add(kSavedTabGroup1);
  saved_tab_group_model()->Add(kSavedTabGroup2);

  const auto& button_list = saved_tab_group_bar()->children();
  views::View* button_1 = button_list[0];

  // move the tab and expect the one that was moved to be in the expected
  // position.
  saved_tab_group_model()->Reorder(kSavedTabGroup1.saved_guid(), 1);
  EXPECT_EQ(3u, saved_tab_group_bar()->children().size());
  EXPECT_EQ(button_1, saved_tab_group_bar()->children()[1]);
}

// If the restriction is exactly the expected size all should be visible
TEST_F(SavedTabGroupBarUnitTest, CalculatePreferredWidthRestrictedByExactSize) {
  Add4Groups();

  int exact_width = GetWidthOfButtonsAndPadding();

  EXPECT_EQ(
      exact_width,
      saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(exact_width));

  // After 4 buttons have been added (excluding the invisible overflow), all
  // subsequent buttons will be hidden. Instead an overflow menu will appear
  // which will house the hidden buttons.
  saved_tab_group_model()->Add(kSavedTabGroup5);

  int new_width = GetWidthOfButtonsAndPadding();

  EXPECT_LT(exact_width, new_width);
  EXPECT_EQ(
      new_width,
      saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(new_width));
}

// If the restriction is more than the expected size all should be visible
TEST_F(SavedTabGroupBarUnitTest,
       CalculatePreferredWidthRestrictedByLargerSize) {
  Add4Groups();
  int exact_width = GetWidthOfButtonsAndPadding();

  EXPECT_EQ(exact_width,
            saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(
                exact_width + 1));

  // After 4 buttons have been added (excluding the invisible overflow), all
  // subsequent buttons will be hidden. Instead an overflow menu will appear
  // which will house the hidden buttons.
  saved_tab_group_model()->Add(kSavedTabGroup5);

  int new_width = GetWidthOfButtonsAndPadding();

  EXPECT_LT(exact_width, new_width);
  EXPECT_EQ(new_width,
            saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(
                new_width + 1));
}

// If the restriction is 1 less than the size the last button should not be
// visible, and second to last should be visible.
TEST_F(SavedTabGroupBarUnitTest,
       CalculatePreferredWidthRestrictedBySmallerSize) {
  Add4Groups();
  int exact_width = GetWidthOfButtonsAndPadding();

  EXPECT_GT(exact_width,
            saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(
                exact_width - 1));

  // After 4 buttons have been added (excluding the invisible overflow), all
  // subsequent buttons will be hidden. Instead an overflow menu will appear
  // which will house the hidden buttons.
  saved_tab_group_model()->Add(kSavedTabGroup5);

  int new_width = GetWidthOfButtonsAndPadding();

  EXPECT_LT(exact_width, new_width);
  EXPECT_GT(new_width,
            saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(
                new_width - 1));
}
