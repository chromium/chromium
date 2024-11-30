// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_metrics.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_bubble_controller.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/internal/tab_group_sync_service_impl.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/interactive_test_internal.h"

namespace tab_groups {
constexpr char kSkipPixelTestsReason[] = "Should only run in pixel_tests.";
constexpr char kRecallHistogram[] = "TabGroups.Shared.Recall.Desktop";
constexpr char kManageHistogram[] = "TabGroups.Shared.Manage.Desktop";

class SharedTabGroupInteractiveUiTest : public InteractiveBrowserTest {
 public:
  SharedTabGroupInteractiveUiTest() = default;
  ~SharedTabGroupInteractiveUiTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {tab_groups::kTabGroupsSaveUIUpdate, tab_groups::kTabGroupsSaveV2,
         tab_groups::kTabGroupSyncServiceDesktopMigration,
         data_sharing::features::kDataSharingFeature},
        {});
    InProcessBrowserTest::SetUp();
  }

  MultiStep FinishTabstripAnimations() {
    return Steps(
        WaitForShow(kTabStripElementId),
        std::move(WithView(kTabStripElementId, [](TabStrip* tab_strip) {
                    tab_strip->StopAnimating(true);
                  }).SetDescription("FinishTabstripAnimation")));
  }

  MultiStep ShowBookmarksBar() {
    return Steps(PressButton(kToolbarAppMenuButtonElementId),
                 SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
                 SelectMenuItem(BookmarkSubMenuModel::kShowBookmarkBarMenuItem),
                 WaitForShow(kBookmarkBarElementId));
  }

  MultiStep HoverTabAt(int index) {
    const char kTabToHover[] = "Tab to hover";
    return Steps(NameDescendantViewByType<Tab>(kBrowserViewElementId,
                                               kTabToHover, index),
                 MoveMouseTo(kTabToHover));
  }

  MultiStep HoverTabGroupHeader(TabGroupId group_id) {
    const char kTabGroupHeaderToHover[] = "Tab group header to hover";
    return Steps(FinishTabstripAnimations(),
                 NameTabGroupHeaderView(group_id, kTabGroupHeaderToHover),
                 MoveMouseTo(kTabGroupHeaderToHover));
  }

  MultiStep NameTabGroupHeaderView(TabGroupId group_id, std::string name) {
    return Steps(NameDescendantView(
        kBrowserViewElementId, name,
        base::BindRepeating(
            [](tab_groups::TabGroupId group_id, const views::View* view) {
              const TabGroupHeader* header =
                  views::AsViewClass<TabGroupHeader>(view);
              if (!header) {
                return false;
              }
              return header->group().value() == group_id;
            },
            group_id)));
  }

  TabGroupId CreateNewTabGroup() {
    EXPECT_TRUE(
        AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
    return browser()->tab_strip_model()->AddToNewGroup({0});
  }

  void ShareTabGroup(TabGroupId group_id, std::string collaboration_id) {
    TabGroupSyncServiceImpl* service = static_cast<TabGroupSyncServiceImpl*>(
        TabGroupSyncServiceFactory::GetForProfile(browser()->profile()));
    service->MakeTabGroupSharedForTesting(group_id, collaboration_id);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Take a screenshot of the shared tab group in app menu > tab groups.
IN_PROC_BROWSER_TEST_F(SharedTabGroupInteractiveUiTest,
                       SharedTabGroupInAppMenu) {
  TabGroupId group_id = CreateNewTabGroup();
  ShareTabGroup(group_id, "fake_collaboration_id");

  RunTestSequence(WaitForShow(kTabGroupHeaderElementId),
                  FinishTabstripAnimations(), ShowBookmarksBar(),
                  PressButton(kToolbarAppMenuButtonElementId),
                  WaitForShow(AppMenuModel::kTabGroupsMenuItem),
                  SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),
                  EnsurePresent(tab_groups::STGEverythingMenu::kTabGroup),
                  SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                                          kSkipPixelTestsReason),
                  Screenshot(STGEverythingMenu::kTabGroup,
                             "shared_icon_in_app_menu", "5924633"),
                  // Close the app menu to prevent flakes on mac.
                  HoverTabAt(0), ClickMouse(),
                  WaitForHide(AppMenuModel::kTabGroupsMenuItem));
}

// Take a screenshot of the shared tab group in the everything menu.
IN_PROC_BROWSER_TEST_F(SharedTabGroupInteractiveUiTest,
                       SharedTabGroupInEverythingMenu) {
  TabGroupId group_id = CreateNewTabGroup();
  ShareTabGroup(group_id, "fake_collaboration_id");

  RunTestSequence(
      WaitForShow(kTabGroupHeaderElementId), FinishTabstripAnimations(),
      ShowBookmarksBar(), PressButton(kSavedTabGroupOverflowButtonElementId),
      EnsurePresent(STGEverythingMenu::kTabGroup),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kSkipPixelTestsReason),
      Screenshot(STGEverythingMenu::kTabGroup, "shared_icon_in_everything_menu",
                 "5924633"),
      // Close the everything menu to prevent flakes on mac.
      HoverTabAt(0), ClickMouse(), WaitForHide(STGEverythingMenu::kTabGroup));
}

