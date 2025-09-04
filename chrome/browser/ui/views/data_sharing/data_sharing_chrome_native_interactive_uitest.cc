// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_bubble_controller.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_view_interface.h"
#include "chrome/browser/ui/views/test/tab_strip_interactive_test_mixin.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/data_sharing/public/data_sharing_utils.h"
#include "components/data_sharing/public/features.h"
#include "components/data_sharing/public/group_data.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"
#include "url/url_constants.h"

namespace tab_groups {

class DataSharingChromeNativeUiTest
    : public TabStripInteractiveTestMixin<InteractiveBrowserTest> {
 protected:
  DataSharingChromeNativeUiTest() = default;
  ~DataSharingChromeNativeUiTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {data_sharing::features::kDataSharingFeature}, {});
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  MultiStep ShowBookmarksBar() {
    return Steps(PressButton(kToolbarAppMenuButtonElementId),
                 SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
                 SelectMenuItem(BookmarkSubMenuModel::kShowBookmarkBarMenuItem),
                 WaitForShow(kBookmarkBarElementId));
  }

  MultiStep SaveGroupLeaveEditorBubbleOpen(tab_groups::TabGroupId group_id) {
    return Steps(EnsureNotPresent(kTabGroupEditorBubbleId),
                 // Right click on the header to open the editor bubble.
                 HoverTabGroupHeader(group_id), ClickMouse(ui_controls::RIGHT),
                 // Wait for the tab group editor bubble to appear.
                 WaitForShow(kTabGroupEditorBubbleId));
  }

