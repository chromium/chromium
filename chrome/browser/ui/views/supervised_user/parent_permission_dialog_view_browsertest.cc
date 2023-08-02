// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/supervised_user/parent_permission_dialog.h"

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_delegate_impl.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_test_delegate.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/supervised_user/parent_permission_dialog_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/common/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/result_catcher.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_auth_consumer.h"

// End to end test of ParentPermissionDialog that exercises the dialog's
// internal logic that orchestrates the parental permission process.
class ParentPermissionDialogViewTest
    : public SupportsTestDialog<MixinBasedInProcessBrowserTest>,
      public TestParentPermissionDialogViewObserver {
 public:
  // The next dialog action to take.
  enum class NextDialogAction {
    kCancel,
    kAccept,
  };

  ParentPermissionDialogViewTest()
      : TestParentPermissionDialogViewObserver(this) {
    // This UI is only used in V1 extensions approvals flow, so to test it V2
    // flow needs to be disabled.
    feature_list_.InitAndDisableFeature(
        supervised_user::kLocalExtensionApprovalsV2);
  }

  ParentPermissionDialogViewTest(const ParentPermissionDialogViewTest&) =
      delete;
  ParentPermissionDialogViewTest& operator=(
      const ParentPermissionDialogViewTest&) = delete;

  void OnParentPermissionDialogDone(ParentPermissionDialog::Result result) {
    result_ = result;
    std::move(on_dialog_done_closure_).Run();
  }

  // TestBrowserUi
  void ShowUi(const std::string& name) override {
    SkBitmap icon =
        *gfx::Image(extensions::util::GetDefaultExtensionIcon()).ToSkBitmap();
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Set up a RunLoop with a quit closure to block until
    // the dialog is shown, which is what this method is supposed
    // to ensure.
    base::RunLoop run_loop;
    dialog_shown_closure_ = run_loop.QuitClosure();

    // These use base::DoNothing because we aren't interested in the dialog's
    // results. Unlike the other non-TestBrowserUi tests, this test doesn't
    // block, because that interferes with the widget accounting done by
    // TestBrowserUi.
    if (name == "default") {
      parent_permission_dialog_ =
          ParentPermissionDialog::CreateParentPermissionDialog(
              browser()->profile(), contents->GetTopLevelNativeWindow(),
              gfx::ImageSkia::CreateFrom1xBitmap(icon), u"Test prompt message",
              base::DoNothing());
    } else if (name == "extension") {
      parent_permission_dialog_ =
          ParentPermissionDialog::CreateParentPermissionDialogForExtension(
              browser()->profile(), contents->GetTopLevelNativeWindow(),
              gfx::ImageSkia::CreateFrom1xBitmap(icon), test_extension(),
              base::DoNothing());
    }
    parent_permission_dialog_->ShowDialog();
    run_loop.Run();
  }

  // TestParentPermissionDialogViewObserver
  void OnTestParentPermissionDialogViewCreated(
      ParentPermissionDialogView* view) override {
    if (dialog_shown_closure_)
      std::move(dialog_shown_closure_).Run();

    view_ = view;
    view_->SetIdentityManagerForTesting(identity_test_env_->identity_manager());
    view_->SetRepromptAfterIncorrectCredential(false);

    if (next_dialog_action_) {
      switch (next_dialog_action_.value()) {
        case NextDialogAction::kCancel:
          view_->CancelDialog();
          break;
        case NextDialogAction::kAccept:
          view_->AcceptDialog();
          break;
      }
    }
  }

  void InitializeFamilyData() {
    // Set up the child user's custodians (AKA parents).
    ASSERT_TRUE(browser());
    supervised_user_test_util::AddCustodians(browser()->profile());

    // Set up the identity test environment, which provides fake
    // OAuth refresh tokens.
    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();
    identity_test_env_->MakeAccountAvailable(FakeGaiaMixin::kFakeUserEmail);
    identity_test_env_->SetPrimaryAccount(FakeGaiaMixin::kFakeUserEmail,
                                          signin::ConsentLevel::kSync);
    identity_test_env_->SetRefreshTokenForPrimaryAccount();
    identity_test_env_->SetAutomaticIssueOfAccessTokens(true);
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    logged_in_user_mixin_.LogInUser(/*issue_any_scope_token=*/true);

    supervised_user_test_util::
        SetSupervisedUserExtensionsMayRequestPermissionsPref(
            browser()->profile(), true);
    supervised_user_extensions_delegate_ =
        std::make_unique<extensions::SupervisedUserExtensionsDelegateImpl>(
            browser()->profile());

    if (browser()->profile()->IsChild())
      InitializeFamilyData();

    test_extension_ = extensions::ExtensionBuilder("test extension").Build();
    extension_service()->AddExtension(test_extension_.get());
    extension_service()->DisableExtension(
        test_extension_->id(),
        extensions::disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);
  }

  void TearDownOnMainThread() override {
    supervised_user_extensions_delegate_.reset();
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  void set_next_reauth_status(
      const GaiaAuthConsumer::ReAuthProofTokenStatus next_status) {
    logged_in_user_mixin_.GetFakeGaiaMixin()->fake_gaia()->SetNextReAuthStatus(
        next_status);
  }

  void set_next_dialog_action(NextDialogAction action) {
    next_dialog_action_ = action;
  }

  // This method will block until the next dialog completing action takes place,
  // so that the result can be checked.
  void ShowPrompt() {
    base::RunLoop run_loop;
    on_dialog_done_closure_ = run_loop.QuitClosure();
    ParentPermissionDialog::DoneCallback callback = base::BindOnce(
        &ParentPermissionDialogViewTest::OnParentPermissionDialogDone,
        base::Unretained(this));

    SkBitmap icon =
        *gfx::Image(extensions::util::GetDefaultExtensionIcon()).ToSkBitmap();
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    parent_permission_dialog_ =
        ParentPermissionDialog::CreateParentPermissionDialog(
            browser()->profile(), contents->GetTopLevelNativeWindow(),
            gfx::ImageSkia::CreateFrom1xBitmap(icon), u"Test prompt message",
            std::move(callback));
    parent_permission_dialog_->ShowDialog();
    run_loop.Run();
  }

  // This method will block until the next dialog action takes place, so that
  // the result can be checked.
  void ShowPromptForExtension() {
    base::RunLoop run_loop;
    on_dialog_done_closure_ = run_loop.QuitClosure();

    ParentPermissionDialog::DoneCallback callback = base::BindOnce(
        &ParentPermissionDialogViewTest::OnParentPermissionDialogDone,
        base::Unretained(this));

    SkBitmap icon =
        *gfx::Image(extensions::util::GetDefaultExtensionIcon()).ToSkBitmap();

    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    parent_permission_dialog_ =
        ParentPermissionDialog::CreateParentPermissionDialogForExtension(
            browser()->profile(), contents->GetTopLevelNativeWindow(),
            gfx::ImageSkia::CreateFrom1xBitmap(icon), test_extension(),
            std::move(callback));
    parent_permission_dialog_->ShowDialog();
    run_loop.Run();
  }

  void CheckResult(ParentPermissionDialog::Result expected) {
    EXPECT_EQ(result_, expected);
  }

  void CheckInvalidCredentialWasReceived() {
    EXPECT_TRUE(view_->GetInvalidCredentialReceived());
  }

 protected:
  const extensions::Extension* test_extension() {
    return test_extension_.get();
  }

  extensions::ExtensionRegistry* extension_registry() {
    return extensions::ExtensionRegistry::Get(browser()->profile());
  }

  extensions::ExtensionService* extension_service() {
    return extensions::ExtensionSystem::Get(browser()->profile())
        ->extension_service();
  }

  std::unique_ptr<extensions::SupervisedUserExtensionsDelegate>
      supervised_user_extensions_delegate_;

 private:
  base::test::ScopedFeatureList feature_list_;

  raw_ptr<ParentPermissionDialogView, ExperimentalAsh> view_ = nullptr;
  std::unique_ptr<ParentPermissionDialog> parent_permission_dialog_;
  ParentPermissionDialog::Result result_;

  // Emulate consumer ownership (create public owner key file, install
  // attributes file, etc) so Chrome doesn't need to do it. The current setup is
  // not sufficient to go through the ownership flow successfully and it's not
  // essential to the logic under test.
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};
  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_,
      // Simulate Gellerization / Adding Supervision to load extensions.
      content::IsPreTest() ? ash::LoggedInUserMixin::LogInType::kRegular
                           : ash::LoggedInUserMixin::LogInType::kChild,
      embedded_test_server(), this};

  // Closure that is triggered once the dialog is shown.
  base::OnceClosure dialog_shown_closure_;

  // Closure that is triggered once the dialog completes.
  base::OnceClosure on_dialog_done_closure_;

  scoped_refptr<const extensions::Extension> test_extension_;

  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  absl::optional<NextDialogAction> next_dialog_action_;
};

