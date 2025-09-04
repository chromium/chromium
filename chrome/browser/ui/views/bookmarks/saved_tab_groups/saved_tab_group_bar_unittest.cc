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
#include "components/collaboration/public/features.h"
#include "components/data_sharing/public/features.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/internal/tab_group_sync_service_impl.h"
#include "components/saved_tab_groups/public/collaboration_finder.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/pref_names.h"
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

class SavedTabGroupBarUnitTest : public TestWithBrowserView {
 public:
  SavedTabGroupBarUnitTest() {
    // TODO (crbug.com/406068322) the Messaging Service currently interferes
    // with this test harness, it needs to be cleaned up.
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{data_sharing::features::kDataSharingFeature},
        {collaboration::features::kCollaborationMessaging});
  }

  SavedTabGroupBar* saved_tab_group_bar() { return saved_tab_group_bar_.get(); }
  TabGroupSyncService* service() {
    return tab_groups::TabGroupSyncServiceFactory::GetForProfile(
        browser()->profile());
  }

  int button_padding() { return button_padding_; }

  void SetUp() override {
    TestWithBrowserView::SetUp();
    browser()->profile()->GetPrefs()->SetBoolean(
        tab_groups::prefs::kAutoPinNewTabGroups, true);

    TabGroupSyncService* service =
        tab_groups::TabGroupSyncServiceFactory::GetForProfile(
            browser()->profile());
    service->SetIsInitializedForTesting(true);
    Wait();

    saved_tab_group_bar_ = std::make_unique<SavedTabGroupBar>(browser(), false);
    saved_tab_group_bar_->SetPageNavigator(nullptr);
  }

  void TearDown() override {
    saved_tab_group_bar_.reset();
    TestWithBrowserView::TearDown();
  }

  // The TabGroupSyncService posts all of its observations. This means that
  // this test must Wait for any command that happens to call an observer as the
  // result of an action.
  void Wait() {
    // Post a dummy task in the current thread and wait for its completion so
    // that any already posted tasks are completed.
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
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
    Wait();
    return local_id;
  }

  // Returns the sync id of the group that was added.
  base::Uuid EnforceGroupSaved(tab_groups::SavedTabGroup group) {
    Wait();
    const LocalTabGroupID local_id = group.local_group_id().value();
    return service()->GetGroup(local_id).value().saved_guid();
  }

  base::Uuid AddGroupFromSync() {
    SavedTabGroup group = test::CreateTestSavedTabGroup();
    service()->AddGroup(group);
    Wait();
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

  void Pin(const base::Uuid& sync_id, int position = 0) {
    service()->UpdateGroupPosition(sync_id, true, position);
    Wait();
  }

  void Unpin(const base::Uuid& sync_id) {
    service()->UpdateGroupPosition(sync_id, false, std::nullopt);
    Wait();
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

TEST_F(SavedTabGroupBarUnitTest, AddsButtonFromModelAdd) {
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

TEST_F(SavedTabGroupBarUnitTest, EverthingButtonAlwaysVisible) {
  // Verify the initial count of saved tab group buttons.
  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  const views::View* overflow_button = saved_tab_group_bar()->children()[0];
  // Everything button shows by default.
  saved_tab_group_bar()->SetBounds(
      0, 2, saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(400), 2);
  EXPECT_TRUE(overflow_button->GetVisible());

  // Add a tab group button; the Everything button is still there.
  const base::Uuid& sync_id =
      EnforceGroupSaved(SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
          CreateNewGroupInBrowser()));

  saved_tab_group_bar()->SetBounds(
      0, 2, saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(400), 2);
  EXPECT_TRUE(overflow_button->GetVisible());

  // Remove the last tab group button; the Everything button is still there.
  service()->RemoveGroup(sync_id);
  saved_tab_group_bar()->SetBounds(
      0, 2, saved_tab_group_bar()->CalculatePreferredWidthRestrictedBy(400), 2);
  EXPECT_TRUE(overflow_button->GetVisible());
}

TEST_F(SavedTabGroupBarUnitTest, BarsWithSameModelsHaveSameButtons) {
  EnforceGroupSaved(SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
      CreateNewGroupInBrowser()));

  SavedTabGroupBar another_tab_group_bar_on_same_model(
      browser(),
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile()), false);

  EXPECT_EQ(saved_tab_group_bar()->children().size(),
            another_tab_group_bar_on_same_model.children().size());
}