  tab_groups::TabGroupId InstrumentATabGroup() {
    // Add 1 tab into the browser. And verify there are 2 tabs (The tab when you
    // open the browser and the added one).
    EXPECT_TRUE(
        AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
    EXPECT_EQ(2, browser()->tab_strip_model()->count());
    return browser()->tab_strip_model()->AddToNewGroup({0, 1});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DataSharingChromeNativeUiTest, ShowShareBubble) {
  tab_groups::LocalTabGroupID group_id = InstrumentATabGroup();
  RunTestSequence(
      FinishTabstripAnimations(), SaveGroupLeaveEditorBubbleOpen(group_id),
      WaitForShow(kTabGroupEditorBubbleShareGroupButtonId), Do([=, this]() {
        // Directly show share UI to bypass sign in flow.
        data_sharing::RequestInfo request_info(group_id,
                                               data_sharing::FlowType::kShare);
        DataSharingBubbleController::From(browser())->Show(request_info);
      }),
      WaitForShow(kDataSharingBubbleElementId),
      // Check the share bubble is anchored onto the group header view.
      CheckView(kDataSharingBubbleElementId,
                [&](views::BubbleDialogDelegateView* bubble) {
                  const auto* const browser_view =
                      BrowserView::GetBrowserViewForBrowser(browser());
                  const views::View* const group_header =
                      browser_view->tab_strip_view()->GetTabGroupAnchorView(
                          group_id);
                  return group_header &&
                         bubble->GetAnchorView() == group_header;
                }));
}

IN_PROC_BROWSER_TEST_F(DataSharingChromeNativeUiTest, ShowManageBubble) {
  auto* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          browser()->profile());
  tab_groups::LocalTabGroupID group_id = InstrumentATabGroup();
  std::optional<tab_groups::SavedTabGroup> group =
      tab_group_service->GetGroup(group_id);
  syncer::CollaborationId fake_collab_id("fake_collab_id");
  group->SetCollaborationId(fake_collab_id);
  tab_group_service->RemoveGroup(group->saved_guid());
  tab_group_service->AddGroup(group.value());

  RunTestSequence(
      FinishTabstripAnimations(), SaveGroupLeaveEditorBubbleOpen(group_id),
      WaitForShow(kTabGroupEditorBubbleManageSharedGroupButtonId),
      Do([=, this]() {
        // Directly show manage UI to bypass sign in flow.
        data_sharing::RequestInfo request_info(group_id,
                                               data_sharing::FlowType::kManage);
        DataSharingBubbleController::From(browser())->Show(request_info);
      }),
      WaitForShow(kDataSharingBubbleElementId),
      CheckView(kDataSharingBubbleElementId, [](views::View* bubble) {
        return bubble->GetWidget()->IsModal();
      }));
}

IN_PROC_BROWSER_TEST_F(DataSharingChromeNativeUiTest, ShowJoinBubble) {
  std::string fake_collab_id = "fake_collab_id";
  std::string fake_access_token = "fake_access_token";

  RunTestSequence(
      Do([=, this]() {
        auto share_link = data_sharing::GetShareLink(
            fake_collab_id, fake_access_token, browser()->profile());
        // Directly show join UI to bypass sign in flow.
        data_sharing::RequestInfo request_info(
            data_sharing::DataSharingUtils::ParseDataSharingUrl(share_link)
                .value(),
            data_sharing::FlowType::kJoin);
        DataSharingBubbleController::From(browser())->Show(request_info);
      }),
      WaitForShow(kDataSharingBubbleElementId),
      CheckView(kDataSharingBubbleElementId, [](views::View* bubble) {
        return bubble->GetWidget()->IsModal();
      }));
}

IN_PROC_BROWSER_TEST_F(DataSharingChromeNativeUiTest, GenerateWebUIUrl) {
  tab_groups::LocalTabGroupID group_id = InstrumentATabGroup();
  syncer::CollaborationId fake_collab_id("fake_collab_id");
  std::string fake_access_token = "fake_access_token";
  std::string fake_tab_group_title = "fake_title";

  auto expected_share_flow_url =
      GURL(std::string(chrome::kChromeUIUntrustedDataSharingURL) + "?" +
           std::string(data_sharing::kQueryParamFlow) + "=" +
           std::string(data_sharing::kFlowShare) + "&" +
           std::string(data_sharing::kQueryParamTabGroupId) + "=" +
           group_id.ToString() + "&" +
           std::string(data_sharing::kQueryParamTabGroupTitle) + "=" +
           fake_tab_group_title);

  auto expected_manage_flow_url =
      GURL(std::string(chrome::kChromeUIUntrustedDataSharingURL) + "?" +
           std::string(data_sharing::kQueryParamFlow) + "=" +
           std::string(data_sharing::kFlowManage) + "&" +
           std::string(data_sharing::kQueryParamGroupId) + "=" +
           fake_collab_id.value() + "&" +
           std::string(data_sharing::kQueryParamTabGroupId) + "=" +
           group_id.ToString() + "&" +
           std::string(data_sharing::kQueryParamIsDisabledForPolicy) + "=" +
           "false" + "&" + std::string(data_sharing::kQueryParamTabGroupTitle) +
           "=" + fake_tab_group_title);

  auto expected_leave_flow_url =
      GURL(std::string(chrome::kChromeUIUntrustedDataSharingURL) + "?" +
           std::string(data_sharing::kQueryParamFlow) + "=" +
           std::string(data_sharing::kFlowLeave) + "&" +
           std::string(data_sharing::kQueryParamGroupId) + "=" +
           fake_collab_id.value() + "&" +
           std::string(data_sharing::kQueryParamTabGroupTitle) + "=" +
           fake_tab_group_title);

  auto expected_join_flow_url =
      GURL(std::string(chrome::kChromeUIUntrustedDataSharingURL) + "?" +
           std::string(data_sharing::kQueryParamFlow) + "=" +
           std::string(data_sharing::kFlowJoin) + "&" +
           std::string(data_sharing::kQueryParamGroupId) + "=" +
           fake_collab_id.value() + "&" +
           std::string(data_sharing::kQueryParamTokenSecret) + "=" +
           fake_access_token);

  auto expected_delete_flow_url_with_token =
      GURL(std::string(chrome::kChromeUIUntrustedDataSharingURL) + "?" +
           std::string(data_sharing::kQueryParamFlow) + "=" +
           std::string(data_sharing::kFlowDelete) + "&" +
           std::string(data_sharing::kQueryParamGroupId) + "=" +
           fake_collab_id.value() + "&" +
           std::string(data_sharing::kQueryParamTabGroupTitle) + "=" +
           fake_tab_group_title);

  auto expected_delete_flow_url_with_tab_group_id =
      GURL(std::string(chrome::kChromeUIUntrustedDataSharingURL) + "?" +
           std::string(data_sharing::kQueryParamFlow) + "=" +
           std::string(data_sharing::kFlowDelete) + "&" +
           std::string(data_sharing::kQueryParamGroupId) + "=" +
           fake_collab_id.value() + "&" +
           std::string(data_sharing::kQueryParamTabGroupTitle) + "=" +
           fake_tab_group_title);

  auto expected_close_flow_url_with_token =
      GURL(std::string(chrome::kChromeUIUntrustedDataSharingURL) + "?" +
           std::string(data_sharing::kQueryParamFlow) + "=" +
           std::string(data_sharing::kFlowClose) + "&" +
           std::string(data_sharing::kQueryParamGroupId) + "=" +
           fake_collab_id.value() + "&" +
           std::string(data_sharing::kQueryParamTabGroupTitle) + "=" +
           fake_tab_group_title);

  auto expected_close_flow_url_with_tab_group_id =
      GURL(std::string(chrome::kChromeUIUntrustedDataSharingURL) + "?" +
           std::string(data_sharing::kQueryParamFlow) + "=" +
           std::string(data_sharing::kFlowClose) + "&" +
           std::string(data_sharing::kQueryParamGroupId) + "=" +
           fake_collab_id.value() + "&" +
           std::string(data_sharing::kQueryParamTabGroupTitle) + "=" +
           fake_tab_group_title);

  TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          browser()->profile());

  std::optional<tab_groups::SavedTabGroup> group =
      tab_group_service->GetGroup(group_id);
  group->SetTitle(base::UTF8ToUTF16(fake_tab_group_title));
  tab_group_service->RemoveGroup(group->saved_guid());
  tab_group_service->AddGroup(group.value());