// Tests that a plain dialog widget is shown using the TestBrowserUi
// infrastructure.
IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

// Tests that the extension-parameterized dialog widget is shown using the
// TestBrowserUi infrastructure.
IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest, InvokeUi_extension) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest, PermissionReceived) {
  set_next_reauth_status(GaiaAuthConsumer::ReAuthProofTokenStatus::kSuccess);
  set_next_dialog_action(
      ParentPermissionDialogViewTest::NextDialogAction::kAccept);
  ShowPrompt();
  CheckResult(ParentPermissionDialog::Result::kParentPermissionReceived);
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest,
                       PermissionFailedInvalidPassword) {
  set_next_reauth_status(
      GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant);
  set_next_dialog_action(
      ParentPermissionDialogViewTest::NextDialogAction::kAccept);
  ShowPrompt();
  CheckInvalidCredentialWasReceived();
  CheckResult(ParentPermissionDialog::Result::kParentPermissionFailed);
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest,
                       PermissionDialogCanceled) {
  set_next_dialog_action(
      ParentPermissionDialogViewTest::NextDialogAction::kCancel);
  ShowPrompt();
  CheckResult(ParentPermissionDialog::Result::kParentPermissionCanceled);
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest,
                       PermissionReceivedForExtension) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  set_next_reauth_status(GaiaAuthConsumer::ReAuthProofTokenStatus::kSuccess);
  set_next_dialog_action(
      ParentPermissionDialogViewTest::NextDialogAction::kAccept);
  ShowPromptForExtension();
  CheckResult(ParentPermissionDialog::Result::kParentPermissionReceived);

  histogram_tester.ExpectBucketCount(SupervisedUserExtensionsMetricsRecorder::
                                         kParentPermissionDialogHistogramName,
                                     SupervisedUserExtensionsMetricsRecorder::
                                         ParentPermissionDialogState::kOpened,
                                     1);
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::
          kParentPermissionDialogHistogramName,
      SupervisedUserExtensionsMetricsRecorder::ParentPermissionDialogState::
          kParentApproved,
      1);
  // The total histogram count is 2 (one for kOpened and one for
  // kParentApproved).
  histogram_tester.ExpectTotalCount(SupervisedUserExtensionsMetricsRecorder::
                                        kParentPermissionDialogHistogramName,
                                    2);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   SupervisedUserExtensionsMetricsRecorder::
                       kParentPermissionDialogOpenedActionName));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   SupervisedUserExtensionsMetricsRecorder::
                       kParentPermissionDialogParentApprovedActionName));
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest,
                       PermissionFailedInvalidPasswordForExtension) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  set_next_reauth_status(
      GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant);
  set_next_dialog_action(
      ParentPermissionDialogViewTest::NextDialogAction::kAccept);
  ShowPromptForExtension();
  CheckInvalidCredentialWasReceived();
  CheckResult(ParentPermissionDialog::Result::kParentPermissionFailed);

  histogram_tester.ExpectBucketCount(SupervisedUserExtensionsMetricsRecorder::
                                         kParentPermissionDialogHistogramName,
                                     SupervisedUserExtensionsMetricsRecorder::
                                         ParentPermissionDialogState::kOpened,
                                     1);
  histogram_tester.ExpectBucketCount(SupervisedUserExtensionsMetricsRecorder::
                                         kParentPermissionDialogHistogramName,
                                     SupervisedUserExtensionsMetricsRecorder::
                                         ParentPermissionDialogState::kFailed,
                                     1);
  // The total histogram count is 2 (one for kOpened and one for kFailed).
  histogram_tester.ExpectTotalCount(SupervisedUserExtensionsMetricsRecorder::
                                        kParentPermissionDialogHistogramName,
                                    2);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   SupervisedUserExtensionsMetricsRecorder::
                       kParentPermissionDialogOpenedActionName));
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest,
                       PermissionDialogCanceledForExtension) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  set_next_dialog_action(
      ParentPermissionDialogViewTest::NextDialogAction::kCancel);

  ShowPromptForExtension();
  CheckResult(ParentPermissionDialog::Result::kParentPermissionCanceled);

  histogram_tester.ExpectBucketCount(SupervisedUserExtensionsMetricsRecorder::
                                         kParentPermissionDialogHistogramName,
                                     SupervisedUserExtensionsMetricsRecorder::
                                         ParentPermissionDialogState::kOpened,
                                     1);
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::
          kParentPermissionDialogHistogramName,
      SupervisedUserExtensionsMetricsRecorder::ParentPermissionDialogState::
          kParentCanceled,
      1);
  // The total histogram count is 2 (one for kOpened and one for
  // kParentCanceled).
  histogram_tester.ExpectTotalCount(SupervisedUserExtensionsMetricsRecorder::
                                        kParentPermissionDialogHistogramName,
                                    2);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   SupervisedUserExtensionsMetricsRecorder::
                       kParentPermissionDialogOpenedActionName));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   SupervisedUserExtensionsMetricsRecorder::
                       kParentPermissionDialogParentCanceledActionName));
}