TEST_F(SavedTabGroupBarUnitTest, RemoveButtonFromModelRemove) {
  const base::Uuid sync_id =
      EnforceGroupSaved(SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
          CreateNewGroupInBrowser()));

  EXPECT_EQ(2u, saved_tab_group_bar()->children().size());

  service()->RemoveGroup(sync_id);
  Wait();
  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());
}

TEST_F(SavedTabGroupBarUnitTest, UpdatedVisualDataMakesChangeToSpecificView) {
  auto pinned_local_id = LocalIDFromSyncID(AddGroupFromLocal());
  Wait();
  auto unpinned_sync_id = AddGroupFromLocal();
  Wait();

  Unpin(unpinned_sync_id);
  auto unpinned_local_id = LocalIDFromSyncID(unpinned_sync_id);

  tab_groups::TabGroupVisualData saved_tab_group_visual_data(kNewTitle,
                                                             kNewColor);

  // Update the visual_data and expect the first button to be updated and the
  // second button to stay the same.
  service()->UpdateVisualData(pinned_local_id, &saved_tab_group_visual_data);
  Wait();
  service()->UpdateVisualData(unpinned_local_id, &saved_tab_group_visual_data);
  Wait();

  SavedTabGroupButton* new_button_1 = views::AsViewClass<SavedTabGroupButton>(
      saved_tab_group_bar()->children()[0]);

  SavedTabGroupButton* new_button_2 = views::AsViewClass<SavedTabGroupButton>(
      saved_tab_group_bar()->children()[1]);

    ASSERT_TRUE(!!new_button_1);
    ASSERT_FALSE(!!new_button_2);

    EXPECT_EQ(new_button_1->GetText(), kNewTitle);
    EXPECT_EQ(new_button_1->tab_group_color_id(), kNewColor);
}

TEST_F(SavedTabGroupBarUnitTest, MoveButtonFromModelMove) {
  const base::Uuid sync_id_1 = AddGroupFromLocal();
  const base::Uuid sync_id_2 = AddGroupFromLocal();
  const base::Uuid sync_id_3 = AddGroupFromLocal();

  Wait();
  ASSERT_THAT(GetButtonGUIDs(),
              testing::ElementsAre(sync_id_3, sync_id_2, sync_id_1));

  Pin(sync_id_2, 2);
  EXPECT_THAT(GetButtonGUIDs(),
              testing::ElementsAre(sync_id_3, sync_id_1, sync_id_2));

  Pin(sync_id_2, 0);
  EXPECT_THAT(GetButtonGUIDs(),
              testing::ElementsAre(sync_id_2, sync_id_3, sync_id_1));

  Pin(sync_id_2, 1);
  EXPECT_THAT(GetButtonGUIDs(),
              testing::ElementsAre(sync_id_3, sync_id_2, sync_id_1));
}

// Verify add pinned tab group will add a button.
TEST_F(SavedTabGroupBarUnitTest, AddPinnedTabGroupButton) {
  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  EnforceGroupSaved(SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
      CreateNewGroupInBrowser()));

  EXPECT_EQ(2u, saved_tab_group_bar()->children().size());
  EXPECT_TRUE(!!views::AsViewClass<SavedTabGroupButton>(
      saved_tab_group_bar()->children()[0]));
}

// Verify pin an existing tab group will add a button.
TEST_F(SavedTabGroupBarUnitTest, PinTabGroupAddButton) {
  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  auto sync_id = AddGroupFromSync();
  Unpin(sync_id);

  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  service()->UpdateGroupPosition(sync_id, true, std::nullopt);
  Wait();

  EXPECT_EQ(2u, saved_tab_group_bar()->children().size());
  EXPECT_TRUE(!!views::AsViewClass<SavedTabGroupButton>(
      saved_tab_group_bar()->children()[0]));
}

