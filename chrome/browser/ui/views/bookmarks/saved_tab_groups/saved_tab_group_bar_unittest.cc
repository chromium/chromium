// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_bar.h"

#include <memory>
#include <optional>
#include <string>

#include "base/containers/contains.h"
#include "base/task/current_thread.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_button.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_overflow_button.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/public/collaboration_finder.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_utils.h"

namespace tab_groups {
namespace {

using testing::ElementsAre;
using testing::IsEmpty;
using testing::SizeIs;

const std::u16string kNewTitle(u"kNewTitle");

const tab_groups::TabGroupColorId kNewColor = tab_groups::TabGroupColorId::kRed;
}  // anonymous namespace

class SavedTabGroupBarUnitTest : public TestWithBrowserView,
                                 public ::testing::WithParamInterface<bool> {
 public:
  SavedTabGroupBarUnitTest() {
    if (IsV2UIEnabled()) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{tab_groups::kTabGroupsSaveUIUpdate,
                                data_sharing::features::kDataSharingFeature},
          {});
    } else {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{data_sharing::features::kDataSharingFeature},
          {tab_groups::kTabGroupsSaveUIUpdate});
    }
  }

  bool IsV2UIEnabled() const { return GetParam(); }
  SavedTabGroupBar* saved_tab_group_bar() { return saved_tab_group_bar_.get(); }
  TabGroupSyncService* service() {
    return tab_groups::SavedTabGroupUtils::GetServiceForProfile(
        browser()->profile());
  }

  int button_padding() { return button_padding_; }

  void SetUp() override {
    TestWithBrowserView::SetUp();

    TabGroupSyncService* service =
        tab_groups::SavedTabGroupUtils::GetServiceForProfile(
            browser()->profile());
    service->SetIsInitializedForTesting(true);

    saved_tab_group_bar_ = std::make_unique<SavedTabGroupBar>(browser(), false);
    saved_tab_group_bar_->SetPageNavigator(nullptr);
  }

  void TearDown() override {
    saved_tab_group_bar_.reset();
    TestWithBrowserView::TearDown();
  }

  void AddTabToBrowser(Browser* browser, int index) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(browser->profile(),
                                                          nullptr);

    browser->tab_strip_model()->AddWebContents(
        std::move(web_contents), index,
        ui::PageTransition::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);
  }

  tab_groups::TabGroupId LocalIDFromSyncID(const base::Uuid& sync_id) {
    return service()->GetGroup(sync_id)->local_group_id().value();
  }

  tab_groups::TabGroupId CreateNewGroupInBrowser() {
    AddTabToBrowser(browser(), 0);
    tab_groups::TabGroupId local_id =
        browser()->tab_strip_model()->AddToNewGroup({0});
    return local_id;
  }

  // Returns the sync id of the group that was added.
  base::Uuid EnforceGroupSaved(tab_groups::SavedTabGroup group) {
    const LocalTabGroupID local_id = group.local_group_id().value();

    if (!tab_groups::IsTabGroupsSaveV2Enabled()) {
      // the group must be manually saved.
      service()->SaveGroup(group);
    }

    return service()->GetGroup(local_id).value().saved_guid();
  }

  base::Uuid AddGroupFromSync() {
    SavedTabGroup group = test::CreateTestSavedTabGroup();
    service()->AddGroup(group);
    return group.saved_guid();
  }

  base::Uuid AddGroupFromLocal() {
    return EnforceGroupSaved(SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
        CreateNewGroupInBrowser()));
  }

  void Add4GroupsFromLocal() {
    AddGroupFromLocal();
    AddGroupFromLocal();
    AddGroupFromLocal();
    AddGroupFromLocal();
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

  void Pin(const base::Uuid& sync_id) {
    service()->UpdateGroupPosition(sync_id, true, 0);
  }

  void Unpin(const base::Uuid& sync_id) {
    service()->UpdateGroupPosition(sync_id, false, std::nullopt);
  }

  void UpdateTitle(const SavedTabGroup& group, const std::u16string& title) {
    tab_groups::TabGroupVisualData new_visual_data{title, group.color()};
    service()->UpdateVisualData(group.local_group_id().value(),
                                &new_visual_data);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<SavedTabGroupBar> saved_tab_group_bar_;

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
  const std::u16string title_1 = u"test_title_1";
  const std::u16string title_2 = u"test_title_2";
  const std::u16string title_3 = u"test_title_3";

  SavedTabGroup group_1 =
      tab_groups::SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
          CreateNewGroupInBrowser());
  SavedTabGroup group_2 =
      tab_groups::SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
          CreateNewGroupInBrowser());
  SavedTabGroup group_3 =
      tab_groups::SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
          CreateNewGroupInBrowser());

  EnforceGroupSaved(group_1);
  EnforceGroupSaved(group_2);
  EnforceGroupSaved(group_3);

  // update the titles
  UpdateTitle(group_1, title_1);
  UpdateTitle(group_2, title_2);
  UpdateTitle(group_3, title_3);

  // A separator is also added.
  auto model = menu_model();
  EXPECT_EQ(model->GetItemCount(), 5u);

  // The three added tab group items starts from model index 2. They are sorted
  // by most recent created first.
  EXPECT_EQ(model->GetLabelAt(2), title_3);
  EXPECT_EQ(model->GetLabelAt(3), title_2);
  EXPECT_EQ(model->GetLabelAt(4), title_1);
}