// Take a screenshot of the shared tab group in the TabStrip.
IN_PROC_BROWSER_TEST_F(SharedTabGroupInteractiveUiTest,
                       SharedTabGroupInTabStrip) {
  const char kTabGroupHeaderToScreenshot[] = "Tab group header to hover";

  TabGroupId group_id = CreateNewTabGroup();
  ShareTabGroup(group_id, "fake_collaboration_id");

  // TODO(crbug.com/380088920): Manually trigger a layout until we have a way to
  // know when the entity tracker is initialized.
  TabGroup* tab_group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(group_id);
  tab_group->SetVisualData(*tab_group->visual_data());

  RunTestSequence(WaitForShow(kTabGroupHeaderElementId),
                  FinishTabstripAnimations(),
                  NameTabGroupHeaderView(group_id, kTabGroupHeaderToScreenshot),
                  SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                                          kSkipPixelTestsReason),
                  Screenshot(kTabGroupHeaderToScreenshot,
                             "shared_icon_in_tab_group_header", "5924633"));
}

// Verify the closed metric is recorded when a shared group is closed from the
// TabGroupEditorBubble.
IN_PROC_BROWSER_TEST_F(SharedTabGroupInteractiveUiTest,
                       RecordMetricOnSharedGroupClosing) {
  base::HistogramTester histogram_tester;

  TabGroupId group_id = CreateNewTabGroup();
  ShareTabGroup(group_id, "fake_collaboration_id");

  RunTestSequence(WaitForShow(kTabGroupHeaderElementId),
                  FinishTabstripAnimations(), HoverTabGroupHeader(group_id),
                  ClickMouse(ui_controls::RIGHT),
                  WaitForShow(kTabGroupEditorBubbleId),
                  PressButton(kTabGroupEditorBubbleCloseGroupButtonId),
                  WaitForHide(kTabGroupEditorBubbleCloseGroupButtonId),
                  FinishTabstripAnimations());

  histogram_tester.ExpectUniqueSample(
      kRecallHistogram,
      saved_tab_groups::metrics::SharedTabGroupRecallTypeDesktop::kClosed, 1);
}

// Verify the OpenedFromBookmarksBar metric is recorded when a shared group is
// opened from the bookmarks bar.
IN_PROC_BROWSER_TEST_F(SharedTabGroupInteractiveUiTest,
                       RecordMetricOnSharedGroupOpeningFromBookmarksBar) {
  ::base::HistogramTester histogram_tester;

  TabGroupId group_id = CreateNewTabGroup();
  ShareTabGroup(group_id, "fake_collaboration_id");

  // Close the tab group.
  browser()->tab_strip_model()->CloseAllTabsInGroup(group_id);

  RunTestSequence(FinishTabstripAnimations(), ShowBookmarksBar(),
                  EnsurePresent(kSavedTabGroupButtonElementId),
                  PressButton(kSavedTabGroupButtonElementId),
                  FinishTabstripAnimations());

  histogram_tester.ExpectUniqueSample(
      kRecallHistogram,
      saved_tab_groups::metrics::SharedTabGroupRecallTypeDesktop::
          kOpenedFromBookmarksBar,
      1);
}

// Verify the OpenedFromEverythingMenu metric is recorded when a shared group is
// opened from the everything menu.
IN_PROC_BROWSER_TEST_F(SharedTabGroupInteractiveUiTest,
                       RecordMetricOnSharedGroupOpeningFromEverythingMenu) {
  ::base::HistogramTester histogram_tester;

  TabGroupId group_id = CreateNewTabGroup();
  ShareTabGroup(group_id, "fake_collaboration_id");

  // Close the tab group.
  browser()->tab_strip_model()->CloseAllTabsInGroup(group_id);

  RunTestSequence(
      FinishTabstripAnimations(), ShowBookmarksBar(),
      PressButton(kSavedTabGroupOverflowButtonElementId),
      SelectMenuItem(STGEverythingMenu::kTabGroup), FinishTabstripAnimations(),
      // Close the everything menu to prevent flakes on mac.
      HoverTabAt(0), ClickMouse(), WaitForHide(STGEverythingMenu::kTabGroup));

  histogram_tester.ExpectUniqueSample(
      kRecallHistogram,
      saved_tab_groups::metrics::SharedTabGroupRecallTypeDesktop::
          kOpenedFromEverythingMenu,
      1);
}

