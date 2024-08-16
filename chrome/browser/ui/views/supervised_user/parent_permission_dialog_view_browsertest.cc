// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/supervised_user/parent_permission_dialog_view.h"

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_delegate_impl.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/supervised_user/parent_permission_dialog.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
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
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/window/dialog_client_view.h"

// Must be in the same namespace as the target type (the global namespace).
// Makes test output more readable.
// http://google.github.io/googletest/advanced.html#teaching-googletest-how-to-print-your-values
std::ostream& operator<<(std::ostream& os,
                         ParentPermissionDialog::Result result) {
  switch (result) {
    case ParentPermissionDialog::Result::kParentPermissionReceived:
      os << "kParentPermissionReceived";
      return os;
    case ParentPermissionDialog::Result::kParentPermissionCanceled:
      os << "kParentPermissionCanceled";
      return os;
    case ParentPermissionDialog::Result::kParentPermissionFailed:
      os << "kParentPermissionFailed";
      return os;
    default:
      NOTREACHED();
  }
}

namespace {

enum class ActionStatus { kWasPerformed, kWasNotPerformed };

// Extracts the `name` argument for ShowUi() from the current test case name.
// E.g. for InvokeUi_name (or DISABLED_InvokeUi_name) returns "name".
std::string NameFromTestCase() {
  const std::string name = base::TestNameWithoutDisabledPrefix(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  size_t underscore = name.find('_');
  return underscore == std::string::npos ? std::string()
                                         : name.substr(underscore + 1);
}

// Brings in the view under test and captures it. Does not implement any test
// logic.
class ParentPermissionDialogViewHarness
    : public TestParentPermissionDialogViewObserver {
 public:
  explicit ParentPermissionDialogViewHarness(
      supervised_user::SupervisionMixin& supervision_mixin)
      : TestParentPermissionDialogViewObserver(this),
        supervision_mixin_(supervision_mixin) {}
  ~ParentPermissionDialogViewHarness() = default;

  ParentPermissionDialog::Result GetResult() {
    CHECK(result_.has_value())
        << "Use only after the dialog was set to be interacted with.";
    return *result_;
  }

  // T is either std::u16string for regular dialogs, or const
  // extensions::Extension*  for extension dialogs.
  template <typename T>
  void ShowUi(T dialog_input, Browser* browser) {
    gfx::ImageSkia icon = gfx::ImageSkia::CreateFrom1xBitmap(
        *gfx::Image(extensions::util::GetDefaultExtensionIcon()).ToSkBitmap());
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();

    dialog_ = CreatePermissionDialog(
        dialog_input, browser, contents, icon, extension_approval_entry_point_,
        base::BindOnce(
            &ParentPermissionDialogViewHarness::OnParentPermissionDialogDone,
            base::Unretained(this)));
    dialog_->ShowDialog();
  }

  bool InvalidCredentialWasReceived() {
    CHECK(under_test_) << "No permission view intercepted.";
    return under_test_->GetInvalidCredentialReceived();
  }

  void SetRepromptAfterIncorrectCredential(
      bool reprompt_on_incorrect_password) {
    reprompt_on_incorrect_password_ = reprompt_on_incorrect_password;
  }

  void SetExtensionApprovalEntryPoint(
      SupervisedUserExtensionParentApprovalEntryPoint
          extension_approval_entry_point) {
    extension_approval_entry_point_ = extension_approval_entry_point;
  }

 protected:
  template <typename T>
  std::unique_ptr<ParentPermissionDialog> CreatePermissionDialog(
      T dialog_input,
      Browser* browser,
      content::WebContents* contents,
      gfx::ImageSkia icon,
      std::optional<SupervisedUserExtensionParentApprovalEntryPoint>
          extension_approval_entry_point,
      ParentPermissionDialog::DoneCallback done_callback);

  template <>
  std::unique_ptr<ParentPermissionDialog> CreatePermissionDialog(
      std::u16string dialog_input,
      Browser* browser,
      content::WebContents* contents,
      gfx::ImageSkia icon,
      std::optional<SupervisedUserExtensionParentApprovalEntryPoint>
          extension_approval_entry_point,
      ParentPermissionDialog::DoneCallback done_callback) {
    return ParentPermissionDialog::CreateParentPermissionDialog(
        browser->profile(), contents->GetTopLevelNativeWindow(), icon,
        dialog_input, std::move(done_callback));
  }

  template <>
  std::unique_ptr<ParentPermissionDialog> CreatePermissionDialog(
      const extensions::Extension* dialog_input,
      Browser* browser,
      content::WebContents* contents,
      gfx::ImageSkia icon,
      std::optional<SupervisedUserExtensionParentApprovalEntryPoint>
          extension_approval_entry_point,
      ParentPermissionDialog::DoneCallback done_callback) {
    return ParentPermissionDialog::CreateParentPermissionDialogForExtension(
        browser->profile(), contents->GetTopLevelNativeWindow(), icon,
        dialog_input, extension_approval_entry_point.value(),
        std::move(done_callback));
  }

 private:
  void OnParentPermissionDialogDone(ParentPermissionDialog::Result result) {
    result_ = result;
  }

  // TestParentPermissionDialogViewObserver implementation.
  // Configures the identity manager of the view and stores
  // reference to the view under test.
  void OnTestParentPermissionDialogViewCreated(
      ParentPermissionDialogView* view) override {
    under_test_ = view;
    under_test_->SetIdentityManagerForTesting(
        supervision_mixin_->GetIdentityTestEnvironment()->identity_manager());
    under_test_->SetRepromptAfterIncorrectCredential(
        reprompt_on_incorrect_password_);
  }

  bool reprompt_on_incorrect_password_ = false;

  // Provides identity manager to the view.
  raw_ref<supervised_user::SupervisionMixin> supervision_mixin_;

  // `under_test_` is intercepted by OnTestParentPermissionDialogViewCreated.
  raw_ptr<ParentPermissionDialogView, DisableDanglingPtrDetection> under_test_;

  // The Dialog widget containing the view under test.
  // The test does not interact directly with this object
  // but it needs to be alive for the duration of the test.
  std::unique_ptr<ParentPermissionDialog> dialog_;

  // Optional result, if dialog was interacted.
  std::optional<ParentPermissionDialog::Result> result_;

  SupervisedUserExtensionParentApprovalEntryPoint
      extension_approval_entry_point_ =
          SupervisedUserExtensionParentApprovalEntryPoint::kMaxValue;
};

// End to end test of ParentPermissionDialog that exercises the dialog's
// internal logic that orchestrates the parental permission process.
class ParentPermissionDialogViewTest
    : public SupportsTestDialog<
          InteractiveBrowserTestT<MixinBasedInProcessBrowserTest>> {
 protected:
  void ShowUi(const std::string& name) override {
    if (name.find("default") != std::string::npos) {
      harness_.ShowUi(std::u16string(u"Test prompt message"), browser());
      return;
    } else if (name.find("extension") != std::string::npos) {
      harness_.ShowUi(test_extension_.get(), browser());
      return;
    } else {
      NOTREACHED() << "Check the suffix of the test name.";
    }
  }

  void SetUpOnMainThread() override {
    // Default ::SetUpOnMainThread() of all dependent mixins are invoked here.
    InteractiveBrowserTestT::SetUpOnMainThread();

    supervised_user_test_util::
        SetSupervisedUserExtensionsMayRequestPermissionsPref(
            browser()->profile(), /*enabled=*/true);

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
    InteractiveBrowserTestT::TearDownOnMainThread();
  }

  const extensions::Extension* test_extension() {
    return test_extension_.get();
  }

  extensions::ExtensionService* extension_service() {
    return extensions::ExtensionSystem::Get(browser()->profile())
        ->extension_service();
  }

  InteractiveTestApi::StepBuilder ShowDialog() {
    return Do([this]() -> void { ShowUi(NameFromTestCase()); });
  }

  auto CheckHistogramBucketCount(
      std::string_view histogram_name,
      SupervisedUserExtensionsMetricsRecorder::ParentPermissionDialogState
          state_bucket,
      int expected_count) {
    return Do([this, histogram_name, state_bucket, expected_count]() -> void {
      histogram_tester_.ExpectBucketCount(histogram_name, state_bucket,
                                          expected_count);
    });
  }

  auto CheckHistogramBucketCount(
      std::string_view histogram_name,
      SupervisedUserExtensionParentApprovalEntryPoint entry_point_bucket,
      int expected_count) {
    return Do(
        [this, histogram_name, entry_point_bucket, expected_count]() -> void {
          histogram_tester_.ExpectBucketCount(
              histogram_name, entry_point_bucket, expected_count);
        });
  }

  auto CheckHistogramTotalCount(std::string_view histogram_name,
                                int expected_count) {
    return Do([this, histogram_name, expected_count]() -> void {
      histogram_tester_.ExpectTotalCount(histogram_name, expected_count);
    });
  }

  auto GetActionStatus(std::string_view action_name) {
    return [this, action_name]() -> ActionStatus {
      return (user_action_tester_.GetActionCount(action_name) == 1)
                 ? ActionStatus::kWasPerformed
                 : ActionStatus::kWasNotPerformed;
    };
  }

  std::unique_ptr<extensions::SupervisedUserExtensionsDelegate>
      supervised_user_extensions_delegate_;

  supervised_user::SupervisionMixin supervision_mixin_{
      mixin_host_,
      this,
      embedded_test_server(),
      {.consent_level = signin::ConsentLevel::kSync,
       .sign_in_mode =
           content::IsPreTest()
               ? supervised_user::SupervisionMixin::SignInMode::kRegular
               : supervised_user::SupervisionMixin::SignInMode::kSupervised}};

  ParentPermissionDialogViewHarness harness_{supervision_mixin_};

 private:
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
  scoped_refptr<const extensions::Extension> test_extension_;
};

// Tests that a plain dialog widget is shown using the TestBrowserUi
// infrastructure.
IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

// Tests that a plain dialog widget is shown using the TestBrowserUi
// infrastructure.
IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest, InvokeUi_extension) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest,
                       PermissionReceived_default) {
  RunTestSequence(InAnyContext(Steps(
      ShowDialog(),
      WaitForShow(ParentPermissionDialog::kDialogViewIdForTesting),
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForHide(ParentPermissionDialog::kDialogViewIdForTesting),
      CheckResult([this]() { return harness_.GetResult(); },
                  ParentPermissionDialog::Result::kParentPermissionReceived))));
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest,
                       PermissionFailedInvalidPassword_default) {
  supervision_mixin_.SetNextReAuthStatus(
      GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant);

  RunTestSequence(InAnyContext(Steps(
      ShowDialog(),
      WaitForShow(ParentPermissionDialog::kDialogViewIdForTesting),
      PressButton(views::DialogClientView::kOkButtonElementId),
      // Closing the dialog results in the freeing of resources, so checks must
      // be done together without waiting for a fresh call stack.
      WithoutDelay(Steps(
          WaitForHide(ParentPermissionDialog::kDialogViewIdForTesting),
          CheckResult(
              [this]() { return harness_.InvalidCredentialWasReceived(); },
              true),
          CheckResult(
              [this]() { return harness_.GetResult(); },
              ParentPermissionDialog::Result::kParentPermissionFailed))))));
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest,
                       PermissionDialogCanceled_default) {
  RunTestSequence(InAnyContext(Steps(
      ShowDialog(),
      WaitForShow(ParentPermissionDialog::kDialogViewIdForTesting),
      PressButton(views::DialogClientView::kCancelButtonElementId),
      WaitForHide(ParentPermissionDialog::kDialogViewIdForTesting),
      CheckResult([this]() { return harness_.GetResult(); },
                  ParentPermissionDialog::Result::kParentPermissionCanceled))));
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest,
                       PermissionReceived_extension) {
  // Provide an extension dialog entry point to test the recorded histograms.
  harness_.SetExtensionApprovalEntryPoint(
      SupervisedUserExtensionParentApprovalEntryPoint::kOnWebstoreInstallation);
  supervision_mixin_.SetNextReAuthStatus(
      GaiaAuthConsumer::ReAuthProofTokenStatus::kSuccess);

  RunTestSequence(InAnyContext(Steps(
      ShowDialog(),
      WaitForShow(ParentPermissionDialog::kDialogViewIdForTesting),
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForHide(ParentPermissionDialog::kDialogViewIdForTesting),
      CheckResult([this]() { return harness_.GetResult(); },
                  ParentPermissionDialog::Result::kParentPermissionReceived),
      CheckHistogramBucketCount(SupervisedUserExtensionsMetricsRecorder::
                                    kParentPermissionDialogHistogramName,
                                SupervisedUserExtensionsMetricsRecorder::
                                    ParentPermissionDialogState::kOpened,
                                1),
      CheckHistogramBucketCount(
          SupervisedUserExtensionsMetricsRecorder::
              kParentPermissionDialogHistogramName,
          SupervisedUserExtensionsMetricsRecorder::ParentPermissionDialogState::
              kParentApproved,
          1),
      // The total histogram count is 2 (one for kOpened and one for
      // kParentApproved).
      CheckHistogramTotalCount(SupervisedUserExtensionsMetricsRecorder::
                                   kParentPermissionDialogHistogramName,
                               2),
      CheckResult(GetActionStatus(SupervisedUserExtensionsMetricsRecorder::
                                      kParentPermissionDialogOpenedActionName),
                  ActionStatus::kWasPerformed),
      // The provided entry point for the parent approval dialog has been
      // recorded.
      CheckHistogramBucketCount(
          SupervisedUserExtensionsMetricsRecorder::
              kExtensionParentApprovalEntryPointHistogramName,
          SupervisedUserExtensionParentApprovalEntryPoint::
              kOnWebstoreInstallation,
          1),
      CheckResult(
          GetActionStatus(SupervisedUserExtensionsMetricsRecorder::
                              kParentPermissionDialogParentApprovedActionName),
          ActionStatus::kWasPerformed))));
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest,
                       PermissionFailedInvalidPassword_extension) {
  // Provide an extension dialog entry point to test the recorded histograms.
  harness_.SetExtensionApprovalEntryPoint(
      SupervisedUserExtensionParentApprovalEntryPoint::
          kOnExtensionManagementSetEnabledOperation);
  supervision_mixin_.SetNextReAuthStatus(
      GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant);

  RunTestSequence(InAnyContext(Steps(
      ShowDialog(),
      WaitForShow(ParentPermissionDialog::kDialogViewIdForTesting),
      PressButton(views::DialogClientView::kOkButtonElementId),
      // Closing the dialog results in the freeing of resources, so checks must
      // be done together without waiting for a fresh call stack.
      WithoutDelay(Steps(
          WaitForHide(ParentPermissionDialog::kDialogViewIdForTesting),
          CheckResult(
              [this]() { return harness_.InvalidCredentialWasReceived(); },
              true),
          CheckResult([this]() { return harness_.GetResult(); },
                      ParentPermissionDialog::Result::kParentPermissionFailed),
          CheckHistogramBucketCount(SupervisedUserExtensionsMetricsRecorder::
                                        kParentPermissionDialogHistogramName,
                                    SupervisedUserExtensionsMetricsRecorder::
                                        ParentPermissionDialogState::kOpened,
                                    1),
          CheckHistogramBucketCount(
              SupervisedUserExtensionsMetricsRecorder::
                  kParentPermissionDialogHistogramName,
              SupervisedUserExtensionsMetricsRecorder::
                  ParentPermissionDialogState::kIncorrectParentPasswordProvided,
              1),
          CheckHistogramBucketCount(SupervisedUserExtensionsMetricsRecorder::
                                        kParentPermissionDialogHistogramName,
                                    SupervisedUserExtensionsMetricsRecorder::
                                        ParentPermissionDialogState::kFailed,
                                    1),
          // The total histogram count is 3 (one for kOpened, one for
          // kIncorrectPassword and one for kFailed).
          CheckHistogramTotalCount(SupervisedUserExtensionsMetricsRecorder::
                                       kParentPermissionDialogHistogramName,
                                   3),
          // The provided entry point for the parent approval dialog has been
          // recorded.
          CheckHistogramBucketCount(
              SupervisedUserExtensionsMetricsRecorder::
                  kExtensionParentApprovalEntryPointHistogramName,
              SupervisedUserExtensionParentApprovalEntryPoint::
                  kOnExtensionManagementSetEnabledOperation,
              1),
          CheckResult(
              GetActionStatus(SupervisedUserExtensionsMetricsRecorder::
                                  kParentPermissionDialogOpenedActionName),
              ActionStatus::kWasPerformed))))));
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest,
                       PermissionFailedInvalidPasswordWithRepromt_extension) {
  harness_.SetRepromptAfterIncorrectCredential(true);
  supervision_mixin_.SetNextReAuthStatus(
      GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant);

  RunTestSequence(InAnyContext(Steps(
      ShowDialog(),
      WaitForShow(ParentPermissionDialog::kDialogViewIdForTesting),
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForShow(ParentPermissionDialog::kIncorrectParentPasswordIdForTesting),
      CheckResult([this]() { return harness_.InvalidCredentialWasReceived(); },
                  true),
      CheckHistogramBucketCount(SupervisedUserExtensionsMetricsRecorder::
                                    kParentPermissionDialogHistogramName,
                                SupervisedUserExtensionsMetricsRecorder::
                                    ParentPermissionDialogState::kOpened,
                                1),
      CheckHistogramBucketCount(
          SupervisedUserExtensionsMetricsRecorder::
              kParentPermissionDialogHistogramName,
          SupervisedUserExtensionsMetricsRecorder::ParentPermissionDialogState::
              kIncorrectParentPasswordProvided,
          1),
      // The dialog remains open waiting for the correct password and has not
      // failed.
      EnsurePresent(ParentPermissionDialog::kDialogViewIdForTesting),
      CheckHistogramBucketCount(SupervisedUserExtensionsMetricsRecorder::
                                    kParentPermissionDialogHistogramName,
                                SupervisedUserExtensionsMetricsRecorder::
                                    ParentPermissionDialogState::kFailed,
                                0))));
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest,
                       PermissionDialogCanceled_extension) {
  RunTestSequence(InAnyContext(Steps(
      ShowDialog(),
      WaitForShow(ParentPermissionDialog::kDialogViewIdForTesting),
      PressButton(views::DialogClientView::kCancelButtonElementId),
      WaitForHide(ParentPermissionDialog::kDialogViewIdForTesting),
      CheckResult([this]() { return harness_.GetResult(); },
                  ParentPermissionDialog::Result::kParentPermissionCanceled),
      CheckHistogramBucketCount(SupervisedUserExtensionsMetricsRecorder::
                                    kParentPermissionDialogHistogramName,
                                SupervisedUserExtensionsMetricsRecorder::
                                    ParentPermissionDialogState::kOpened,
                                1),
      CheckHistogramBucketCount(
          SupervisedUserExtensionsMetricsRecorder::
              kParentPermissionDialogHistogramName,
          SupervisedUserExtensionsMetricsRecorder::ParentPermissionDialogState::
              kParentCanceled,
          1),
      // The total histogram count is 2 (one for kOpened and one for
      // kParentCanceled).
      CheckHistogramTotalCount(SupervisedUserExtensionsMetricsRecorder::
                                   kParentPermissionDialogHistogramName,
                               2),
      CheckResult(GetActionStatus(SupervisedUserExtensionsMetricsRecorder::
                                      kParentPermissionDialogOpenedActionName),
                  ActionStatus::kWasPerformed),
      CheckResult(
          GetActionStatus(SupervisedUserExtensionsMetricsRecorder::
                              kParentPermissionDialogParentCanceledActionName),
          ActionStatus::kWasPerformed))));
}