TEST_P(SavedTabGroupBarUnitTest, AddsButtonFromModelAdd) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  // There's always an overflow button in the saved tab group bar.
  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  {  // Add a group and expect it to show up in the bar by default.
    AddGroupFromLocal();
    EXPECT_EQ(2u, saved_tab_group_bar()->children().size());
  }

  {  // Create a model change that comes from sync as unpinned..
    auto sync_id = AddGroupFromSync();
    Unpin(sync_id);

    EXPECT_EQ(2u, saved_tab_group_bar()->children().size());
  }
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
    const base::Uuid& sync_id =
        EnforceGroupSaved(SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
            CreateNewGroupInBrowser()));

    saved_tab_group_bar()->SetBounds(
        0, 2, saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(400),
        2);
    EXPECT_TRUE(overflow_button->GetVisible());

    // Remove the last tab group button; the Everything button is still there.
    service()->RemoveGroup(sync_id);
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
  Add4GroupsFromLocal();
  EXPECT_FALSE(overflow_button->GetVisible());
  EXPECT_EQ(5u, saved_tab_group_bar()->children().size());

  // Verify that the overflow button is visible when a 5th button is added and
  // that the 5th button is not visible.
  EnforceGroupSaved(SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
      CreateNewGroupInBrowser()));

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
  Add4GroupsFromLocal();

  const base::Uuid& sync_id =
      EnforceGroupSaved(SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
          CreateNewGroupInBrowser()));

  // Layout the buttons.
  saved_tab_group_bar()->SetBounds(
      0, 2, saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(400), 2);

  EXPECT_TRUE(overflow_button->GetVisible());
  EXPECT_FALSE(saved_tab_group_bar()->children()[4]->GetVisible());
  EXPECT_EQ(6u, saved_tab_group_bar()->children().size());

  service()->RemoveGroup(sync_id);

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
  Add4GroupsFromLocal();

  const base::Uuid& sync_id =
      EnforceGroupSaved(SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
          CreateNewGroupInBrowser()));

  // Layout the buttons.
  saved_tab_group_bar()->SetBounds(
      0, 2, saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(400), 2);

  EXPECT_TRUE(overflow_button->GetVisible());
  EXPECT_FALSE(saved_tab_group_bar()->children()[4]->GetVisible());
  EXPECT_EQ(6u, saved_tab_group_bar()->children().size());

  service()->RemoveGroup(sync_id);

  // Layout the buttons.
  saved_tab_group_bar()->SetBounds(
      0, 2, saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(400), 2);

  EXPECT_FALSE(overflow_button->GetVisible());
  EXPECT_TRUE(saved_tab_group_bar()->children()[3]->GetVisible());
  EXPECT_EQ(5u, saved_tab_group_bar()->children().size());
}

TEST_P(SavedTabGroupBarUnitTest, BarsWithSameModelsHaveSameButtons) {
  EnforceGroupSaved(SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
      CreateNewGroupInBrowser()));

  SavedTabGroupBar another_tab_group_bar_on_same_model(
      browser(),
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(profile()), false);

  EXPECT_EQ(saved_tab_group_bar()->children().size(),
            another_tab_group_bar_on_same_model.children().size());
}