// Verify the OpenedFromSubmenu metric is recoreded when a shared group is
// opened from the app menu > tab groups sub menu.
IN_PROC_BROWSER_TEST_F(SharedTabGroupInteractiveUiTest,
                       RecordMetricOnSharedGroupOpeningFromAppMenu) {
  ::base::HistogramTester histogram_tester;

  TabGroupId group_id = CreateNewTabGroup();
  ShareTabGroup(group_id, "fake_collaboration_id");

  // Close the tab group.
  browser()->tab_strip_model()->CloseAllTabsInGroup(group_id);

  RunTestSequence(FinishTabstripAnimations(), ShowBookmarksBar(),
                  PressButton(kToolbarAppMenuButtonElementId),
                  WaitForShow(AppMenuModel::kTabGroupsMenuItem),
                  SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),
                  SelectMenuItem(STGEverythingMenu::kTabGroup),
                  SelectMenuItem(STGEverythingMenu::kOpenGroup),
                  FinishTabstripAnimations(),
                  // Close the app menu to prevent flakes on mac.
                  HoverTabAt(0), ClickMouse(),
                  WaitForHide(AppMenuModel::kTabGroupsMenuItem));

  histogram_tester.ExpectUniqueSample(
      kRecallHistogram,
      saved_tab_groups::metrics::SharedTabGroupRecallTypeDesktop::
          kOpenedFromSubmenu,
      1);
}

// Verify the ShareGroup metric is recorded when the "Share group" button is
// pressed in the tab group editor bubble.
IN_PROC_BROWSER_TEST_F(SharedTabGroupInteractiveUiTest,
                       RecordMetricWhenShareGroupPressed) {
  ::base::HistogramTester histogram_tester;

  TabGroupId group_id = CreateNewTabGroup();
  // ShareTabGroup(group_id, "fake_collaboration_id");

  RunTestSequence(
      WaitForShow(kTabGroupHeaderElementId), FinishTabstripAnimations(),
      HoverTabGroupHeader(group_id), ClickMouse(ui_controls::RIGHT),
      WaitForShow(kTabGroupEditorBubbleId),
      PressButton(kTabGroupEditorBubbleShareGroupButtonId),
      WaitForShow(kDataSharingBubbleElementId),
      // Close the dialog to prevent flakes on mac.
      HoverTabAt(0), ClickMouse(), WaitForHide(kDataSharingBubbleElementId),
      FinishTabstripAnimations());

  histogram_tester.ExpectUniqueSample(
      kManageHistogram,
      saved_tab_groups::metrics::SharedTabGroupManageTypeDesktop::kShareGroup,
      1);
}

// Verify the ManageGroup metric is recorded when the "Manage group" button is
// pressed in the tab group editor bubble.
IN_PROC_BROWSER_TEST_F(SharedTabGroupInteractiveUiTest,
                       RecordMetricWhenManagedGroupPressed) {
  ::base::HistogramTester histogram_tester;

  TabGroupId group_id = CreateNewTabGroup();
  ShareTabGroup(group_id, "fake_collaboration_id");

  RunTestSequence(
      WaitForShow(kTabGroupHeaderElementId), FinishTabstripAnimations(),
      HoverTabGroupHeader(group_id), ClickMouse(ui_controls::RIGHT),
      WaitForShow(kTabGroupEditorBubbleId),
      PressButton(kTabGroupEditorBubbleManageSharedGroupButtonId),
      WaitForShow(kDataSharingBubbleElementId), Do([&]() {
        DataSharingBubbleController::GetOrCreateForBrowser(browser())->Close();
      }),
      WaitForHide(kDataSharingBubbleElementId));

  histogram_tester.ExpectUniqueSample(
      kManageHistogram,
      saved_tab_groups::metrics::SharedTabGroupManageTypeDesktop::kManageGroup,
      1);
}

// Verify the DeleteGroup metric is recorded when the "Delete group" button is
// pressed in the tab group editor bubble.
IN_PROC_BROWSER_TEST_F(SharedTabGroupInteractiveUiTest,
                       RecordMetricWhenDeleteGroupPressed) {
  ::base::HistogramTester histogram_tester;

  TabGroupId group_id = CreateNewTabGroup();
  ShareTabGroup(group_id, "fake_collaboration_id");

  RunTestSequence(
      WaitForShow(kTabGroupHeaderElementId), FinishTabstripAnimations(),
      HoverTabGroupHeader(group_id), ClickMouse(ui_controls::RIGHT),
      WaitForShow(kTabGroupEditorBubbleId),
      PressButton(kTabGroupEditorBubbleDeleteGroupButtonId),
      WaitForShow(kDeletionDialogCancelButtonId),
      PressButton(kDeletionDialogCancelButtonId),
      WaitForHide(kDeletionDialogCancelButtonId), FinishTabstripAnimations());

  histogram_tester.ExpectUniqueSample(
      kManageHistogram,
      saved_tab_groups::metrics::SharedTabGroupManageTypeDesktop::kDeleteGroup,
      1);
}

}  // namespace tab_groups
