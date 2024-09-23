// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/scoped_test_mv2_enabler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_delegate_impl.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_test_delegate.h"
#include "chrome/browser/ui/supervised_user/parent_permission_dialog.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/supervised_user/parent_permission_dialog_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
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

// End to end test of ExtensionEnableFlowTestSupervised that exercises the
// dialog's internal logic that orchestrates the parental permission process.
class ExtensionEnableFlowTestSupervised
    : public MixinBasedInProcessBrowserTest,
      public TestParentPermissionDialogViewObserver {
 public:
  // The next dialog action to take.
  enum class NextDialogAction {
    kCancel,
    kAccept,
  };

  ExtensionEnableFlowTestSupervised()
      : TestParentPermissionDialogViewObserver(this) {
    feature_list_.InitWithFeatures(
        // Enable extensions for supervised users in Desktop platforms.
        /*enabled_features=*/
        {supervised_user::
             kEnableExtensionsPermissionsForSupervisedUsersOnDesktop},
        /*disabled_features=*/{});
  }

  ExtensionEnableFlowTestSupervised(const ExtensionEnableFlowTestSupervised&) =
      delete;
  ExtensionEnableFlowTestSupervised& operator=(
      const ExtensionEnableFlowTestSupervised&) = delete;
  ~ExtensionEnableFlowTestSupervised() override { feature_list_.Reset(); }

  void OnParentPermissionDialogDone(ParentPermissionDialog::Result result) {
    result_ = result;
    std::move(on_dialog_done_closure_).Run();
  }

  // TestParentPermissionDialogViewObserver
  void OnTestParentPermissionDialogViewCreated(
      ParentPermissionDialogView* view) override {
    if (dialog_shown_closure_) {
      std::move(dialog_shown_closure_).Run();
    }

    view_ = view;
    view_->SetIdentityManagerForTesting(
        supervision_mixin_.GetIdentityTestEnvironment()->identity_manager());
    view_->SetRepromptAfterIncorrectCredential(false);

    if (!next_dialog_action_.has_value()) {
      return;
    }
    if (next_dialog_action_) {
      switch (next_dialog_action_.value()) {
        case NextDialogAction::kCancel:
          view_->CancelDialog();
          break;
        case NextDialogAction::kAccept:
          view_->AcceptDialog();
          break;
        default:
          NOTREACHED();
      }
    }
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();

    supervised_user_test_util::
        SetSupervisedUserExtensionsMayRequestPermissionsPref(
            browser()->profile(), true);
    supervised_user_extensions_delegate_ =
        std::make_unique<extensions::SupervisedUserExtensionsDelegateImpl>(
            browser()->profile());

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
    supervision_mixin_.SetNextReAuthStatus(next_status);
  }

  void set_next_dialog_action(NextDialogAction action) {
    next_dialog_action_ = action;
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
  raw_ptr<ParentPermissionDialogView, DanglingUntriaged> view_ = nullptr;
  std::unique_ptr<ParentPermissionDialog> parent_permission_dialog_;
  ParentPermissionDialog::Result result_;

  supervised_user::SupervisionMixin supervision_mixin_{
      mixin_host_,
      this,
      embedded_test_server(),
      {.consent_level = signin::ConsentLevel::kSync,
       .sign_in_mode =
           content::IsPreTest()
               ? supervised_user::SupervisionMixin::SignInMode::kRegular
               : supervised_user::SupervisionMixin::SignInMode::kSupervised}};

  // Closure that is triggered once the dialog is shown.
  base::OnceClosure dialog_shown_closure_;

  // Closure that is triggered once the dialog completes.
  base::OnceClosure on_dialog_done_closure_;

  scoped_refptr<const extensions::Extension> test_extension_;

  std::optional<NextDialogAction> next_dialog_action_;

  // TODO(https://crbug.com/40804030): Remove when these tests use only MV3
  // extensions.
  extensions::ScopedTestMV2Enabler mv2_enabler_;
};

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

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  enable_flow.StartForWebContents(web_contents);
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
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  enable_flow.StartForWebContents(web_contents);
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
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  enable_flow.StartForWebContents(web_contents);
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
    : public ExtensionEnableFlowTestSupervised {
 public:
  void SetUpOnMainThread() override {
    ExtensionEnableFlowTestSupervised::SetUpOnMainThread();
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
    if (catcher.GetNextResult()) {
      return true;
    }
    if (error_message) {
      *error_message = catcher.message();
    }
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