using ExtensionEnableFlowTestSupervised = ParentPermissionDialogViewTest;

// Tests launching an app that requires parent approval from the launcher.
IN_PROC_BROWSER_TEST_F(ExtensionEnableFlowTestSupervised,
                       ParentPermissionDialogAccept) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(browser()->profile()->IsChild());

  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(
      test_extension()->id()));

  set_next_reauth_status(GaiaAuthConsumer::ReAuthProofTokenStatus::kSuccess);
  set_next_dialog_action(NextDialogAction::kAccept);

  ExtensionEnableFlowTestDelegate delegate;
  ExtensionEnableFlow enable_flow(browser()->profile(), test_extension()->id(),
                                  &delegate);
  enable_flow.Start();
  delegate.Wait();

  ASSERT_TRUE(delegate.result());
  EXPECT_EQ(ExtensionEnableFlowTestDelegate::FINISHED, *delegate.result());

  // The extension should be enabled now.
  EXPECT_TRUE(extension_registry()->enabled_extensions().Contains(
      test_extension()->id()));

  // Proof that the parent Permission Dialog launched.
  histogram_tester.ExpectBucketCount(SupervisedUserExtensionsMetricsRecorder::
                                         kParentPermissionDialogHistogramName,
                                     SupervisedUserExtensionsMetricsRecorder::
                                         ParentPermissionDialogState::kOpened,
                                     1);
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::
          kParentPermissionDialogHistogramName,
      SupervisedUserExtensionsMetricsRecorder::ParentPermissionDialogState::
          kParentApproved,
      1);
  // The total histogram count is 2 (one for kOpened and one for
  // kParentApproved).
  histogram_tester.ExpectTotalCount(SupervisedUserExtensionsMetricsRecorder::
                                        kParentPermissionDialogHistogramName,
                                    2);
}

