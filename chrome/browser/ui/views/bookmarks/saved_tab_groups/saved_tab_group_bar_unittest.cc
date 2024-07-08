// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_bar.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_button.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_overflow_button.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/saved_tab_groups/features.h"
#include "components/saved_tab_groups/saved_tab_group_test_utils.h"
#include "components/saved_tab_groups/types.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/view_utils.h"

namespace tab_groups {
namespace {

const SavedTabGroup kSavedTabGroup1(std::u16string(u"test_title_1"),
                                    tab_groups::TabGroupColorId::kGrey,
                                    {},
                                    std::nullopt);

const SavedTabGroup kSavedTabGroup2(std::u16string(u"test_title_2"),
                                    tab_groups::TabGroupColorId::kGrey,
                                    {},
                                    std::nullopt);

const SavedTabGroup kSavedTabGroup3(std::u16string(u"test_title_3"),
                                    tab_groups::TabGroupColorId::kGrey,
                                    {},
                                    std::nullopt);

const SavedTabGroup kSavedTabGroup4(std::u16string(u"test_title_4"),
                                    tab_groups::TabGroupColorId::kGrey,
                                    {},
                                    std::nullopt);

const SavedTabGroup kSavedTabGroup5(std::u16string(u"test_title_5"),
                                    tab_groups::TabGroupColorId::kGrey,
                                    {},
                                    std::nullopt);

const std::u16string kNewTitle(u"kNewTitle");

const tab_groups::TabGroupColorId kNewColor = tab_groups::TabGroupColorId::kRed;

}  // anonymous namespace

class SavedTabGroupBarUnitTest : public ChromeViewsTestBase,
                                 public ::testing::WithParamInterface<bool> {
 public:
  SavedTabGroupBarUnitTest()
      : saved_tab_group_model_(std::make_unique<SavedTabGroupModel>()) {
    if (IsV2UIEnabled()) {
      feature_list_.InitWithFeatures({tab_groups::kTabGroupsSaveUIUpdate}, {});
    } else {
      feature_list_.InitWithFeatures({}, {tab_groups::kTabGroupsSaveUIUpdate});
    }
  }

  bool IsV2UIEnabled() const { return GetParam(); }
  SavedTabGroupBar* saved_tab_group_bar() { return saved_tab_group_bar_.get(); }
  SavedTabGroupModel* saved_tab_group_model() {
    return saved_tab_group_model_.get();
  }

  int button_padding() { return button_padding_; }

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    CreateBrowser();

    saved_tab_group_model_ = std::make_unique<SavedTabGroupModel>();
    saved_tab_group_bar_ = std::make_unique<SavedTabGroupBar>(
        browser(), saved_tab_group_model(), false);

    saved_tab_group_bar_->SetPageNavigator(nullptr);
  }

  void TearDown() override {
    saved_tab_group_bar_.reset();
    saved_tab_group_model_.reset();
    browser_window_.reset();
    browser_.reset();
    profile_.reset();

    ChromeViewsTestBase::TearDown();
  }

  void Add4Groups() {
    saved_tab_group_model_->Add(kSavedTabGroup1);
    saved_tab_group_model_->Add(kSavedTabGroup2);
    saved_tab_group_model_->Add(kSavedTabGroup3);
    saved_tab_group_model_->Add(kSavedTabGroup4);
  }

  int GetWidthOfButtonsAndPadding() {
    int size = 0;

    // Iterate through bubble getting size plus button padding calculated
    // button_sizes + extra_padding
    for (const views::View* const button : saved_tab_group_bar_->children()) {
      size += button->GetVisible()
                  ? button->GetPreferredSize().width() + button_padding_
                  : 0;
    }

    return size;
  }

  void CreateBrowser() {
    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();
    browser_window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params(profile_.get(), /*user_gesture*/ true);
    params.type = Browser::TYPE_NORMAL;
    params.window = browser_window_.get();
    browser_.reset(Browser::Create(params));
  }

  std::vector<base::Uuid> GetButtonGUIDs() {
    std::vector<base::Uuid> guids;
    for (views::View* view : saved_tab_group_bar()->children()) {
      const SavedTabGroupButton* button =
          views::AsViewClass<SavedTabGroupButton>(view);
      if (!button) {
        continue;
      }

      guids.push_back(button->guid());
    }

    // Also check that we found the right number of buttons and that they're
    // contiguous at the start of `children()`.
    const size_t num_children = saved_tab_group_bar()->children().size();
    EXPECT_EQ(guids.size(), num_children - 1);
    EXPECT_NE(views::AsViewClass<SavedTabGroupOverflowButton>(
                  saved_tab_group_bar()->children()[num_children - 1]),
              nullptr);

    return guids;
  }