enum class ExtensionsManagingToggle : int {
  /* Extensions are managed by the dedicated
  "Skip parent approval to install extensions" FL button. */
  kExtensions = 0,
  /* Extensions are managed by the
  "Permissions for sites, apps and extensions" FL button. */
  kPermissions = 1
};

// Test which labels are used in the Parent Permission Input Section
// of the permission dialog based on the usage of the dialog
// (extension approval, other approval).
class ParentPermissionInputSectionLabelTest
    : public ParentPermissionDialogViewTest,
      public ::testing::WithParamInterface<ExtensionsManagingToggle> {
 public:
  ParentPermissionInputSectionLabelTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    if (GetParam() == ExtensionsManagingToggle::kExtensions) {
      enabled_features.push_back(
          supervised_user::
              kEnableSupervisedUserSkipParentApprovalToInstallExtensions);
      enabled_features.push_back(
          supervised_user::kUpdatedSupervisedUserExtensionApprovalStrings);
    }
    enabled_features.push_back(
        supervised_user::
            kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
    scoped_feature_list_.InitWithFeatures(enabled_features,
                                          /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ParentPermissionInputSectionLabelTest,
                       PermissionReceived_extension) {
  supervision_mixin_.SetNextReAuthStatus(
      GaiaAuthConsumer::ReAuthProofTokenStatus::kSuccess);

  // When the feature
  // `kEnableSupervisedUserSkipParentApprovalToInstallExtensions`
  // is enabled and the parent approval dialog is shown for an extensions
  // approval, extension-specific labels are shown to the Permission Input
  // section of the dialog. Otherwise, general purpose labels are used.
  auto present_parent_label_id =
      GetParam() == ExtensionsManagingToggle::kExtensions
          ? ParentPermissionDialog::
                kExtensionsParentApprovalVerificationTextIdForTesting
          : ParentPermissionDialog::kParentAccountTextIdForTesting;
  auto non_present_parent_label_id =
      GetParam() == ExtensionsManagingToggle::kExtensions
          ? ParentPermissionDialog::kParentAccountTextIdForTesting
          : ParentPermissionDialog::
                kExtensionsParentApprovalVerificationTextIdForTesting;

  RunTestSequence(InAnyContext(
      Steps(ShowDialog(),
            WaitForShow(ParentPermissionDialog::kDialogViewIdForTesting),
            WaitForShow(present_parent_label_id),
            EnsureNotPresent(non_present_parent_label_id))));
}

IN_PROC_BROWSER_TEST_P(ParentPermissionInputSectionLabelTest,
                       PermissionReceived_default) {
  supervision_mixin_.SetNextReAuthStatus(
      GaiaAuthConsumer::ReAuthProofTokenStatus::kSuccess);

  // Check that only the general purpose labels are shown when the parent
  // approval dialog is used for a purpose other than extension approval. Labels
  // related to extension approval are not shown.
  RunTestSequence(InAnyContext(
      Steps(ShowDialog(),
            WaitForShow(ParentPermissionDialog::kDialogViewIdForTesting),
            WaitForShow(ParentPermissionDialog::kParentAccountTextIdForTesting),
            EnsureNotPresent(
                ParentPermissionDialog::
                    kExtensionsParentApprovalVerificationTextIdForTesting))));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ParentPermissionInputSectionLabelTest,
    testing::Values(ExtensionsManagingToggle::kExtensions,
                    ExtensionsManagingToggle::kPermissions));

}  // namespace