// Tests launching an app and canceling parent approval from the launcher.
IN_PROC_BROWSER_TEST_F(ExtensionEnableFlowTestSupervised,
                       ParentPermissionDialogCancel) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(browser()->profile()->IsChild());

  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(
      test_extension()->id()));

  set_next_dialog_action(NextDialogAction::kCancel);

  ExtensionEnableFlowTestDelegate delegate;
  ExtensionEnableFlow enable_flow(browser()->profile(), test_extension()->id(),
                                  &delegate);
  enable_flow.Start();
  delegate.Wait();

  ASSERT_TRUE(delegate.result());
  EXPECT_EQ(ExtensionEnableFlowTestDelegate::ABORTED, *delegate.result());

  // The extension should remain disabled.
  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(
      test_extension()->id()));

  // Proof that the parent Permission Dialog launched.
  histogram_tester.ExpectBucketCount(SupervisedUserExtensionsMetricsRecorder::
                                         kParentPermissionDialogHistogramName,
                                     SupervisedUserExtensionsMetricsRecorder::
                                         ParentPermissionDialogState::kOpened,
                                     1);
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::
          kParentPermissionDialogHistogramName,
      SupervisedUserExtensionsMetricsRecorder::ParentPermissionDialogState::
          kParentCanceled,
      1);
  // The total histogram count is 2 (one for kOpened and one for
  // kParentCanceled).
  histogram_tester.ExpectTotalCount(SupervisedUserExtensionsMetricsRecorder::
                                        kParentPermissionDialogHistogramName,
                                    2);
}

