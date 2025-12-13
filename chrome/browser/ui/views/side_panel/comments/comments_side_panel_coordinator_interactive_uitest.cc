// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/side_panel/comments/comments_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/test/tab_strip_interactive_test_mixin.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/collaboration/public/features.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/features.h"
#include "components/data_sharing/public/group_data.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"

class CommentsSidePanelCoordinatorInteractiveUiTest
    : public TabStripInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {
            data_sharing::features::kDataSharingFeature,
            collaboration::features::kCollaborationComments,
        },
        {});
    InteractiveBrowserTest::SetUp();
  }

  tab_groups::TabGroupId CreateNewTabGroup(std::u16string title = u"") {
    EXPECT_TRUE(
        AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
    tab_groups::TabGroupId group_id =
        browser()->tab_strip_model()->AddToNewGroup({0});
    tab_groups::TabGroupVisualData updates(std::u16string(title),
                                           tab_groups::TabGroupColorId::kGrey);
    tab_group_sync_service()->UpdateVisualData(group_id, &updates);
    return group_id;
  }

  void ShareTabGroup(tab_groups::TabGroupId group_id,
                     syncer::CollaborationId collaboration_id,
                     data_sharing::MemberRole member_role,
                     bool should_sign_in) {
    tab_group_sync_service()->MakeTabGroupSharedForTesting(group_id,
                                                           collaboration_id);

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
        data_sharing::GroupData(data_sharing::GroupId(collaboration_id.value()),
                                display_name, {group_member}, {}, access_token);

    data_sharing_service()->AddGroupDataForTesting(std::move(group_data));
  }

  tab_groups::TabGroupSyncService* tab_group_sync_service() {
    return tab_groups::TabGroupSyncServiceFactory::GetForProfile(
        browser()->profile());
  }

  data_sharing::DataSharingService* data_sharing_service() {
    data_sharing::DataSharingService* data_sharing_service =
        data_sharing::DataSharingServiceFactory::GetForProfile(
            browser()->profile());
    return data_sharing_service;
  }

  CommentsSidePanelCoordinator* side_panel_coordinator() {
    return browser()->GetFeatures().comments_side_panel_coordinator();
  }

  actions::ActionItem* GetActionItemForCommentsSidePanel() {
    return actions::ActionManager::Get().FindAction(
        kActionSidePanelShowComments,
        browser()->GetActions()->root_action_item());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CommentsSidePanelCoordinatorInteractiveUiTest,
                       EntryIsRegistered) {
  // The comments entry should be registered in the window registry.
  EXPECT_EQ(
      SidePanelRegistry::From(browser())
          ->GetEntryForKey(SidePanelEntry::Key(SidePanelEntry::Id::kComments))
          ->key()
          .id(),
      SidePanelEntry::Id::kComments);
}

// Verify the comments action is only shown when the active tab is shared.
IN_PROC_BROWSER_TEST_F(CommentsSidePanelCoordinatorInteractiveUiTest,
                       CommentActionIsVisible) {
  CreateNewTabGroup();
  tab_groups::TabGroupId group_id = CreateNewTabGroup();
  ShareTabGroup(group_id, syncer::CollaborationId("fake_collaboration_id"),
                data_sharing::MemberRole::kOwner, /*should_sign_in=*/false);

  const int shared_tab_index = 0;
  const int non_shared_tab_index = 1;
  const int ungrouped_tab_index = 2;

  browser()->tab_strip_model()->ActivateTabAt(ungrouped_tab_index);

  RunTestSequence(
      // Verify the comments action is visible when the active tab is shared.
      HoverTabAt(shared_tab_index), ClickMouse(), FinishTabstripAnimations(),
      WaitForShow(kSharedTabGroupCommentsActionElementId),

      // Activate non-shared tab.
      HoverTabAt(non_shared_tab_index), ClickMouse(),
      FinishTabstripAnimations(),
      WaitForHide(kSharedTabGroupCommentsActionElementId),

      // Activate ungrouped tab.
      HoverTabAt(ungrouped_tab_index), ClickMouse(), FinishTabstripAnimations(),
      WaitForHide(kSharedTabGroupCommentsActionElementId),

      // Verify the comments action is visible when the tab is shared.
      HoverTabAt(shared_tab_index), ClickMouse(), FinishTabstripAnimations(),
      WaitForShow(kSharedTabGroupCommentsActionElementId));
}

// Verify the comments action is shown when a tab is added to a shared group.
IN_PROC_BROWSER_TEST_F(CommentsSidePanelCoordinatorInteractiveUiTest,
                       CommentActionIsVisible_AddingTabToGroup) {
  tab_groups::TabGroupId group_id = CreateNewTabGroup();
  ShareTabGroup(group_id, syncer::CollaborationId("fake_collaboration_id"),
                data_sharing::MemberRole::kOwner, /*should_sign_in=*/false);

  // Simplest way to add a tab to group is to add the tab at the beginning of
  // the tab strip and drag it to group header to its right.
  EXPECT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  const int ungrouped_tab_index = 0;

  browser()->tab_strip_model()->ActivateTabAt(ungrouped_tab_index);

  RunTestSequence(WaitForShow(kTabGroupHeaderElementId),
                  EnsureNotPresent(kSharedTabGroupCommentsActionElementId),
                  HoverTabAt(ungrouped_tab_index),
                  DragMouseTo(kTabGroupHeaderElementId), Do([&]() {
                    // Verify the tab was added to the group.
                    TabGroupModel* tab_group_model =
                        browser()->tab_strip_model()->group_model();
                    EXPECT_EQ(
                        tab_group_model->GetTabGroup(group_id)->tab_count(), 2);
                  }),
                  WaitForShow(kSharedTabGroupCommentsActionElementId));
}

// Verify the comments action is shown when a tab group becomes shared.
IN_PROC_BROWSER_TEST_F(CommentsSidePanelCoordinatorInteractiveUiTest,
                       CommentActionIsVisible_SharingGroup) {
  tab_groups::TabGroupId group_id = CreateNewTabGroup();

  const int non_shared_tab_index = 0;
  browser()->tab_strip_model()->ActivateTabAt(non_shared_tab_index);

  RunTestSequence(
      WaitForShow(kTabGroupHeaderElementId),
      EnsureNotPresent(kSharedTabGroupCommentsActionElementId),
      // Share the group.
      Do([&] {
        ShareTabGroup(
            group_id, syncer::CollaborationId("fake_collaboration_id"),
            data_sharing::MemberRole::kOwner, /*should_sign_in=*/false);

        // Trigger observers to fire by updating the group's visual data.
        tab_group_sync_service()->UpdateVisualData(group_id,
                                                   browser()
                                                       ->GetTabStripModel()
                                                       ->group_model()
                                                       ->GetTabGroup(group_id)
                                                       ->visual_data());
      }),
      WaitForShow(kSharedTabGroupCommentsActionElementId),
      // Unshare the group.
      Do([&] {
        EXPECT_TRUE(tab_group_sync_service()
                        ->GetGroup(group_id)
                        ->is_shared_tab_group());
        tab_group_sync_service()->MakeTabGroupUnsharedForTesting(group_id);
        EXPECT_FALSE(tab_group_sync_service()
                         ->GetGroup(group_id)
                         ->is_shared_tab_group());

        // Trigger observers to fire by updating the group's visual data.
        tab_group_sync_service()->UpdateVisualData(group_id,
                                                   browser()
                                                       ->GetTabStripModel()
                                                       ->group_model()
                                                       ->GetTabGroup(group_id)
                                                       ->visual_data());
      }),
      WaitForHide(kSharedTabGroupCommentsActionElementId));
}

// Verify the comments side panel will resume visilibity when switching to a
// non-shared tab and back.
IN_PROC_BROWSER_TEST_F(CommentsSidePanelCoordinatorInteractiveUiTest,
                       CommentSidePanelIsVisible) {
  CreateNewTabGroup();
  tab_groups::TabGroupId group_id = CreateNewTabGroup();
  ShareTabGroup(group_id, syncer::CollaborationId("fake_collaboration_id"),
                data_sharing::MemberRole::kOwner, /*should_sign_in=*/false);

  const int shared_tab_index = 0;
  const int non_shared_tab_index = 1;
  const int ungrouped_tab_index = 2;

  browser()->tab_strip_model()->ActivateTabAt(ungrouped_tab_index);

  RunTestSequence(
      // Verify the comments side panel can be opened when the active tab is
      // shared.
      HoverTabAt(shared_tab_index), ClickMouse(), FinishTabstripAnimations(),
      WaitForShow(kSharedTabGroupCommentsActionElementId),
      PressButton(kSharedTabGroupCommentsActionElementId),
      WaitForShow(kSidePanelElementId),

      // Activate non-shared tab.
      HoverTabAt(non_shared_tab_index), ClickMouse(),
      FinishTabstripAnimations(), WaitForHide(kSidePanelElementId),

      // Activate ungrouped tab.
      HoverTabAt(ungrouped_tab_index), ClickMouse(), FinishTabstripAnimations(),
      WaitForHide(kSidePanelElementId),

      // Verify the comments side panel is resumed without clicking the comments
      // action.
      HoverTabAt(shared_tab_index), ClickMouse(), FinishTabstripAnimations(),
      WaitForShow(kSidePanelElementId));
}

// Verify the comments side panel will update the title when the active tab
// group changes.
IN_PROC_BROWSER_TEST_F(CommentsSidePanelCoordinatorInteractiveUiTest,
                       CommentSidePanelTitleUpdates) {
  tab_groups::TabGroupId group_id1 = CreateNewTabGroup(u"Group 1");
  tab_groups::TabGroupId group_id2 = CreateNewTabGroup(u"Group 2");
  ShareTabGroup(group_id1, syncer::CollaborationId("fake_collaboration_id"),
                data_sharing::MemberRole::kOwner, /*should_sign_in=*/false);
  ShareTabGroup(group_id2, syncer::CollaborationId("fake_collaboration_id"),
                data_sharing::MemberRole::kOwner, /*should_sign_in=*/false);

  const int group2_tab_index = 0;
  const int group1_tab_index = 1;
  const int ungrouped_tab_index = 2;

  browser()->tab_strip_model()->ActivateTabAt(ungrouped_tab_index);

  RunTestSequence(
      // Initially, the title should have no group name.
      Do([&]() {
        actions::ActionItem* action_item = GetActionItemForCommentsSidePanel();
        EXPECT_EQ(u"Comments", action_item->GetText());
      }),

      // Verify the comments action will be updated with the group name.
      SelectTab(kTabStripElementId, group1_tab_index, InputType::kMouse),
      WaitForActiveTabChange(group1_tab_index),
      WaitForShow(kSharedTabGroupCommentsActionElementId),
      PressButton(kSharedTabGroupCommentsActionElementId),
      WaitForShow(kSidePanelElementId), FinishTabstripAnimations(), Do([&]() {
        actions::ActionItem* action_item = GetActionItemForCommentsSidePanel();
        EXPECT_EQ(u"Comments - Group 1", action_item->GetText());
      }),

      // Activate another shared tab, verify the title is updated.
      SelectTab(kTabStripElementId, group2_tab_index, InputType::kMouse),
      WaitForActiveTabChange(group2_tab_index), Do([&]() {
        actions::ActionItem* action_item = GetActionItemForCommentsSidePanel();
        EXPECT_EQ(u"Comments - Group 2", action_item->GetText());
      }));
}
