// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/collaboration/messaging/messaging_backend_service_factory.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_metrics.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test/tab_strip_interactive_test_mixin.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_tabs_menu_model.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_bubble_controller.h"
#include "chrome/browser/ui/views/tabs/recent_activity_bubble_dialog_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/collaboration/public/messaging/activity_log.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/features.h"
#include "components/data_sharing/public/group_data.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_id.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_connection.h"
#include "net/test/embedded_test_server/http_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interactive_test_internal.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace tab_groups {
constexpr char kSkipPixelTestsReason[] = "Should only run in pixel_tests.";
constexpr char kRecallHistogram[] = "TabGroups.Shared.Recall.Desktop";
constexpr char kManageHistogram[] = "TabGroups.Shared.Manage.Desktop";

class SharedTabGroupInteractiveUiTest
    : public TabStripInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  SharedTabGroupInteractiveUiTest() = default;
  ~SharedTabGroupInteractiveUiTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {tab_groups::kTabGroupSyncServiceDesktopMigration,
         data_sharing::features::kDataSharingFeature},
        {tabs::kTabGroupShortcuts});
    InProcessBrowserTest::SetUp();
  }

  MultiStep ShowBookmarksBar() {
    return Steps(PressButton(kToolbarAppMenuButtonElementId),
                 SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
                 SelectMenuItem(BookmarkSubMenuModel::kShowBookmarkBarMenuItem),
                 WaitForShow(kBookmarkBarElementId));
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

  void ShareTabGroup(TabGroupId group_id,
                     std::string collaboration_id,
                     data_sharing::MemberRole member_role,
                     bool should_sign_in) {
    TabGroupSyncService* service =
        TabGroupSyncServiceFactory::GetForProfile(browser()->profile());
    service->MakeTabGroupSharedForTesting(group_id, collaboration_id);

    // Additional Properties.
    const std::string display_name = "Display Name";
    const std::string email = "test@mail.com";
    const GURL avatar_url = GURL("chrome://newtab");
    const std::string given_name = "Given Name";
    const std::string access_token = "fake_access_token";
    const GaiaId gaia_id("fake_gaia_id");

    GaiaId gaia_id_to_use = gaia_id;
    if (should_sign_in) {
      // Simulate a signed in primary account.
      signin::IdentityManager* identity_manager =
          IdentityManagerFactory::GetForProfile(browser()->profile());
      signin::MakePrimaryAccountAvailable(identity_manager, email,
                                          signin::ConsentLevel::kSignin);
      signin::MakePrimaryAccountAvailable(identity_manager, email,
                                          signin::ConsentLevel::kSync);
      CoreAccountInfo account_info = identity_manager->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);

      gaia_id_to_use = account_info.gaia;
    }

    data_sharing::GroupMember group_member =
        data_sharing::GroupMember(gaia_id_to_use, display_name, email,
                                  member_role, avatar_url, given_name);
    data_sharing::GroupData group_data =
        data_sharing::GroupData(data_sharing::GroupId(collaboration_id),
                                display_name, {group_member}, {}, access_token);

    data_sharing_service()->AddGroupDataForTesting(std::move(group_data));
  }

  std::vector<collaboration::messaging::ActivityLogItem> CreateActivityLog(
      std::string collaboration_id) {
    using collaboration::messaging::CollaborationEvent;
    using collaboration::messaging::MessageAttribution;
    using collaboration::messaging::TabMessageMetadata;

    data_sharing::GroupMember trig_member;
    trig_member.given_name = "User";
    trig_member.avatar_url = GURL("https://google.com/avatar");

    TabMessageMetadata tab_metadata;
    tab_metadata.last_known_url = "https://.google.com";

    MessageAttribution attribution;
    attribution.triggering_user = trig_member;
    attribution.tab_metadata = tab_metadata;

    collaboration::messaging::ActivityLogItem item;
    item.collaboration_event = CollaborationEvent::TAB_ADDED;
    item.title_text = u"User added a tab";
    item.description_text = u"google.com";
    item.time_delta_text = u"2h ago";
    item.show_favicon = false;
    item.activity_metadata = attribution;

    return {item};
  }

  data_sharing::DataSharingService* data_sharing_service() {
    data_sharing::DataSharingService* data_sharing_service =
        data_sharing::DataSharingServiceFactory::GetForProfile(
            browser()->profile());
    return data_sharing_service;
  }

  collaboration::messaging::MessagingBackendService* messaging_service() {
    collaboration::messaging::MessagingBackendService* messaging_service =
        collaboration::messaging::MessagingBackendServiceFactory::GetForProfile(
            browser()->profile());
    return messaging_service;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verify the feedback button is only shown when there is at least one shared
// tab group in the current browser.
IN_PROC_BROWSER_TEST_F(SharedTabGroupInteractiveUiTest, FeedbackButtonVisible) {
  TabGroupId group_id = CreateNewTabGroup();
  ShareTabGroup(group_id, "fake_collaboration_id",
                data_sharing::MemberRole::kOwner, /*should_sign_in=*/false);

  // Manually activate an inactive tab since the test version of
  // MakeTabGroupShared does not fire observers.
  browser()->GetTabStripModel()->ActivateTabAt(1);

  RunTestSequence(
      // Verify the feedback button is visible when there is 1 shared tab group.
      FinishTabstripAnimations(), WaitForShow(kTabGroupHeaderElementId),
      WaitForShow(kSharedTabGroupFeedbackElementId),
      // Verify the feedback button is not visible if we remove it.
      HoverTabGroupHeader(group_id), ClickMouse(ui_controls::RIGHT),
      WaitForShow(kTabGroupEditorBubbleId),
      PressButton(kTabGroupEditorBubbleCloseGroupButtonId),
      WaitForHide(kTabGroupEditorBubbleCloseGroupButtonId),
      FinishTabstripAnimations(),
      WaitForHide(kSharedTabGroupFeedbackElementId));
}

// Take a screenshot of the shared tab group in app menu > tab groups.
IN_PROC_BROWSER_TEST_F(SharedTabGroupInteractiveUiTest,
                       SharedTabGroupInAppMenu) {
  TabGroupId group_id = CreateNewTabGroup();
  ShareTabGroup(group_id, "fake_collaboration_id",
                data_sharing::MemberRole::kOwner, /*should_sign_in=*/false);

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
  ShareTabGroup(group_id, "fake_collaboration_id",
                data_sharing::MemberRole::kOwner, /*should_sign_in=*/false);

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
  ShareTabGroup(group_id, "fake_collaboration_id",
                data_sharing::MemberRole::kOwner, /*should_sign_in=*/false);

  // TODO(crbug.com/380088920): Manually trigger a layout until we have a way to
  // know when the entity tracker is initialized.
  TabGroup* tab_group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(group_id);
  browser()->tab_strip_model()->ChangeTabGroupVisuals(
      group_id, *tab_group->visual_data());

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
  ShareTabGroup(group_id, "fake_collaboration_id",
                data_sharing::MemberRole::kOwner, /*should_sign_in=*/false);

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
  ShareTabGroup(group_id, "fake_collaboration_id",
                data_sharing::MemberRole::kOwner, /*should_sign_in=*/false);

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
  ShareTabGroup(group_id, "fake_collaboration_id",
                data_sharing::MemberRole::kOwner, /*should_sign_in=*/false);

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
  ShareTabGroup(group_id, "fake_collaboration_id",
                data_sharing::MemberRole::kOwner, /*should_sign_in=*/false);

  // Close the tab group.
  browser()->tab_strip_model()->CloseAllTabsInGroup(group_id);

  RunTestSequence(FinishTabstripAnimations(), ShowBookmarksBar(),
                  PressButton(kToolbarAppMenuButtonElementId),
                  WaitForShow(AppMenuModel::kTabGroupsMenuItem),
                  SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),
                  SelectMenuItem(STGEverythingMenu::kTabGroup),
                  SelectMenuItem(STGTabsMenuModel::kOpenGroup),
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

  RunTestSequence(
      WaitForShow(kTabGroupHeaderElementId), FinishTabstripAnimations(),
      HoverTabGroupHeader(group_id), ClickMouse(ui_controls::RIGHT),
      WaitForShow(kTabGroupEditorBubbleId),
      PressButton(kTabGroupEditorBubbleShareGroupButtonId),
      WaitForShow(kDataSharingSigninPromptDialogCancelButtonElementId),
      PressButton(kDataSharingSigninPromptDialogCancelButtonElementId),
      WaitForHide(kDataSharingSigninPromptDialogCancelButtonElementId));

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
  ShareTabGroup(group_id, "fake_collaboration_id",
                data_sharing::MemberRole::kOwner, /*should_sign_in=*/false);

  RunTestSequence(
      WaitForShow(kTabGroupHeaderElementId), FinishTabstripAnimations(),
      HoverTabGroupHeader(group_id), ClickMouse(ui_controls::RIGHT),
      WaitForShow(kTabGroupEditorBubbleId),
      PressButton(kTabGroupEditorBubbleManageSharedGroupButtonId),
      WaitForShow(kDataSharingSigninPromptDialogCancelButtonElementId),
      PressButton(kDataSharingSigninPromptDialogCancelButtonElementId),
      WaitForHide(kDataSharingSigninPromptDialogCancelButtonElementId));

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
  ShareTabGroup(group_id, "fake_collaboration_id",
                data_sharing::MemberRole::kOwner, /*should_sign_in=*/false);

  RunTestSequence(
      WaitForShow(kTabGroupHeaderElementId), FinishTabstripAnimations(),
      HoverTabGroupHeader(group_id), ClickMouse(ui_controls::RIGHT),
      WaitForShow(kTabGroupEditorBubbleId),
      PressButton(kTabGroupEditorBubbleDeleteGroupButtonId),
      WaitForShow(kDataSharingSigninPromptDialogCancelButtonElementId),
      PressButton(kDataSharingSigninPromptDialogCancelButtonElementId),
      WaitForHide(kDataSharingSigninPromptDialogCancelButtonElementId),
      FinishTabstripAnimations());

  histogram_tester.ExpectUniqueSample(
      kManageHistogram,
      saved_tab_groups::metrics::SharedTabGroupManageTypeDesktop::kDeleteGroup,
      1);
}

// Verify members see the leave group button instead of the delete button and
// that pressing the leave group buttons displays a dialog.
IN_PROC_BROWSER_TEST_F(SharedTabGroupInteractiveUiTest, LeaveGroupPressed) {
  TabGroupId group_id = CreateNewTabGroup();
  ShareTabGroup(group_id, "fake_collaboration_id",
                data_sharing::MemberRole::kMember, /*should_sign_in=*/true);

  RunTestSequence(WaitForShow(kTabGroupHeaderElementId),
                  FinishTabstripAnimations(), HoverTabGroupHeader(group_id),
                  ClickMouse(ui_controls::RIGHT),
                  WaitForShow(kTabGroupEditorBubbleId),
                  EnsurePresent(kTabGroupEditorBubbleLeaveGroupButtonId),
                  FinishTabstripAnimations());
}

// Verify members see the leave group button instead of the delete button in the
// context menu of a tab group. Pressing the button displays a dialog.
IN_PROC_BROWSER_TEST_F(SharedTabGroupInteractiveUiTest,
                       LeaveGroupPressedFromContextMenu) {
  TabGroupId group_id = CreateNewTabGroup();
  ShareTabGroup(group_id, "fake_collaboration_id",
                data_sharing::MemberRole::kMember, /*should_sign_in=*/true);

  RunTestSequence(WaitForShow(kTabGroupHeaderElementId),
                  FinishTabstripAnimations(),
                  PressButton(kToolbarAppMenuButtonElementId),
                  WaitForShow(AppMenuModel::kTabGroupsMenuItem),
                  SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),
                  SelectMenuItem(STGEverythingMenu::kTabGroup),
                  EnsurePresent(STGTabsMenuModel::kLeaveGroupMenuItem),
                  FinishTabstripAnimations());
}