TEST_P(SavedTabGroupBarUnitTest, RemoveButtonFromModelRemove) {
  AddTabToBrowser(browser(), TabStripModel::kNoTab);
  {  // Remove the group and expect no buttons except the overflow menu.
    const base::Uuid sync_id =
        EnforceGroupSaved(SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
            CreateNewGroupInBrowser()));

    EXPECT_EQ(2u, saved_tab_group_bar()->children().size());

    service()->RemoveGroup(sync_id);
    EXPECT_EQ(1u, saved_tab_group_bar()->children().size());
  }

  {  // Remove the group and expect no buttons except the overflow menu.
    const base::Uuid sync_id =
        EnforceGroupSaved(SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
            CreateNewGroupInBrowser()));

    EXPECT_EQ(2u, saved_tab_group_bar()->children().size());

    service()->RemoveGroup(sync_id);
    EXPECT_EQ(1u, saved_tab_group_bar()->children().size());
  }

  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());
}

TEST_P(SavedTabGroupBarUnitTest, UpdatedVisualDataMakesChangeToSpecificView) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  auto pinned_local_id = LocalIDFromSyncID(AddGroupFromLocal());

  auto unpinned_sync_id = AddGroupFromLocal();
  Unpin(unpinned_sync_id);
  auto unpinned_local_id = LocalIDFromSyncID(unpinned_sync_id);

  tab_groups::TabGroupVisualData saved_tab_group_visual_data(kNewTitle,
                                                             kNewColor);

  // Update the visual_data and expect the first button to be updated and the
  // second button to stay the same.
  service()->UpdateVisualData(pinned_local_id, &saved_tab_group_visual_data);
  service()->UpdateVisualData(unpinned_local_id, &saved_tab_group_visual_data);

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
  const base::Uuid sync_id_1 = AddGroupFromLocal();
  const base::Uuid sync_id_2 = AddGroupFromLocal();
  const base::Uuid sync_id_3 = AddGroupFromLocal();

  if (IsV2UIEnabled()) {
    ASSERT_THAT(GetButtonGUIDs(),
                testing::ElementsAre(sync_id_3, sync_id_2, sync_id_1));
    service()->UpdateGroupPosition(sync_id_2, std::nullopt, 2);
    EXPECT_THAT(GetButtonGUIDs(),
                testing::ElementsAre(sync_id_3, sync_id_1, sync_id_2));
    service()->UpdateGroupPosition(sync_id_2, std::nullopt, 0);
    EXPECT_THAT(GetButtonGUIDs(),
                testing::ElementsAre(sync_id_2, sync_id_3, sync_id_1));
    service()->UpdateGroupPosition(sync_id_2, std::nullopt, 1);
    EXPECT_THAT(GetButtonGUIDs(),
                testing::ElementsAre(sync_id_3, sync_id_2, sync_id_1));
  } else {
    ASSERT_THAT(GetButtonGUIDs(),
                testing::ElementsAre(sync_id_1, sync_id_2, sync_id_3));
    service()->UpdateGroupPosition(sync_id_2, std::nullopt, 2);
    EXPECT_THAT(GetButtonGUIDs(),
                testing::ElementsAre(sync_id_1, sync_id_3, sync_id_2));
    service()->UpdateGroupPosition(sync_id_2, std::nullopt, 0);
    EXPECT_THAT(GetButtonGUIDs(),
                testing::ElementsAre(sync_id_2, sync_id_1, sync_id_3));
    service()->UpdateGroupPosition(sync_id_2, std::nullopt, 1);
    EXPECT_THAT(GetButtonGUIDs(),
                testing::ElementsAre(sync_id_1, sync_id_2, sync_id_3));
  }
}

