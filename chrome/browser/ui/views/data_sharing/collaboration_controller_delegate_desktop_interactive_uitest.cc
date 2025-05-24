// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/mock_callback.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/data_sharing/collaboration_controller_delegate_desktop.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_bubble_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/collaboration/public/collaboration_controller_delegate.h"
#include "components/collaboration/public/service_status.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/public/collaboration_finder.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/sync/base/collaboration_id.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/views/interaction/interactive_views_test.h"

namespace {
class TestCollaborationControllerDelegateDesktop
    : public CollaborationControllerDelegateDesktop {
 public:
  explicit TestCollaborationControllerDelegateDesktop(
      Browser* browser,
      std::optional<data_sharing::FlowType> flow = std::nullopt)
      : CollaborationControllerDelegateDesktop(browser, flow) {}
  MOCK_METHOD(collaboration::ServiceStatus, GetServiceStatus, (), (override));
};

}  // namespace

class CollaborationControllerDelegateDesktopInteractiveUITest
    : public InteractiveBrowserTest {
 public:
  CollaborationControllerDelegateDesktopInteractiveUITest() = default;

  CollaborationControllerDelegateDesktopInteractiveUITest(
      const CollaborationControllerDelegateDesktopInteractiveUITest&) = delete;
  CollaborationControllerDelegateDesktopInteractiveUITest& operator=(
      const CollaborationControllerDelegateDesktopInteractiveUITest&) = delete;

  ~CollaborationControllerDelegateDesktopInteractiveUITest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {data_sharing::features::kDataSharingFeature,
         tab_groups::kTabGroupSyncServiceDesktopMigration},
        {});
    InProcessBrowserTest::SetUp();
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

IN_PROC_BROWSER_TEST_F(CollaborationControllerDelegateDesktopInteractiveUITest,
                       ShowPromptDialog) {
  // Show prompt dialog when not signed in and signed not enabled.
  collaboration::ServiceStatus status;
  TestCollaborationControllerDelegateDesktop delegate(browser());
  EXPECT_CALL(delegate, GetServiceStatus()).WillOnce(testing::Return(status));
  EXPECT_EQ(nullptr, delegate.prompt_dialog_widget_for_testing());
  base::MockCallback<
      collaboration::CollaborationControllerDelegate::ResultCallback>
      callback;
  delegate.ShowAuthenticationUi(collaboration::FlowType::kJoin, callback.Get());
  EXPECT_NE(nullptr, delegate.prompt_dialog_widget_for_testing());
}

IN_PROC_BROWSER_TEST_F(CollaborationControllerDelegateDesktopInteractiveUITest,
                       DoNotShowPromptDialog) {
  // Do not show prompt dialog when signed in and sync enabled.
  collaboration::ServiceStatus status;
  status.signin_status = collaboration::SigninStatus::kSignedIn;
  status.sync_status = collaboration::SyncStatus::kSyncEnabled;
  TestCollaborationControllerDelegateDesktop delegate(browser());
  EXPECT_CALL(delegate, GetServiceStatus()).WillOnce(testing::Return(status));
  EXPECT_EQ(nullptr, delegate.prompt_dialog_widget_for_testing());
  base::MockCallback<
      collaboration::CollaborationControllerDelegate::ResultCallback>
      callback;
  delegate.ShowAuthenticationUi(collaboration::FlowType::kJoin, callback.Get());
  EXPECT_EQ(nullptr, delegate.prompt_dialog_widget_for_testing());
}

IN_PROC_BROWSER_TEST_F(CollaborationControllerDelegateDesktopInteractiveUITest,
                       ShowErrorDialog) {
  // Show error dialog.
  TestCollaborationControllerDelegateDesktop delegate(browser());
  EXPECT_EQ(nullptr, delegate.error_dialog_widget_for_testing());
  base::MockCallback<
      collaboration::CollaborationControllerDelegate::ResultCallback>
      callback;
  delegate.ShowError(collaboration::CollaborationControllerDelegate::ErrorInfo(
                         collaboration::CollaborationControllerDelegate::
                             ErrorInfo::Type::kUnknown),
                     callback.Get());
  EXPECT_NE(nullptr, delegate.error_dialog_widget_for_testing());
}

IN_PROC_BROWSER_TEST_F(CollaborationControllerDelegateDesktopInteractiveUITest,
                       ShowJoinDialog) {
  TestCollaborationControllerDelegateDesktop delegate(browser());
  std::string fake_collab_id = "fake_collab_id";
  std::string fake_access_token = "fake_access_token";
  data_sharing::GroupToken token = data_sharing::GroupToken(
      data_sharing::GroupId(fake_collab_id), fake_access_token);
  data_sharing::SharedDataPreview preview_data;
  base::MockCallback<
      collaboration::CollaborationControllerDelegate::ResultCallback>
      callback;
  RunTestSequence(Do([&]() {
                    delegate.ShowJoinDialog(token, preview_data,
                                            callback.Get());
                  }),
                  WaitForShow(kDataSharingBubbleElementId));
}

