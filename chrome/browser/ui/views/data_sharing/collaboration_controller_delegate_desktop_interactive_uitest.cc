// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/signin/promos/signin_promo_tab_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/data_sharing/collaboration_controller_delegate_desktop.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_bubble_controller.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/collaboration/public/collaboration_controller_delegate.h"
#include "components/collaboration/public/collaboration_flow_type.h"
#include "components/collaboration/public/service_status.h"
#include "components/data_sharing/public/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/collaboration_finder.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/collaboration_id.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
std::unique_ptr<KeyedService> CreateTestSyncService(content::BrowserContext*) {
  return std::make_unique<syncer::TestSyncService>();
}
#endif

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
        {data_sharing::features::kDataSharingFeature}, {});
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

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_PromptDialogAccept DISABLED_PromptDialogAccept
#else
#define MAYBE_PromptDialogAccept PromptDialogAccept
#endif
IN_PROC_BROWSER_TEST_F(CollaborationControllerDelegateDesktopInteractiveUITest,
                       MAYBE_PromptDialogAccept) {
  // Show prompt dialog and accept it.
  collaboration::ServiceStatus status;
  TestCollaborationControllerDelegateDesktop delegate(browser());
  EXPECT_CALL(delegate, GetServiceStatus())
      .Times(2)
      .WillRepeatedly(testing::Return(status));
  EXPECT_EQ(nullptr, delegate.prompt_dialog_widget_for_testing());
  base::MockCallback<
      collaboration::CollaborationControllerDelegate::ResultCallback>
      callback;

  delegate.ShowAuthenticationUi(collaboration::FlowType::kJoin, callback.Get());
  views::Widget* dialog_widget = delegate.prompt_dialog_widget_for_testing();
  EXPECT_NE(nullptr, dialog_widget);

  // Accepting the dialog should trigger a sign-in flow. The callback should be
  // invoked with kSuccess.
  EXPECT_CALL(
      callback,
      Run(collaboration::CollaborationControllerDelegate::Outcome::kSuccess))
      .Times(1);

  views::test::AcceptDialog(dialog_widget);
  EXPECT_EQ(nullptr, delegate.prompt_dialog_widget_for_testing());
}

IN_PROC_BROWSER_TEST_F(CollaborationControllerDelegateDesktopInteractiveUITest,
                       PromptDialogCancel) {
  // Show prompt dialog and cancel it.
  collaboration::ServiceStatus status;
  TestCollaborationControllerDelegateDesktop delegate(browser());
  EXPECT_CALL(delegate, GetServiceStatus()).WillOnce(testing::Return(status));
  EXPECT_EQ(nullptr, delegate.prompt_dialog_widget_for_testing());
  base::MockCallback<
      collaboration::CollaborationControllerDelegate::ResultCallback>
      callback;

  delegate.ShowAuthenticationUi(collaboration::FlowType::kJoin, callback.Get());
  views::Widget* dialog_widget = delegate.prompt_dialog_widget_for_testing();
  EXPECT_NE(nullptr, dialog_widget);

  EXPECT_CALL(
      callback,
      Run(collaboration::CollaborationControllerDelegate::Outcome::kCancel))
      .Times(1);

  views::test::CancelDialog(dialog_widget);
  EXPECT_EQ(nullptr, delegate.prompt_dialog_widget_for_testing());
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
  RunTestSequence(
      Do([&]() {
        delegate.ShowJoinDialog(token, preview_data, callback.Get());
      }),
      WaitForShow(kDataSharingBubbleElementId),
      Do([&]() { DataSharingBubbleController::From(browser())->Close(); }));
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
        auto* controller = DataSharingBubbleController::From(browser());
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
        auto* controller = DataSharingBubbleController::From(browser());
        controller->Close();
      }));
}