  Browser* browser() { return browser_.get(); }

 private:
  std::unique_ptr<SavedTabGroupBar> saved_tab_group_bar_;
  std::unique_ptr<SavedTabGroupModel> saved_tab_group_model_;

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;

  static constexpr int button_padding_ = 8;
  static constexpr int button_height_ = 20;
};

class STGEverythingMenuUnitTest : public SavedTabGroupBarUnitTest {
 public:
  void SetUp() override {
    SavedTabGroupBarUnitTest::SetUp();
    everything_menu_ = std::make_unique<STGEverythingMenu>(nullptr, browser());
  }

  void TearDown() override {
    everything_menu_.reset();
    SavedTabGroupBarUnitTest::TearDown();
  }

  SavedTabGroupModel* saved_tab_group_model_from_browser() {
    return const_cast<SavedTabGroupModel*>(
        everything_menu_->GetSavedTabGroupModelFromBrowser());
  }

  std::unique_ptr<ui::SimpleMenuModel> menu_model() {
    return everything_menu_->CreateMenuModel();
  }

 protected:
  // Used to mock time elapsed between two tab groups creation.
  static constexpr base::TimeDelta interval_ = base::Seconds(3);

  std::unique_ptr<STGEverythingMenu> everything_menu_;
};

TEST_P(STGEverythingMenuUnitTest, TabGroupItemsSortedByCreationTime) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  // Only the "Create new tab group" is added.
  EXPECT_EQ(menu_model()->GetItemCount(), 1u);

  // Add three test groups.
  const SavedTabGroup saved_tab_group1(std::u16string(u"test_title_1"),
                                       tab_groups::TabGroupColorId::kGrey, {},
                                       std::nullopt);
  saved_tab_group_model_from_browser()->Add(saved_tab_group1);

  task_environment()->FastForwardBy(interval_);
  const SavedTabGroup saved_tab_group2(std::u16string(u"test_title_2"),
                                       tab_groups::TabGroupColorId::kGrey, {},
                                       std::nullopt);
  saved_tab_group_model_from_browser()->Add(saved_tab_group2);

  task_environment()->FastForwardBy(interval_);
  const SavedTabGroup saved_tab_group3(std::u16string(u"test_title_3"),
                                       tab_groups::TabGroupColorId::kGrey, {},
                                       std::nullopt);
  saved_tab_group_model_from_browser()->Add(saved_tab_group3);

  // A separator is also added.
  auto model = menu_model();
  EXPECT_EQ(model->GetItemCount(), 5u);

  // The three added tab group items starts from model index 2. They are sorted
  // by most recent created first.
  EXPECT_EQ(model->GetLabelAt(2), u"test_title_3");
  EXPECT_EQ(model->GetLabelAt(3), u"test_title_2");
  EXPECT_EQ(model->GetLabelAt(4), u"test_title_1");
}

TEST_P(SavedTabGroupBarUnitTest, AddsButtonFromModelAdd) {
  // Verify the initial count of saved tab group buttons. Even when visibly
  // empty, the SavedTabGroupBar still contains an invisible overflow menu
  // that is invisible.
  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  saved_tab_group_model()->Add(kSavedTabGroup1);
  EXPECT_EQ(IsV2UIEnabled() ? 1u : 2u,
            saved_tab_group_bar()->children().size());

  SavedTabGroup group_2_with_position = kSavedTabGroup2;
  group_2_with_position.SetPosition(1);
  saved_tab_group_model()->AddedFromSync(group_2_with_position);
  EXPECT_EQ(IsV2UIEnabled() ? 2u : 3u,
            saved_tab_group_bar()->children().size());
}