IN_PROC_BROWSER_TEST_F(CollaborationControllerDelegateDesktopInteractiveUITest,
                       ShowJoinDialogWithError) {
  TestCollaborationControllerDelegateDesktop delegate(browser());
  std::string fake_collab_id = "fake_collab_id";
  std::string fake_access_token = "fake_access_token";
  data_sharing::GroupToken token = data_sharing::GroupToken(
      data_sharing::GroupId(fake_collab_id), fake_access_token);
  data_sharing::SharedDataPreview preview_data;
  base::MockCallback<
      collaboration::CollaborationControllerDelegate::ResultCallback>
      callback;
  RunTestSequence(
      Do([&]() {
        delegate.ShowJoinDialog(token, preview_data, callback.Get());
      }),
      WaitForShow(kDataSharingBubbleElementId), Do([&]() {
        // Close join dialog and show the error dialog.
        auto* controller =
            DataSharingBubbleController::GetOrCreateForBrowser(browser());
        controller->Close();
        controller->ShowErrorDialog(
            static_cast<int>(absl::StatusCode::kUnknown));
      }),
      WaitForShow(kDataSharingErrorDialogOkButtonElementId));
}

IN_PROC_BROWSER_TEST_F(CollaborationControllerDelegateDesktopInteractiveUITest,
                       ShowShareDialog) {
  TestCollaborationControllerDelegateDesktop delegate(browser());
  tab_groups::LocalTabGroupID group_id = InstrumentATabGroup();
  base::MockCallback<base::OnceCallback<void(
      collaboration::CollaborationControllerDelegate::Outcome,
      std::optional<data_sharing::GroupToken>)>>
      callback;
  RunTestSequence(
      Do([&]() { delegate.ShowShareDialog(group_id, callback.Get()); }),
      WaitForShow(kDataSharingBubbleElementId), Do([&]() {
        // Close the dialog before the callback runs out of scope.
        auto* controller =
            DataSharingBubbleController::GetOrCreateForBrowser(browser());
        controller->Close();
      }));
}

IN_PROC_BROWSER_TEST_F(CollaborationControllerDelegateDesktopInteractiveUITest,
                       ShowManageDialog) {
  TestCollaborationControllerDelegateDesktop delegate(browser());

  // Add a saved tab group with fake_collab_id
  std::string fake_collab_id = "fake_collab_id";
  tab_groups::LocalTabGroupID group_id = InstrumentATabGroup();
  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          browser()->GetProfile());
  tab_group_service->MakeTabGroupSharedForTesting(group_id, fake_collab_id);

  base::MockCallback<
      collaboration::CollaborationControllerDelegate::ResultCallback>
      callback;
  RunTestSequence(
      Do([&]() { delegate.ShowManageDialog(group_id, callback.Get()); }),
      WaitForShow(kDataSharingBubbleElementId));
}

IN_PROC_BROWSER_TEST_F(CollaborationControllerDelegateDesktopInteractiveUITest,
                       ShowDeleteDialog) {
  TestCollaborationControllerDelegateDesktop delegate(
      browser(), data_sharing::FlowType::kDelete);

  // Add a saved tab group with fake_collab_id
  std::string fake_collab_id = "fake_collab_id";
  tab_groups::LocalTabGroupID group_id = InstrumentATabGroup();
  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          browser()->GetProfile());
  tab_group_service->MakeTabGroupSharedForTesting(group_id, fake_collab_id);
  std::optional<tab_groups::SavedTabGroup> group =
      tab_group_service->GetGroup(group_id);

  base::MockCallback<
      collaboration::CollaborationControllerDelegate::ResultCallback>
      callback;
  RunTestSequence(Do([&]() {
                    delegate.ShowDeleteDialog(group->saved_guid(),
                                              callback.Get());
                  }),
                  WaitForShow(kDataSharingBubbleElementId));
}

IN_PROC_BROWSER_TEST_F(CollaborationControllerDelegateDesktopInteractiveUITest,
                       PromoteCurrentScreen) {
  // Make sure prompt dialog is shown.
  collaboration::ServiceStatus status;
  TestCollaborationControllerDelegateDesktop delegate(browser());
  EXPECT_CALL(delegate, GetServiceStatus()).WillOnce(testing::Return(status));
  EXPECT_EQ(nullptr, delegate.prompt_dialog_widget_for_testing());
  base::MockCallback<
      collaboration::CollaborationControllerDelegate::ResultCallback>
      callback;
  delegate.PromoteCurrentScreen();
  EXPECT_NE(nullptr, delegate.prompt_dialog_widget_for_testing());
}

IN_PROC_BROWSER_TEST_F(CollaborationControllerDelegateDesktopInteractiveUITest,
                       PromoteTabGroup) {
  std::string fake_collab_id = "fake_collab_id";
  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          browser()->GetProfile());

  // Add a saved tab group with fake_collab_id
  tab_groups::LocalTabGroupID group_id = InstrumentATabGroup();
  tab_group_service->MakeTabGroupSharedForTesting(group_id, fake_collab_id);

  // Make sure PromoteTabGroup() is successful.
  TestCollaborationControllerDelegateDesktop delegate(browser());
  base::MockCallback<
      collaboration::CollaborationControllerDelegate::ResultCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(collaboration::CollaborationControllerDelegate::Outcome::kSuccess))
      .Times(1);
  delegate.PromoteTabGroup(data_sharing::GroupId(fake_collab_id),
                           callback.Get());
}

IN_PROC_BROWSER_TEST_F(CollaborationControllerDelegateDesktopInteractiveUITest,
                       OnBrowserClose) {
  Browser* browser2 = CreateBrowser(browser()->profile());
  TestCollaborationControllerDelegateDesktop delegate(browser2);
  base::MockCallback<base::OnceCallback<void()>> exit_callback;
  base::MockCallback<
      collaboration::CollaborationControllerDelegate::ResultCallback>
      callback;

  // Pass in exit callback.
  delegate.PrepareFlowUI(exit_callback.Get(), callback.Get());

  // Make sure closing a browser will invoke the exit callback.
  EXPECT_CALL(exit_callback, Run).Times(1);
  browser2->window()->Close();
}