IN_PROC_BROWSER_TEST_F(CollaborationControllerDelegateDesktopInteractiveUITest,
                       ShowManageDialog) {
  TestCollaborationControllerDelegateDesktop delegate(browser());

  // Add a saved tab group with fake_collab_id
  syncer::CollaborationId fake_collab_id("fake_collab_id");
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
  syncer::CollaborationId fake_collab_id("fake_collab_id");
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
  syncer::CollaborationId fake_collab_id("fake_collab_id");
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
  delegate.PromoteTabGroup(data_sharing::GroupId(fake_collab_id.value()),
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

IN_PROC_BROWSER_TEST_F(CollaborationControllerDelegateDesktopInteractiveUITest,
                       OnBrowserCloseWithOpenDialog) {
  Browser* browser2 = CreateBrowser(browser()->profile());

  // Show a prompt dialog.
  collaboration::ServiceStatus status;
  TestCollaborationControllerDelegateDesktop delegate(browser2);
  EXPECT_CALL(delegate, GetServiceStatus()).WillOnce(testing::Return(status));
  base::MockCallback<
      collaboration::CollaborationControllerDelegate::ResultCallback>
      callback;
  delegate.ShowAuthenticationUi(collaboration::FlowType::kJoin, callback.Get());
  ASSERT_NE(nullptr, delegate.prompt_dialog_widget_for_testing());
  ASSERT_FALSE(delegate.prompt_dialog_widget_for_testing()->IsClosed());

  // Pass in an exit callback to the delegate.
  base::MockCallback<base::OnceCallback<void()>> exit_callback;
  delegate.PrepareFlowUI(exit_callback.Get(), callback.Get());

  // Closing the browser should not crash and should invoke the exit callback.
  EXPECT_CALL(exit_callback, Run).Times(1);
  browser2->window()->Close();
}

IN_PROC_BROWSER_TEST_F(CollaborationControllerDelegateDesktopInteractiveUITest,
                       ShowErrorDialogAndOpenUpdateChromePage) {
  // Show error dialog and open update chrome page.
  TestCollaborationControllerDelegateDesktop delegate(browser());
  EXPECT_EQ(nullptr, delegate.error_dialog_widget_for_testing());
  base::MockCallback<
      collaboration::CollaborationControllerDelegate::ResultCallback>
      callback;
  delegate.ShowError(
      collaboration::CollaborationControllerDelegate::ErrorInfo(
          collaboration::CollaborationControllerDelegate::ErrorInfo::Type::
              kUpdateChromeUiForVersionOutOfDate,
          collaboration::FlowType::kJoin),
      callback.Get());
  EXPECT_NE(nullptr, delegate.error_dialog_widget_for_testing());

  // Click ok button and verify a new tab is opened.
  content::TestNavigationObserver nav_observer(GURL("chrome://settings/help"));
  nav_observer.StartWatchingNewWebContents();
  views::test::AcceptDialog(delegate.error_dialog_widget_for_testing());
  nav_observer.Wait();
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(
      GURL("chrome://settings/help"),
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
class
    CollaborationControllerDelegateDesktopInteractiveUITestWithHistorySyncOptIn
    : public InteractiveBrowserTest {
 public:
  CollaborationControllerDelegateDesktopInteractiveUITestWithHistorySyncOptIn() =
      default;

  CollaborationControllerDelegateDesktopInteractiveUITestWithHistorySyncOptIn(
      const CollaborationControllerDelegateDesktopInteractiveUITestWithHistorySyncOptIn&) =
      delete;
  CollaborationControllerDelegateDesktopInteractiveUITestWithHistorySyncOptIn&
  operator=(
      const CollaborationControllerDelegateDesktopInteractiveUITestWithHistorySyncOptIn&) =
      delete;

  ~CollaborationControllerDelegateDesktopInteractiveUITestWithHistorySyncOptIn()
      override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {data_sharing::features::kDataSharingFeature,
         syncer::kReplaceSyncPromosWithSignInPromos},
        {});
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    SetHistorySyncStatus(false);
  }

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    url_loader_factory_helper_.SetUp();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &CollaborationControllerDelegateDesktopInteractiveUITestWithHistorySyncOptIn::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        context, base::BindRepeating(&CreateTestSyncService));
  }

  syncer::TestSyncService* sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(browser()->GetProfile()));
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return url_loader_factory_helper_.test_url_loader_factory();
  }

  void SignIn() {
    signin::MakeAccountAvailable(
        identity_manager(),
        signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
            .WithCookie()
            .WithAccessPoint(
                signin_metrics::AccessPoint::kCollaborationJoinTabGroup)
            .AsPrimary(signin::ConsentLevel::kSignin)
            .Build("test@email.com"));
  }

  bool IsSignedIn() {
    return signin_util::GetSignedInState(identity_manager()) ==
           signin_util::SignedInState::kSignedIn;
  }

  void SetHistorySyncStatus(bool enabled) {
    sync_service()->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kHistory, enabled);
    sync_service()->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kTabs, enabled);
    sync_service()->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kSavedTabGroups, enabled);
  }

  void ShowAndAcceptDialog(
      collaboration::SigninStatus signin_status,
      collaboration::SyncStatus sync_status =
          collaboration::SyncStatus::kNotSyncing,
      collaboration::FlowType flow_type = collaboration::FlowType::kJoin) {
    // Set up an account picture in case it is shown in the dialog.
    signin::SimulateAccountImageFetch(
        identity_manager(),
        signin_ui_util::GetSingleAccountForPromos(identity_manager())
            .account_id,
        "https://avatar.com/avatar.png", gfx::test::CreateImage(/*size=*/32));

    // Show prompt dialog and accept it.
    TestCollaborationControllerDelegateDesktop delegate(browser());
    EXPECT_CALL(delegate, GetServiceStatus())
        .Times(2)
        .WillRepeatedly(testing::Return(collaboration::ServiceStatus{
            .signin_status = signin_status, .sync_status = sync_status}));
    EXPECT_EQ(nullptr, delegate.prompt_dialog_widget_for_testing());
    base::MockCallback<
        collaboration::CollaborationControllerDelegate::ResultCallback>
        callback;

    delegate.ShowAuthenticationUi(flow_type, callback.Get());
    views::Widget* dialog_widget = delegate.prompt_dialog_widget_for_testing();
    EXPECT_NE(nullptr, dialog_widget);

    // Accepting the dialog should trigger a sign-in flow. The callback should
    // be invoked with kSuccess.
    EXPECT_CALL(
        callback,
        Run(collaboration::CollaborationControllerDelegate::Outcome::kSuccess))
        .Times(1);

    views::test::AcceptDialog(dialog_widget);
    EXPECT_EQ(nullptr, delegate.prompt_dialog_widget_for_testing());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  ChromeSigninClientWithURLLoaderHelper url_loader_factory_helper_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(
    CollaborationControllerDelegateDesktopInteractiveUITestWithHistorySyncOptIn,
    PromptDialogAccept_SignedOut) {
  base::HistogramTester histogram_tester;

  ShowAndAcceptDialog(collaboration::SigninStatus::kNotSignedIn);

  EXPECT_TRUE(SigninPromoTabHelper::GetForWebContents(
                  *browser()->tab_strip_model()->GetActiveWebContents())
                  ->IsInitializedForTesting());

  EXPECT_FALSE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));
  EXPECT_FALSE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kTabs));
  EXPECT_FALSE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kSavedTabGroups));

  // Signing in should also enable history sync.
  SignIn();

  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));
  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kTabs));
  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kSavedTabGroups));

  // Signin metrics - Offered/Started/Completed are recorded, but no values for
  // WebSignin (WithDefault).
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::kCollaborationJoinTabGroup, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Offered.NewAccountNoExistingAccount",
      signin_metrics::AccessPoint::kCollaborationJoinTabGroup, 1);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Offered.WithDefault", 0);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Started",
      signin_metrics::AccessPoint::kCollaborationJoinTabGroup, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Completed",
      signin_metrics::AccessPoint::kCollaborationJoinTabGroup, 1);
  histogram_tester.ExpectTotalCount("Signin.WebSignin.SourceToChromeSignin", 0);

  histogram_tester.ExpectUniqueSample(
      "Signin.HistorySyncOptIn.Offered",
      signin_metrics::AccessPoint::kCollaborationJoinTabGroup,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(
    CollaborationControllerDelegateDesktopInteractiveUITestWithHistorySyncOptIn,
    PromptDialogAccept_WebSignedIn) {
  base::HistogramTester histogram_tester;

  AccountInfo info = signin::MakeAccountAvailable(
      identity_manager(),
      signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .WithCookie()
          .WithAccessPoint(signin_metrics::AccessPoint::kWebSignin)
          .Build("test@email.com"));

  // Accepting the dialog should sign the user in and also enable history sync.
  ShowAndAcceptDialog(collaboration::SigninStatus::kNotSignedIn);

  EXPECT_TRUE(IsSignedIn());
  EXPECT_FALSE(SigninPromoTabHelper::GetForWebContents(
                   *browser()->tab_strip_model()->GetActiveWebContents())
                   ->IsInitializedForTesting());

  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));
  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kTabs));
  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kSavedTabGroups));

  // Signin metrics - WebSignin (WithDefault) metrics are also recorded.
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::kCollaborationJoinTabGroup, 1);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Started", 0);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Completed",
      signin_metrics::AccessPoint::kCollaborationJoinTabGroup, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::kCollaborationJoinTabGroup, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Offered.WithDefault",
      signin_metrics::AccessPoint::kCollaborationJoinTabGroup, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.SignIn.Offered.NewAccountNoExistingAccount", 0);
  histogram_tester.ExpectBucketCount(
      "Signin.WebSignin.SourceToChromeSignin",
      signin_metrics::AccessPoint::kCollaborationJoinTabGroup, 1);

  histogram_tester.ExpectUniqueSample(
      "Signin.HistorySyncOptIn.Offered",
      signin_metrics::AccessPoint::kCollaborationJoinTabGroup,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(
    CollaborationControllerDelegateDesktopInteractiveUITestWithHistorySyncOptIn,
    PromptDialogAccept_SignInPending) {
  AccountInfo info = signin::MakePrimaryAccountAvailable(
      identity_manager(), "test@email.com", signin::ConsentLevel::kSignin);
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager());

  // Start recording metrics after signing in.
  base::HistogramTester histogram_tester;

  ShowAndAcceptDialog(collaboration::SigninStatus::kSignedInPaused);

  // History sync should be enabled immediately, before the reauth is
  // completed.
  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));
  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kTabs));
  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kSavedTabGroups));

  // A reauth tab is expected to be shown.
  content::WebContents* reauth_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(reauth_tab);
  DiceTabHelper* dice_tab_helper = DiceTabHelper::FromWebContents(reauth_tab);
  ASSERT_TRUE(dice_tab_helper);
  EXPECT_EQ(dice_tab_helper->signin_access_point(),
            signin_metrics::AccessPoint::kCollaborationJoinTabGroup);
  EXPECT_EQ(dice_tab_helper->signin_reason(),
            signin_metrics::Reason::kReauthentication);

  // Signin metrics - nothing should be recorded for reauth.
  histogram_tester.ExpectTotalCount("Signin.SignIn.Offered", 0);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Started", 0);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Completed", 0);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Offered.WithDefault", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.SignIn.Offered.NewAccountNoExistingAccount", 0);
  histogram_tester.ExpectTotalCount("Signin.WebSignin.SourceToChromeSignin", 0);

  histogram_tester.ExpectUniqueSample(
      "Signin.HistorySyncOptIn.Offered",
      signin_metrics::AccessPoint::kCollaborationJoinTabGroup,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(
    CollaborationControllerDelegateDesktopInteractiveUITestWithHistorySyncOptIn,
    PromptDialogAccept_SignInPendingWithHistorySyncAlreadyEnabled) {
  signin::MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                                      signin::ConsentLevel::kSignin);
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager());

  // Start recording metrics after signing in.
  base::HistogramTester histogram_tester;

  // Show and accept the dialog.
  ShowAndAcceptDialog(collaboration::SigninStatus::kSignedInPaused,
                      collaboration::SyncStatus::kSyncEnabled);

  // Since the type did not need to be enabled, we don't record that history
  // sync opt in was offered.
  histogram_tester.ExpectTotalCount("Signin.HistorySyncOptIn.Offered", 0);
}