TEST_P(SavedTabGroupBarUnitTest, EverthingButtonAlwaysVisibleForV2) {
  // Verify the initial count of saved tab group buttons.
  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  const views::View* overflow_button = saved_tab_group_bar()->children()[0];
  if (IsV2UIEnabled()) {
    // Everything button shows by default.
    saved_tab_group_bar()->SetBounds(
        0, 2, saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(400),
        2);
    EXPECT_TRUE(overflow_button->GetVisible());

    // Add a tab group button; the Everything button is still there.
    saved_tab_group_model()->Add(kSavedTabGroup1);
    saved_tab_group_bar()->SetBounds(
        0, 2, saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(400),
        2);
    EXPECT_TRUE(overflow_button->GetVisible());

    // Remove the last tab group button; the Everything button is still there.
    saved_tab_group_model()->Remove(kSavedTabGroup1.saved_guid());
    saved_tab_group_bar()->SetBounds(
        0, 2, saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(400),
        2);
    EXPECT_TRUE(overflow_button->GetVisible());
  } else {
    saved_tab_group_bar()->SetBounds(
        0, 2, saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(400),
        2);
    EXPECT_FALSE(overflow_button->GetVisible());
  }
}

TEST_P(SavedTabGroupBarUnitTest, OverflowMenuVisibleWhenFifthButtonAdded) {
  if (IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V2";
  }

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

  // Layout the buttons.
  saved_tab_group_bar()->SetBounds(
      0, 2, saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(400), 2);

  EXPECT_TRUE(overflow_button->GetVisible());
  EXPECT_FALSE(saved_tab_group_bar()->children()[4]->GetVisible());
  EXPECT_EQ(6u, saved_tab_group_bar()->children().size());
}

// Verifies that when a 5th saved group is removed, the overflow menu is not
// visible.
TEST_P(SavedTabGroupBarUnitTest, OverflowMenuHiddenWhenFifthButtonRemoved) {
  if (IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V2";
  }

  // The first view should be an invisible overflow menu.
  ASSERT_EQ(1u, saved_tab_group_bar()->children().size());

  const views::View* overflow_button = saved_tab_group_bar()->children()[0];
  EXPECT_FALSE(overflow_button->GetVisible());

  // Verify that the overflow button is visible when a 5th button is added and
  // that the 5th button is not visible.
  Add4Groups();
  saved_tab_group_model()->Add(kSavedTabGroup5);

  // Layout the buttons.
  saved_tab_group_bar()->SetBounds(
      0, 2, saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(400), 2);

  EXPECT_TRUE(overflow_button->GetVisible());
  EXPECT_FALSE(saved_tab_group_bar()->children()[4]->GetVisible());
  EXPECT_EQ(6u, saved_tab_group_bar()->children().size());

  saved_tab_group_model()->Remove(kSavedTabGroup5.saved_guid());

  // Layout the buttons.
  saved_tab_group_bar()->SetBounds(
      0, 2, saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(400), 2);

  EXPECT_FALSE(overflow_button->GetVisible());
  EXPECT_EQ(5u, saved_tab_group_bar()->children().size());
}

// Verifies that when a 5th saved group is added and the first group is removed,
// the overflow menu is not visible and the 5th button is visible.
TEST_P(SavedTabGroupBarUnitTest, OverflowMenuHiddenWhenFirstButtonRemoved) {
  if (IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V2";
  }

  // The first view should be an invisible overflow menu.
  ASSERT_EQ(1u, saved_tab_group_bar()->children().size());

  const views::View* overflow_button = saved_tab_group_bar()->children()[0];
  EXPECT_FALSE(overflow_button->GetVisible());

  // Verify that the overflow button is visible when a 5th button is added and
  // that the 5th button is not visible.
  Add4Groups();
  saved_tab_group_model()->Add(kSavedTabGroup5);

  // Layout the buttons.
  saved_tab_group_bar()->SetBounds(
      0, 2, saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(400), 2);

  EXPECT_TRUE(overflow_button->GetVisible());
  EXPECT_FALSE(saved_tab_group_bar()->children()[4]->GetVisible());
  EXPECT_EQ(6u, saved_tab_group_bar()->children().size());

  saved_tab_group_model()->Remove(kSavedTabGroup5.saved_guid());

  // Layout the buttons.
  saved_tab_group_bar()->SetBounds(
      0, 2, saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(400), 2);

  EXPECT_FALSE(overflow_button->GetVisible());
  EXPECT_TRUE(saved_tab_group_bar()->children()[3]->GetVisible());
  EXPECT_EQ(5u, saved_tab_group_bar()->children().size());
}

TEST_P(SavedTabGroupBarUnitTest, BarsWithSameModelsHaveSameButtons) {
  saved_tab_group_model()->Add(kSavedTabGroup1);

  SavedTabGroupBar another_tab_group_bar_on_same_model(
      browser(), saved_tab_group_model(), false);

  EXPECT_EQ(saved_tab_group_bar()->children().size(),
            another_tab_group_bar_on_same_model.children().size());
}