// Verify remove last tab will display the close last tab dialog.
IN_PROC_BROWSER_TEST_F(SharedTabGroupInteractiveUiTest, GroupCloseLastTab) {
  TabGroupId group_id = CreateNewTabGroup();
  ShareTabGroup(group_id, "fake_collaboration_id",
                data_sharing::MemberRole::kMember, /*should_sign_in=*/false);

  RunTestSequence(
      WaitForShow(kTabGroupHeaderElementId), FinishTabstripAnimations(),
      Do([&]() {
        BrowserView* browser_view =
            static_cast<BrowserView*>(browser()->window());
        browser_view->tabstrip()->CloseTab(browser_view->tabstrip()->tab_at(0),
                                           CloseTabSource::kFromMouse);
      }),
      WaitForShow(kDataSharingSigninPromptDialogCancelButtonElementId),
      PressButton(kDataSharingSigninPromptDialogCancelButtonElementId),
      WaitForHide(kDataSharingSigninPromptDialogCancelButtonElementId),
      FinishTabstripAnimations());
}

// Verify members see the recent activity button when activity exists.
IN_PROC_BROWSER_TEST_F(SharedTabGroupInteractiveUiTest, RecentActivity) {
  std::string collaboration_id = "fake_collaboration_id";
  TabGroupId group_id = CreateNewTabGroup();
  ShareTabGroup(group_id, collaboration_id, data_sharing::MemberRole::kMember,
                /*should_sign_in=*/false);

  auto log = CreateActivityLog(collaboration_id);
  messaging_service()->AddActivityLogForTesting(
      data_sharing::GroupId(collaboration_id), log);

  RunTestSequence(
      WaitForShow(kTabGroupHeaderElementId), FinishTabstripAnimations(),
      HoverTabGroupHeader(group_id), ClickMouse(ui_controls::RIGHT),
      WaitForShow(kTabGroupEditorBubbleId),
      EnsurePresent(kTabGroupEditorBubbleRecentActivityButtonId), HoverTabAt(0),
      ClickMouse(), WaitForHide(kRecentActivityBubbleDialogId),
      FinishTabstripAnimations());
}

}  // namespace tab_groups