IN_PROC_BROWSER_TEST_F(
    CollaborationControllerDelegateDesktopInteractiveUITestWithHistorySyncOptIn,
    PromptDialogAccept_SignedInWithoutHistorySync) {
  SignIn();

  // Start recording metrics after signing in.
  base::HistogramTester histogram_tester;

  // Accepting the dialog should enable history sync.
  ShowAndAcceptDialog(collaboration::SigninStatus::kSignedIn);

  EXPECT_TRUE(IsSignedIn());
  EXPECT_FALSE(SigninPromoTabHelper::GetForWebContents(
                   *browser()->tab_strip_model()->GetActiveWebContents())
                   ->IsInitializedForTesting());

  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));
  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kTabs));
  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kSavedTabGroups));

  // Signin metrics - nothing should be recorded for only history sync optin.
  histogram_tester.ExpectTotalCount("Signin.SignIn.Offered", 0);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Started", 0);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Completed", 0);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Offered.WithDefault", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.SignIn.Offered.NewAccountNoExistingAccount", 0);
  histogram_tester.ExpectTotalCount("Signin.WebSignin.SourceToChromeSignin", 0);

  histogram_tester.ExpectUniqueSample(
      "Signin.HistorySyncOptIn.Offered",
      signin_metrics::AccessPoint::kCollaborationJoinTabGroup,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(
    CollaborationControllerDelegateDesktopInteractiveUITestWithHistorySyncOptIn,
    PromptDialogAccept_SignInDisabled) {
  base::HistogramTester histogram_tester;

  // Show dialog and open the Google services settings page.
  ShowAndAcceptDialog(collaboration::SigninStatus::kSigninDisabled);

  // Verify the settings page was opened.
  EXPECT_EQ(
      GURL("chrome://settings/googleServices"),
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());

  // History sync was not enabled.
  EXPECT_FALSE(IsSignedIn());
  EXPECT_FALSE(SigninPromoTabHelper::GetForWebContents(
                   *browser()->tab_strip_model()->GetActiveWebContents())
                   ->IsInitializedForTesting());

  EXPECT_FALSE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));
  EXPECT_FALSE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kTabs));
  EXPECT_FALSE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kSavedTabGroups));

  // Signin metrics - nothing should be recorded for sync disabled.
  histogram_tester.ExpectTotalCount("Signin.SignIn.Offered", 0);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Started", 0);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Completed", 0);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Offered.WithDefault", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.SignIn.Offered.NewAccountNoExistingAccount", 0);
  histogram_tester.ExpectTotalCount("Signin.WebSignin.SourceToChromeSignin", 0);

  histogram_tester.ExpectUniqueSample(
      "Signin.HistorySyncOptIn.Offered",
      signin_metrics::AccessPoint::kCollaborationJoinTabGroup,
      /*expected_bucket_count=*/0);
}

IN_PROC_BROWSER_TEST_F(
    CollaborationControllerDelegateDesktopInteractiveUITestWithHistorySyncOptIn,
    PromptDialogAccept_CorrectAccessPointRecorded) {
  base::HistogramTester histogram_tester;

  // This should set the access point to the share tabs dialog.
  ShowAndAcceptDialog(collaboration::SigninStatus::kSignedIn,
                      collaboration::SyncStatus::kNotSyncing,
                      collaboration::FlowType::kShareOrManage);

  histogram_tester.ExpectUniqueSample(
      "Signin.HistorySyncOptIn.Offered",
      signin_metrics::AccessPoint::kCollaborationShareTabGroup,
      /*expected_bucket_count=*/1);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