TEST_P(SavedTabGroupBarUnitTest, RemoveButtonFromModelRemove) {
  saved_tab_group_model()->Add(kSavedTabGroup1);

  // Remove the group and expect no buttons except the overflow menu.
  saved_tab_group_model()->Remove(kSavedTabGroup1.saved_guid());
  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());
  EXPECT_TRUE(views::IsViewClass<SavedTabGroupOverflowButton>(
      saved_tab_group_bar()->children()[0]));

  SavedTabGroup group_1_with_position = kSavedTabGroup1;
  group_1_with_position.SetPosition(1);
  saved_tab_group_model()->AddedFromSync(group_1_with_position);

  // Remove the group and expect no buttons.
  saved_tab_group_model()->RemovedFromSync(kSavedTabGroup1.saved_guid());
  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());
  EXPECT_TRUE(views::IsViewClass<SavedTabGroupOverflowButton>(
      saved_tab_group_bar()->children()[0]));
}

TEST_P(SavedTabGroupBarUnitTest, UpdatedVisualDataMakesChangeToSpecificView) {
  saved_tab_group_model()->Add(kSavedTabGroup1);
  const LocalTabGroupID local_group_id_1 = test::GenerateRandomTabGroupID();
  saved_tab_group_model()->OnGroupOpenedInTabStrip(kSavedTabGroup1.saved_guid(),
                                                   local_group_id_1);

  SavedTabGroup group_2_with_position = kSavedTabGroup2;
  group_2_with_position.SetPosition(1);
  saved_tab_group_model()->AddedFromSync(group_2_with_position);

  tab_groups::TabGroupVisualData saved_tab_group_visual_data(kNewTitle,
                                                             kNewColor);

  // Update the visual_data and expect the first button to be updated and the
  // second button to stay the same.
  saved_tab_group_model()->UpdateVisualData(local_group_id_1,
                                            &saved_tab_group_visual_data);
  saved_tab_group_model()->UpdatedVisualDataFromSync(
      kSavedTabGroup2.saved_guid(), &saved_tab_group_visual_data);

  SavedTabGroupButton* new_button_1 = views::AsViewClass<SavedTabGroupButton>(
      saved_tab_group_bar()->children()[0]);
  SavedTabGroupButton* new_button_2 = views::AsViewClass<SavedTabGroupButton>(
      saved_tab_group_bar()->children()[1]);

  if (IsV2UIEnabled()) {
    ASSERT_TRUE(!!new_button_1);
    ASSERT_FALSE(!!new_button_2);

    EXPECT_EQ(new_button_1->GetText(), kNewTitle);
    EXPECT_EQ(new_button_1->tab_group_color_id(), kNewColor);
  } else {
    ASSERT_TRUE(!!new_button_1);
    ASSERT_TRUE(!!new_button_2);

    EXPECT_EQ(new_button_1->GetText(), kNewTitle);
    EXPECT_EQ(new_button_1->tab_group_color_id(), kNewColor);
    EXPECT_EQ(new_button_2->GetText(), kNewTitle);
    EXPECT_EQ(new_button_2->tab_group_color_id(), kNewColor);
  }
}