TEST_F(SavedTabGroupBarUnitTest, AccessibleName) {
  tab_groups::TabGroupId tab_group_id = CreateNewGroupInBrowser();
  EnforceGroupSaved(
      SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(tab_group_id));
  Wait();

  SavedTabGroupButton* saved_tab_group_button =
      views::AsViewClass<SavedTabGroupButton>(
          saved_tab_group_bar()->children()[0]);
  saved_tab_group_button->SetText(u"");

  ui::AXNodeData data;
  saved_tab_group_button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_GROUP_AX_LABEL_UNNAMED_SAVED_GROUP_FORMAT, u"",
                l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_OPENED)),
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));

  saved_tab_group_button->SetText(u"Accessible Name");
  data = ui::AXNodeData();
  saved_tab_group_button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_GROUP_AX_LABEL_NAMED_SAVED_GROUP_FORMAT, u"", u"Accessible Name",
          l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_OPENED)),
      data.GetString16Attribute(ax::mojom::StringAttribute::kName));

  SavedTabGroup saved_tab_group = *service()->GetGroup(tab_group_id);
  saved_tab_group.SetCollaborationId(
      syncer::CollaborationId("collaboration_id"));
  saved_tab_group_button->UpdateButtonData(saved_tab_group);
  data = ui::AXNodeData();
  saved_tab_group_button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_GROUP_AX_LABEL_UNNAMED_SAVED_GROUP_FORMAT,
                l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_SHARED),
                l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_OPENED)),
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

TEST_F(SavedTabGroupBarUnitTest, TooltipText) {
  AddGroupFromLocal();
  SavedTabGroupButton* saved_tab_group_button =
      views::AsViewClass<SavedTabGroupButton>(
          saved_tab_group_bar()->children()[0]);
  saved_tab_group_button->SetText(u"");

  ui::AXNodeData data;
  saved_tab_group_button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_GROUP_AX_LABEL_UNNAMED_SAVED_GROUP_FORMAT, u"",
                l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_OPENED)),
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ(saved_tab_group_button->GetRenderedTooltipText(gfx::Point()),
            l10n_util::GetStringFUTF16(
                IDS_GROUP_AX_LABEL_UNNAMED_SAVED_GROUP_FORMAT, u"",
                l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_OPENED)));
  EXPECT_NE(data.GetString16Attribute(ax::mojom::StringAttribute::kDescription),
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));

  saved_tab_group_button->SetText(u"Accessible Name");
  data = ui::AXNodeData();
  saved_tab_group_button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_GROUP_AX_LABEL_NAMED_SAVED_GROUP_FORMAT, u"", u"Accessible Name",
          l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_OPENED)),
      data.GetString16Attribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ(
      saved_tab_group_button->GetRenderedTooltipText(gfx::Point()),
      l10n_util::GetStringFUTF16(
          IDS_GROUP_AX_LABEL_NAMED_SAVED_GROUP_FORMAT, u"", u"Accessible Name",
          l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_OPENED)));
  EXPECT_NE(data.GetString16Attribute(ax::mojom::StringAttribute::kDescription),
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

// Verify unpin an existing tab group will remove a button.
TEST_F(SavedTabGroupBarUnitTest, UnpinTabGroupRemoveButton) {
  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  const base::Uuid& sync_id =
      EnforceGroupSaved(SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
          CreateNewGroupInBrowser()));

  EXPECT_EQ(2u, saved_tab_group_bar()->children().size());

  service()->UpdateGroupPosition(sync_id, false, std::nullopt);
  Wait();

  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());
}
TEST_F(SavedTabGroupBarUnitTest, PinAndUnpinMultipleTabGroups) {
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
  Wait();
  EXPECT_EQ(2u, saved_tab_group_bar()->children().size());
  ASSERT_THAT(GetButtonGUIDs(), testing::ElementsAre(sync_id_1));

  service()->UpdateGroupPosition(sync_id_2, true, std::nullopt);
  Wait();
  EXPECT_EQ(3u, saved_tab_group_bar()->children().size());
  ASSERT_THAT(GetButtonGUIDs(), testing::ElementsAre(sync_id_2, sync_id_1));

  service()->UpdateGroupPosition(sync_id_3, true, std::nullopt);
  Wait();
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
  Wait();
  EXPECT_EQ(3u, saved_tab_group_bar()->children().size());
  ASSERT_THAT(GetButtonGUIDs(), testing::ElementsAre(sync_id_3, sync_id_2));

  service()->UpdateGroupPosition(sync_id_2, false, std::nullopt);
  Wait();
  EXPECT_EQ(2u, saved_tab_group_bar()->children().size());
  ASSERT_THAT(GetButtonGUIDs(), testing::ElementsAre(sync_id_3));

  service()->UpdateGroupPosition(sync_id_3, false, std::nullopt);
  Wait();
  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());
}