// If the restriction is exactly the expected size all should be visible
TEST_P(SavedTabGroupBarUnitTest, CalculatePreferredWidthRestrictedByExactSize) {
  if (IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  Add4GroupsFromLocal();

  int exact_width = GetWidthOfButtonsAndPadding();
  int calculated_width =
      saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(exact_width);
  EXPECT_EQ(exact_width, calculated_width);

  // After 4 buttons have been added (excluding the invisible overflow), all
  // subsequent buttons will be hidden. Instead an overflow menu will appear
  // which will house the hidden buttons.
  EnforceGroupSaved(SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
      CreateNewGroupInBrowser()));

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
    GTEST_SKIP() << "N/A for V1";
  }

  Add4GroupsFromLocal();
  int exact_width = GetWidthOfButtonsAndPadding();

  EXPECT_EQ(exact_width,
            saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(
                exact_width + 1));

  // After 4 buttons have been added (excluding the invisible overflow), all
  // subsequent buttons will be hidden. Instead an overflow menu will appear
  // which will house the hidden buttons.
  EnforceGroupSaved(SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
      CreateNewGroupInBrowser()));

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
    GTEST_SKIP() << "N/A for V1";
  }

  Add4GroupsFromLocal();
  int exact_width = GetWidthOfButtonsAndPadding();

  EXPECT_GT(exact_width,
            saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(
                exact_width - 1));

  // After 4 buttons have been added (excluding the invisible overflow), all
  // subsequent buttons will be hidden. Instead an overflow menu will appear
  // which will house the hidden buttons.
  EnforceGroupSaved(SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
      CreateNewGroupInBrowser()));

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
    GTEST_SKIP() << "N/A for V1";
  }

  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  EnforceGroupSaved(SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
      CreateNewGroupInBrowser()));

  EXPECT_EQ(2u, saved_tab_group_bar()->children().size());
  EXPECT_TRUE(!!views::AsViewClass<SavedTabGroupButton>(
      saved_tab_group_bar()->children()[0]));
}

// Verify pin an existing tab group will add a button.
TEST_P(SavedTabGroupBarUnitTest, PinTabGroupAddButton) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  auto sync_id = AddGroupFromSync();
  Unpin(sync_id);

  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  service()->UpdateGroupPosition(sync_id, true, std::nullopt);
  EXPECT_EQ(2u, saved_tab_group_bar()->children().size());
  EXPECT_TRUE(!!views::AsViewClass<SavedTabGroupButton>(
      saved_tab_group_bar()->children()[0]));
}

TEST_P(SavedTabGroupBarUnitTest, AccessibleName) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }
  EnforceGroupSaved(SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
      CreateNewGroupInBrowser()));
  SavedTabGroupButton* saved_tab_group_button =
      views::AsViewClass<SavedTabGroupButton>(
          saved_tab_group_bar()->children()[0]);
  saved_tab_group_button->SetText(u"");

  ui::AXNodeData data;
  saved_tab_group_button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_GROUP_AX_LABEL_UNNAMED_SAVED_GROUP_FORMAT,
                l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_OPENED)),
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));

  saved_tab_group_button->SetText(u"Accessible Name");
  data = ui::AXNodeData();
  saved_tab_group_button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_GROUP_AX_LABEL_NAMED_SAVED_GROUP_FORMAT, u"Accessible Name",
                l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_OPENED)),
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

TEST_P(SavedTabGroupBarUnitTest, TooltipText) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }
  AddGroupFromLocal();
  SavedTabGroupButton* saved_tab_group_button =
      views::AsViewClass<SavedTabGroupButton>(
          saved_tab_group_bar()->children()[0]);
  saved_tab_group_button->SetText(u"");

  ui::AXNodeData data;
  saved_tab_group_button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_GROUP_AX_LABEL_UNNAMED_SAVED_GROUP_FORMAT,
                l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_OPENED)),
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ(saved_tab_group_button->GetTooltipText(gfx::Point()),
            l10n_util::GetStringFUTF16(
                IDS_GROUP_AX_LABEL_UNNAMED_SAVED_GROUP_FORMAT,
                l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_OPENED)));
  EXPECT_NE(data.GetString16Attribute(ax::mojom::StringAttribute::kDescription),
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));

  saved_tab_group_button->SetText(u"Accessible Name");
  data = ui::AXNodeData();
  saved_tab_group_button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_GROUP_AX_LABEL_NAMED_SAVED_GROUP_FORMAT, u"Accessible Name",
                l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_OPENED)),
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ(saved_tab_group_button->GetTooltipText(gfx::Point()),
            l10n_util::GetStringFUTF16(
                IDS_GROUP_AX_LABEL_NAMED_SAVED_GROUP_FORMAT, u"Accessible Name",
                l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_OPENED)));
  EXPECT_NE(data.GetString16Attribute(ax::mojom::StringAttribute::kDescription),
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

// Verify unpin an existing tab group will remove a button.
TEST_P(SavedTabGroupBarUnitTest, UnpinTabGroupRemoveButton) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  const base::Uuid& sync_id =
      EnforceGroupSaved(SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
          CreateNewGroupInBrowser()));

  EXPECT_EQ(2u, saved_tab_group_bar()->children().size());

  service()->UpdateGroupPosition(sync_id, false, std::nullopt);
  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());
}