TEST_P(SavedTabGroupBarUnitTest, MoveButtonFromModelMove) {
  const base::Uuid guid_1 = kSavedTabGroup1.saved_guid();
  const base::Uuid guid_2 = kSavedTabGroup2.saved_guid();
  const base::Uuid guid_3 = kSavedTabGroup3.saved_guid();

  if (IsV2UIEnabled()) {
    SavedTabGroup group1 = kSavedTabGroup1;
    group1.SetPinned(true);
    SavedTabGroup group2 = kSavedTabGroup2;
    group2.SetPinned(true);
    SavedTabGroup group3 = kSavedTabGroup3;
    group3.SetPinned(true);
    saved_tab_group_model()->Add(group1);
    saved_tab_group_model()->Add(group2);
    saved_tab_group_model()->Add(group3);

    ASSERT_THAT(GetButtonGUIDs(), testing::ElementsAre(guid_3, guid_2, guid_1));
    saved_tab_group_model()->ReorderGroupLocally(kSavedTabGroup2.saved_guid(),
                                                 2);
    EXPECT_THAT(GetButtonGUIDs(), testing::ElementsAre(guid_3, guid_1, guid_2));
    saved_tab_group_model()->ReorderGroupLocally(kSavedTabGroup2.saved_guid(),
                                                 0);
    EXPECT_THAT(GetButtonGUIDs(), testing::ElementsAre(guid_2, guid_3, guid_1));
    saved_tab_group_model()->ReorderGroupLocally(kSavedTabGroup2.saved_guid(),
                                                 1);
    EXPECT_THAT(GetButtonGUIDs(), testing::ElementsAre(guid_3, guid_2, guid_1));
  } else {
    saved_tab_group_model()->Add(kSavedTabGroup1);
    saved_tab_group_model()->Add(kSavedTabGroup2);
    saved_tab_group_model()->Add(kSavedTabGroup3);

    ASSERT_THAT(GetButtonGUIDs(), testing::ElementsAre(guid_1, guid_2, guid_3));
    saved_tab_group_model()->ReorderGroupLocally(kSavedTabGroup2.saved_guid(),
                                                 2);
    EXPECT_THAT(GetButtonGUIDs(), testing::ElementsAre(guid_1, guid_3, guid_2));
    saved_tab_group_model()->ReorderGroupLocally(kSavedTabGroup2.saved_guid(),
                                                 0);
    EXPECT_THAT(GetButtonGUIDs(), testing::ElementsAre(guid_2, guid_1, guid_3));
    saved_tab_group_model()->ReorderGroupLocally(kSavedTabGroup2.saved_guid(),
                                                 1);
    EXPECT_THAT(GetButtonGUIDs(), testing::ElementsAre(guid_1, guid_2, guid_3));
  }
}

// If the restriction is exactly the expected size all should be visible
TEST_P(SavedTabGroupBarUnitTest, CalculatePreferredWidthRestrictedByExactSize) {
  if (IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V2";
  }

  Add4Groups();

  int exact_width = GetWidthOfButtonsAndPadding();
  int calculated_width =
      saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(exact_width);
  EXPECT_EQ(exact_width, calculated_width);

  // After 4 buttons have been added (excluding the invisible overflow), all
  // subsequent buttons will be hidden. Instead an overflow menu will appear
  // which will house the hidden buttons.
  saved_tab_group_model()->Add(kSavedTabGroup5);

  // Layout the buttons.
  saved_tab_group_bar()->SetBounds(
      0, 2, saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(400), 2);

  // Update the `new_width` to take the buttons into account.
  int new_width = GetWidthOfButtonsAndPadding();
  calculated_width =
      saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(new_width);

  EXPECT_LT(exact_width, new_width);
  EXPECT_EQ(new_width, calculated_width);
}

// If the restriction is more than the expected size all should be visible
TEST_P(SavedTabGroupBarUnitTest,
       CalculatePreferredWidthRestrictedByLargerSize) {
  if (IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V2";
  }

  Add4Groups();
  int exact_width = GetWidthOfButtonsAndPadding();

  EXPECT_EQ(exact_width,
            saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(
                exact_width + 1));

  // After 4 buttons have been added (excluding the invisible overflow), all
  // subsequent buttons will be hidden. Instead an overflow menu will appear
  // which will house the hidden buttons.
  saved_tab_group_model()->Add(kSavedTabGroup5);

  // Layout the buttons.
  saved_tab_group_bar()->SetBounds(
      0, 2, saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(400), 2);
  int new_width = GetWidthOfButtonsAndPadding();
  int actual_width =
      saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(new_width + 1);

  EXPECT_LT(exact_width, new_width);
  EXPECT_EQ(new_width, actual_width);
}

// If the restriction is 1 less than the size the last button should not be
// visible, and second to last should be visible.
TEST_P(SavedTabGroupBarUnitTest,
       CalculatePreferredWidthRestrictedBySmallerSize) {
  if (IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V2";
  }

  Add4Groups();
  int exact_width = GetWidthOfButtonsAndPadding();

  EXPECT_GT(exact_width,
            saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(
                exact_width - 1));

  // After 4 buttons have been added (excluding the invisible overflow), all
  // subsequent buttons will be hidden. Instead an overflow menu will appear
  // which will house the hidden buttons.
  saved_tab_group_model()->Add(kSavedTabGroup5);

  // Layout the buttons.
  saved_tab_group_bar()->SetBounds(
      0, 2, saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(400), 2);
  int new_width = GetWidthOfButtonsAndPadding();
  int actual_width =
      saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(new_width - 1);

  EXPECT_LT(exact_width, new_width);
  EXPECT_GT(new_width, actual_width);
}