  data_sharing::RequestInfo request_info_share(group_id,
                                               data_sharing::FlowType::kShare);
  auto url =
      data_sharing::GenerateWebUIUrl(request_info_share, browser()->profile());
  EXPECT_EQ(url.value().spec(), expected_share_flow_url);

  tab_group_service->MakeTabGroupSharedForTesting(group_id, fake_collab_id);

  data_sharing::RequestInfo request_info_manage(
      group_id, data_sharing::FlowType::kManage);
  url =
      data_sharing::GenerateWebUIUrl(request_info_manage, browser()->profile());
  EXPECT_EQ(url.value().spec(), expected_manage_flow_url);

  data_sharing::GroupToken token = data_sharing::GroupToken(
      data_sharing::GroupId(fake_collab_id.value()), fake_access_token);
  data_sharing::RequestInfo request_info_join(token,
                                              data_sharing::FlowType::kJoin);
  url = data_sharing::GenerateWebUIUrl(request_info_join, browser()->profile());
  EXPECT_EQ(url.value().spec(), expected_join_flow_url);

  data_sharing::RequestInfo request_info_leave(token,
                                               data_sharing::FlowType::kLeave);
  url =
      data_sharing::GenerateWebUIUrl(request_info_leave, browser()->profile());
  EXPECT_EQ(url.value().spec(), expected_leave_flow_url);

  data_sharing::RequestInfo request_info_delete_with_tab_group_id(
      group_id, data_sharing::FlowType::kDelete);
  url = data_sharing::GenerateWebUIUrl(request_info_delete_with_tab_group_id,
                                       browser()->profile());
  EXPECT_EQ(url.value().spec(), expected_delete_flow_url_with_tab_group_id);

  data_sharing::GroupToken token2 = data_sharing::GroupToken(
      data_sharing::GroupId(fake_collab_id.value()), fake_access_token);
  data_sharing::RequestInfo request_info_delete_with_token(
      token2, data_sharing::FlowType::kDelete);
  url = data_sharing::GenerateWebUIUrl(request_info_delete_with_token,
                                       browser()->profile());
  EXPECT_EQ(url.value().spec(), expected_delete_flow_url_with_token);

  data_sharing::RequestInfo request_info_close_with_tab_group_id(
      group_id, data_sharing::FlowType::kClose);
  url = data_sharing::GenerateWebUIUrl(request_info_close_with_tab_group_id,
                                       browser()->profile());
  EXPECT_EQ(url.value().spec(), expected_close_flow_url_with_tab_group_id);

  data_sharing::RequestInfo request_info_close_with_token(
      token2, data_sharing::FlowType::kClose);
  url = data_sharing::GenerateWebUIUrl(request_info_close_with_token,
                                       browser()->profile());
  EXPECT_EQ(url.value().spec(), expected_close_flow_url_with_token);
}

IN_PROC_BROWSER_TEST_F(DataSharingChromeNativeUiTest,
                       CloseBubbleResetProgress) {
  auto* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          browser()->profile());
  tab_groups::LocalTabGroupID group_id = InstrumentATabGroup();
  std::optional<tab_groups::SavedTabGroup> group =
      tab_group_service->GetGroup(group_id);
  syncer::CollaborationId fake_collab_id("fake_collab_id");
  group->SetCollaborationId(fake_collab_id);
  tab_group_service->RemoveGroup(group->saved_guid());
  tab_group_service->AddGroup(group.value());

  RunTestSequence(
      FinishTabstripAnimations(), SaveGroupLeaveEditorBubbleOpen(group_id),
      WaitForShow(kTabGroupEditorBubbleManageSharedGroupButtonId),
      Do([=, this]() {
        // Ensure action and progress set OnGroupAction
        auto* bubble_controller = DataSharingBubbleController::From(browser());
        data_sharing::RequestInfo request_info(group_id,
                                               data_sharing::FlowType::kDelete);
        bubble_controller->Show(request_info);

        EXPECT_EQ(std::nullopt, bubble_controller->group_action_for_testing());
        EXPECT_EQ(std::nullopt,
                  bubble_controller->group_action_progress_for_testing());
        bubble_controller->OnGroupAction(
            data_sharing::mojom::GroupAction::kDeleteGroup,
            data_sharing::mojom::GroupActionProgress::kSuccess);
        EXPECT_EQ(data_sharing::mojom::GroupAction::kDeleteGroup,
                  bubble_controller->group_action_for_testing());
        EXPECT_EQ(data_sharing::mojom::GroupActionProgress::kSuccess,
                  bubble_controller->group_action_progress_for_testing());
      }),
      WaitForShow(kDataSharingBubbleElementId), Do([=, this]() {
        // Ensure action and progress reset on dialog close.
        auto* bubble_controller = DataSharingBubbleController::From(browser());
        bubble_controller->Close();
        EXPECT_EQ(std::nullopt, bubble_controller->group_action_for_testing());
        EXPECT_EQ(std::nullopt,
                  bubble_controller->group_action_progress_for_testing());
      }));
}

}  // namespace tab_groups