TEST_P(SavedTabGroupBarUnitTest, PinAndUnpinMultipleTabGroups) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  base::Uuid sync_id_1 = AddGroupFromLocal();
  base::Uuid sync_id_2 = AddGroupFromLocal();
  base::Uuid sync_id_3 = AddGroupFromLocal();

  // start groups as unpinned.
  Unpin(sync_id_1);
  Unpin(sync_id_2);
  Unpin(sync_id_3);

  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  service()->UpdateGroupPosition(sync_id_1, true, std::nullopt);
  EXPECT_EQ(2u, saved_tab_group_bar()->children().size());
  ASSERT_THAT(GetButtonGUIDs(), testing::ElementsAre(sync_id_1));

  service()->UpdateGroupPosition(sync_id_2, true, std::nullopt);
  EXPECT_EQ(3u, saved_tab_group_bar()->children().size());
  ASSERT_THAT(GetButtonGUIDs(), testing::ElementsAre(sync_id_2, sync_id_1));

  service()->UpdateGroupPosition(sync_id_3, true, std::nullopt);
  EXPECT_EQ(4u, saved_tab_group_bar()->children().size());
  ASSERT_THAT(GetButtonGUIDs(),
              testing::ElementsAre(sync_id_3, sync_id_2, sync_id_1));

  std::optional<SavedTabGroup> retrieved_group_1 =
      service()->GetGroup(sync_id_1);
  std::optional<SavedTabGroup> retrieved_group_2 =
      service()->GetGroup(sync_id_2);
  std::optional<SavedTabGroup> retrieved_group_3 =
      service()->GetGroup(sync_id_3);

  service()->UpdateGroupPosition(sync_id_1, false, std::nullopt);
  EXPECT_EQ(3u, saved_tab_group_bar()->children().size());
  ASSERT_THAT(GetButtonGUIDs(), testing::ElementsAre(sync_id_3, sync_id_2));

  service()->UpdateGroupPosition(sync_id_2, false, std::nullopt);
  EXPECT_EQ(2u, saved_tab_group_bar()->children().size());
  ASSERT_THAT(GetButtonGUIDs(), testing::ElementsAre(sync_id_3));

  service()->UpdateGroupPosition(sync_id_3, false, std::nullopt);
  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());
}

TEST_P(SavedTabGroupBarUnitTest, OnlyShowEverthingButtonForV2) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  AddGroupFromLocal();

  saved_tab_group_bar()->SetBounds(
      0, 2, saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(2), 2);

  // Saved tab group button is not visible because there's not enough space.
  // Everything button is visible.
  EXPECT_FALSE(saved_tab_group_bar()->children()[1]->GetVisible());

  // Saved tab group button is not visible because there's not enough space.
  // Everything button is visible.
  EXPECT_TRUE(saved_tab_group_bar()->children()[0]->GetVisible());
}

TEST_P(SavedTabGroupBarUnitTest, AccessibleProperties) {
  ui::AXNodeData data;

  saved_tab_group_bar()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::Role::kToolbar, data.role);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ACCNAME_SAVED_TAB_GROUPS),
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

TEST_P(SavedTabGroupBarUnitTest, GroupWithNoTabsDoesntShow) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  SavedTabGroup empty_pinned_group(u"Test Title",
                                   tab_groups::TabGroupColorId::kBlue, {});
  // position must be set or the update time will be overridden during model
  // save.
  empty_pinned_group.SetPosition(0);

  service()->AddGroup(std::move(empty_pinned_group));

  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());
}

INSTANTIATE_TEST_SUITE_P(SavedTabGroupBar,
                         SavedTabGroupBarUnitTest,
                         testing::Bool());

INSTANTIATE_TEST_SUITE_P(SavedTabGroupEverythingMenu,
                         STGEverythingMenuUnitTest,
                         testing::Bool());

}  // namespace tab_groups