// Tests that the Parent Permission Dialog doesn't appear at all when the parent
// has disabled the "Permissions for sites, apps and extensions" toggle, and the
// supervised user sees the Extension Install Blocked By Parent error dialog
// instead.
IN_PROC_BROWSER_TEST_F(ExtensionEnableFlowTestSupervised,
                       ParentBlockedExtensionEnable) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(browser()->profile()->IsChild());

  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(
      test_extension()->id()));

  // Simulate the parent disabling the "Permissions for sites, apps and
  // extensions" toggle.
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(browser()->profile(),
                                                           false);

  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);

  ExtensionEnableFlowTestDelegate delegate;
  ExtensionEnableFlow enable_flow(browser()->profile(), test_extension()->id(),
                                  &delegate);
  enable_flow.Start();
  delegate.Wait();

  ASSERT_TRUE(delegate.result());
  EXPECT_EQ(ExtensionEnableFlowTestDelegate::ABORTED, *delegate.result());

  // The extension should remain disabled.
  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(
      test_extension()->id()));

  // Proof that the Parent Permission Dialog didn't launch.
  histogram_tester.ExpectTotalCount(SupervisedUserExtensionsMetricsRecorder::
                                        kParentPermissionDialogHistogramName,
                                    0);

  // Proof that the Extension Install Blocked By Parent Dialog launched.
  histogram_tester.ExpectUniqueSample(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName,
      SupervisedUserExtensionsMetricsRecorder::EnablementState::kFailedToEnable,
      1);
}

class ExtensionManagementApiTestSupervised
    : public ParentPermissionDialogViewTest {
 public:
  void SetUpOnMainThread() override {
    ParentPermissionDialogViewTest::SetUpOnMainThread();
    // Loads the extension as a regular user and then simulates Gellerization /
    // Adding Supervision since supervised users can't load extensions directly.
    if (content::IsPreTest()) {
      LoadNamedExtension("disabled_extension");
      LoadNamedExtension("test");
    } else {
      // In addition to the two extensions from the PRE test, there's one more
      // test extension from the ParentPermissionDialogViewTest parent class.
      EXPECT_EQ(3u, extension_registry()->disabled_extensions().size());
      scoped_refptr<const extensions::Extension> test_extension;
      for (const auto& e : extension_registry()->disabled_extensions()) {
        if (e->name() == "disabled_extension") {
          disabled_extension_id_ = e->id();
        } else if (e->name() == "Extension Management API Test") {
          CHECK(test_extension_id_.empty());
          test_extension_id_ = e->id();
          test_extension = e;
        }
      }
      EXPECT_FALSE(disabled_extension_id_.empty());
      EXPECT_FALSE(test_extension_id_.empty());
      // Approve the extension for running the test.
      supervised_user_extensions_delegate_->AddExtensionApproval(
          *test_extension);
    }
  }

 protected:
  void LoadNamedExtension(const std::string& name) {
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    test_data_dir = test_data_dir.AppendASCII("extensions");
    test_data_dir = test_data_dir.AppendASCII("api_test");
    extensions::ChromeTestExtensionLoader loader(browser()->profile());
    base::FilePath basedir = test_data_dir.AppendASCII("management");
    scoped_refptr<const extensions::Extension> extension =
        loader.LoadExtension(basedir.AppendASCII(name));
    ASSERT_TRUE(extension);
  }

  bool RunManagementSubtest(const std::string& page_url,
                            std::string* error_message) {
    DCHECK(!test_extension_id_.empty()) << "test_extension_id_ is required";
    DCHECK(!page_url.empty()) << "Argument page_url is required.";

    const extensions::Extension* test_extension =
        extension_registry()->enabled_extensions().GetByID(test_extension_id_);
    DCHECK(test_extension) << "Test extension is not enabled";

    extensions::ResultCatcher catcher;
    GURL url = test_extension->GetResourceURL(page_url);
    DCHECK(url.is_valid());
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    if (catcher.GetNextResult())
      return true;
    if (error_message)
      *error_message = catcher.message();
    return false;
  }

  std::string disabled_extension_id_;
  std::string test_extension_id_;
};

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTestSupervised,
                       PRE_ParentPermissionGrantedForEnable) {
  ASSERT_FALSE(browser()->profile()->IsChild());
}