// Verify add pinned tab group will add a button.
TEST_P(SavedTabGroupBarUnitTest, AddPinnedTabGroupButton) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V2";
  }

  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  SavedTabGroup group1 = kSavedTabGroup1;
  group1.SetPinned(true);

  saved_tab_group_model()->Add(group1);
  EXPECT_EQ(2u, saved_tab_group_bar()->children().size());
  EXPECT_TRUE(!!views::AsViewClass<SavedTabGroupButton>(
      saved_tab_group_bar()->children()[0]));
}

// Verify pin an existing tab group will add a button.
TEST_P(SavedTabGroupBarUnitTest, PinTabGroupAddButton) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V2";
  }

  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  saved_tab_group_model()->Add(kSavedTabGroup1);
  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  saved_tab_group_model()->TogglePinState(kSavedTabGroup1.saved_guid());
  EXPECT_EQ(2u, saved_tab_group_bar()->children().size());
  EXPECT_TRUE(!!views::AsViewClass<SavedTabGroupButton>(
      saved_tab_group_bar()->children()[0]));
}

// Verify unpin an existing tab group will remove a button.
TEST_P(SavedTabGroupBarUnitTest, UnpinTabGroupRemoveButton) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V2";
  }

  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  SavedTabGroup group1 = kSavedTabGroup1;
  group1.SetPinned(true);

  saved_tab_group_model()->Add(group1);
  EXPECT_EQ(2u, saved_tab_group_bar()->children().size());

  saved_tab_group_model()->TogglePinState(kSavedTabGroup1.saved_guid());
  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());
}

TEST_P(SavedTabGroupBarUnitTest, PinAndUnpinMultipleTabGroups) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V2";
  }

  const base::Uuid guid_1 = kSavedTabGroup1.saved_guid();
  const base::Uuid guid_2 = kSavedTabGroup2.saved_guid();
  const base::Uuid guid_3 = kSavedTabGroup3.saved_guid();

  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  saved_tab_group_model()->Add(kSavedTabGroup1);
  saved_tab_group_model()->Add(kSavedTabGroup2);
  saved_tab_group_model()->Add(kSavedTabGroup3);
  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  saved_tab_group_model()->TogglePinState(guid_1);
  EXPECT_EQ(2u, saved_tab_group_bar()->children().size());
  ASSERT_THAT(GetButtonGUIDs(), testing::ElementsAre(guid_1));

  saved_tab_group_model()->TogglePinState(guid_2);
  EXPECT_EQ(3u, saved_tab_group_bar()->children().size());
  ASSERT_THAT(GetButtonGUIDs(), testing::ElementsAre(guid_2, guid_1));

  saved_tab_group_model()->TogglePinState(guid_3);
  EXPECT_EQ(4u, saved_tab_group_bar()->children().size());
  ASSERT_THAT(GetButtonGUIDs(), testing::ElementsAre(guid_3, guid_2, guid_1));

  saved_tab_group_model()->TogglePinState(guid_1);
  EXPECT_EQ(3u, saved_tab_group_bar()->children().size());
  ASSERT_THAT(GetButtonGUIDs(), testing::ElementsAre(guid_3, guid_2));

  saved_tab_group_model()->TogglePinState(guid_2);
  EXPECT_EQ(2u, saved_tab_group_bar()->children().size());
  ASSERT_THAT(GetButtonGUIDs(), testing::ElementsAre(guid_3));

  saved_tab_group_model()->TogglePinState(guid_3);
  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());
}

TEST_P(SavedTabGroupBarUnitTest, OnlyShowEverthingButtonForV2) {
  if (IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  saved_tab_group_model()->Add(kSavedTabGroup1);

  EXPECT_EQ(2u, saved_tab_group_bar()->children().size());

  saved_tab_group_bar()->SetBounds(
      0, 2, saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(40), 2);

  // Saved tab group button is not visible because there's not enough space.
  EXPECT_FALSE(saved_tab_group_bar()->children()[0]->GetVisible());

  // Everything button is visible.
  EXPECT_TRUE(saved_tab_group_bar()->children()[1]->GetVisible());
}

INSTANTIATE_TEST_SUITE_P(SavedTabGroupBar,
                         SavedTabGroupBarUnitTest,
                         testing::Bool());

INSTANTIATE_TEST_SUITE_P(SavedTabGroupEverythingMenu,
                         STGEverythingMenuUnitTest,
                         testing::Bool());

}  // namespace tab_groups