TEST_F(SavedTabGroupBarUnitTest, OnlyShowEverthingButton) {
  ASSERT_EQ(1u, saved_tab_group_bar()->children().size());
  AddGroupFromLocal();
  ASSERT_EQ(2u, saved_tab_group_bar()->children().size());

  const int button_preferred_size = saved_tab_group_bar()
                                        ->GetSavedTabGroupButtons()
                                        .at(0)
                                        ->GetPreferredSize()
                                        .width() +
                                    SavedTabGroupBar::kBetweenElementSpacing;
  const int overflow_preferred_width =
      saved_tab_group_bar()->overflow_button()->GetPreferredSize().width() +
      SavedTabGroupBar::kBetweenElementSpacing;

  // Not enough width for even the overflow button
  saved_tab_group_bar()->SetBounds(0, 0, overflow_preferred_width - 1, 100);
  EXPECT_FALSE(saved_tab_group_bar()->IsOverflowButtonVisible());
  EXPECT_EQ(0, saved_tab_group_bar()->GetNumberOfVisibleGroups());

  // Just enough width for overflow button only
  saved_tab_group_bar()->SetBounds(0, 0, overflow_preferred_width + 1, 100);
  EXPECT_TRUE(saved_tab_group_bar()->IsOverflowButtonVisible());
  EXPECT_EQ(0, saved_tab_group_bar()->GetNumberOfVisibleGroups());

  // Enough width to show one saved group button and the overflow button
  saved_tab_group_bar()->SetBounds(
      0, 0, button_preferred_size + overflow_preferred_width, 100);
  EXPECT_TRUE(saved_tab_group_bar()->IsOverflowButtonVisible());
  EXPECT_EQ(1, saved_tab_group_bar()->GetNumberOfVisibleGroups());
}

TEST_F(SavedTabGroupBarUnitTest, AccessibleProperties) {
  ui::AXNodeData data;

  saved_tab_group_bar()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::Role::kToolbar, data.role);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ACCNAME_SAVED_TAB_GROUPS),
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

TEST_F(SavedTabGroupBarUnitTest, GroupWithNoTabsDoesntShow) {
  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());

  SavedTabGroup empty_pinned_group(u"Test Title",
                                   tab_groups::TabGroupColorId::kBlue, {});
  // position must be set or the update time will be overridden during model
  // save.
  empty_pinned_group.SetPosition(0);

  service()->AddGroup(std::move(empty_pinned_group));

  EXPECT_EQ(1u, saved_tab_group_bar()->children().size());
}

TEST_F(SavedTabGroupBarUnitTest, GroupLoadFromModelInOrder) {
  base::Uuid uuid1 = AddGroupFromLocal();
  base::Uuid uuid2 = AddGroupFromLocal();
  base::Uuid uuid3 = AddGroupFromLocal();

  auto saved_tab_group_bar =
      std::make_unique<SavedTabGroupBar>(browser(), false);
  auto children = saved_tab_group_bar->children();

  // Verify groups are shown in reverse order(last added groups show first).
  EXPECT_EQ(4u, children.size());
  EXPECT_EQ(uuid3,
            views::AsViewClass<SavedTabGroupButton>(children[0])->guid());
  EXPECT_EQ(uuid2,
            views::AsViewClass<SavedTabGroupButton>(children[1])->guid());
  EXPECT_EQ(uuid1,
            views::AsViewClass<SavedTabGroupButton>(children[2])->guid());
}

}  // namespace tab_groups