// Tests launching the Parent Permission Dialog from the management api when the
// extension hasn't already been approved.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTestSupervised,
                       ParentPermissionGrantedForEnable) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(browser()->profile()->IsChild());

  set_next_reauth_status(GaiaAuthConsumer::ReAuthProofTokenStatus::kSuccess);
  set_next_dialog_action(NextDialogAction::kAccept);

  std::string error;
  EXPECT_TRUE(RunManagementSubtest(
      "supervised_user_permission_granted_for_enable.html", &error))
      << error;

  // The extension should be enabled now.
  EXPECT_TRUE(extension_registry()->enabled_extensions().Contains(
      disabled_extension_id_));

  // Proof that the Parent Permission Dialog launched.
  histogram_tester.ExpectBucketCount(SupervisedUserExtensionsMetricsRecorder::
                                         kParentPermissionDialogHistogramName,
                                     SupervisedUserExtensionsMetricsRecorder::
                                         ParentPermissionDialogState::kOpened,
                                     1);
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::
          kParentPermissionDialogHistogramName,
      SupervisedUserExtensionsMetricsRecorder::ParentPermissionDialogState::
          kParentApproved,
      1);
  // The total histogram count is 2 (one for kOpened and one for
  // kParentApproved).
  histogram_tester.ExpectTotalCount(SupervisedUserExtensionsMetricsRecorder::
                                        kParentPermissionDialogHistogramName,
                                    2);
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTestSupervised,
                       PRE_ParentPermissionNotGrantedForEnable) {
  ASSERT_FALSE(browser()->profile()->IsChild());
}

// Tests that extensions are not enabled after the parent permission dialog is
// cancelled.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTestSupervised,
                       ParentPermissionNotGrantedForEnable) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(browser()->profile()->IsChild());

  set_next_dialog_action(NextDialogAction::kCancel);

  std::string error;
  EXPECT_TRUE(RunManagementSubtest(
      "supervised_user_permission_not_granted_for_enable.html", &error))
      << error;

  // The extension should still be disabled.
  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(
      disabled_extension_id_));

  // Proof that the Parent Permission Dialog launched.
  histogram_tester.ExpectBucketCount(SupervisedUserExtensionsMetricsRecorder::
                                         kParentPermissionDialogHistogramName,
                                     SupervisedUserExtensionsMetricsRecorder::
                                         ParentPermissionDialogState::kOpened,
                                     1);
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::
          kParentPermissionDialogHistogramName,
      SupervisedUserExtensionsMetricsRecorder::ParentPermissionDialogState::
          kParentCanceled,
      1);
  // The total histogram count is 2 (one for kOpened and one for
  // kParentCanceled).
  histogram_tester.ExpectTotalCount(SupervisedUserExtensionsMetricsRecorder::
                                        kParentPermissionDialogHistogramName,
                                    2);
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTestSupervised,
                       PRE_ParentBlockedExtensionEnable) {
  ASSERT_FALSE(browser()->profile()->IsChild());
}

// Tests that the Parent Permission Dialog doesn't appear at all when the parent
// has disabled the "Permissions for sites, apps and extensions" toggle, and the
// supervised user sees the Extension Install Blocked By Parent error dialog
// instead.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTestSupervised,
                       ParentBlockedExtensionEnable) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(browser()->profile()->IsChild());

  // Simulate the parent disabling the "Permissions for sites, apps and
  // extensions" toggle.
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(browser()->profile(),
                                                           false);

  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);

  std::string error;
  EXPECT_TRUE(RunManagementSubtest(
      "supervised_user_parent_disabled_permission_for_enable.html", &error))
      << error;

  // The extension should still be disabled.
  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(
      disabled_extension_id_));

  // Proof that the Parent Permission Dialog didn't launch.
  histogram_tester.ExpectTotalCount(SupervisedUserExtensionsMetricsRecorder::
                                        kParentPermissionDialogHistogramName,
                                    0);

  // Proof that the Extension Install Blocked By Parent Dialog launched instead.
  histogram_tester.ExpectUniqueSample(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName,
      SupervisedUserExtensionsMetricsRecorder::EnablementState::kFailedToEnable,
      1);
}
