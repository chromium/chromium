// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/supervised_user/parent_permission_dialog.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
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
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/supervised_user/parent_permission_dialog.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/supervised_user/parent_permission_dialog_view.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "components/signin/public/identity_manager/identity_manager.h"
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
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/image/image_skia.h"

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
      NOTREACHED_NORETURN();
  }
}

namespace {

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
  // The next dialog action to take.
  enum class NextDialogAction {
    kCancel,
    kAccept,
  };

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
    // Blocks ShowUi until the dialog is intercepted and stored at
    // `under_test_`.
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();

    gfx::ImageSkia icon = gfx::ImageSkia::CreateFrom1xBitmap(
        *gfx::Image(extensions::util::GetDefaultExtensionIcon()).ToSkBitmap());
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();

    dialog_ = CreatePermissionDialog(
        dialog_input, browser, contents, icon,
        base::BindOnce(
            &ParentPermissionDialogViewHarness::OnParentPermissionDialogDone,
            base::Unretained(this)));

    dialog_->ShowDialog();
    run_loop.Run();
  }

  void set_ignore_result() { ignore_result_ = true; }

  void set_next_action(NextDialogAction next_dialog_action) {
    next_dialog_action_ = next_dialog_action;
  }

  bool InvalidCredentialWasReceived() {
    CHECK(under_test_) << "No permission view intercepted.";
    return under_test_->GetInvalidCredentialReceived();
  }

 protected:
  template <typename T>
  std::unique_ptr<ParentPermissionDialog> CreatePermissionDialog(
      T dialog_input,
      Browser* browser,
      content::WebContents* contents,
      gfx::ImageSkia icon,
      ParentPermissionDialog::DoneCallback done_callback);

  template <>
  std::unique_ptr<ParentPermissionDialog> CreatePermissionDialog(
      std::u16string dialog_input,
      Browser* browser,
      content::WebContents* contents,
      gfx::ImageSkia icon,
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
      ParentPermissionDialog::DoneCallback done_callback) {
    return ParentPermissionDialog::CreateParentPermissionDialogForExtension(
        browser->profile(), contents->GetTopLevelNativeWindow(), icon,
        dialog_input, std::move(done_callback));
  }

 private:
  void OnParentPermissionDialogDone(ParentPermissionDialog::Result result) {
    if (ignore_result_) {
      return;
    }

    result_ = result;
    std::move(quit_closure_).Run();
  }

  // TestParentPermissionDialogViewObserver - store reference to the view under
  // test.
  void OnTestParentPermissionDialogViewCreated(
      ParentPermissionDialogView* view) override {
    if (ignore_result_ && quit_closure_) {
      std::move(quit_closure_).Run();
    }

    under_test_ = view;
    under_test_->SetIdentityManagerForTesting(
        supervision_mixin_->GetIdentityTestEnvironment()->identity_manager());
    under_test_->SetRepromptAfterIncorrectCredential(false);

    if (!next_dialog_action_.has_value()) {
      return;
    }

    switch (*next_dialog_action_) {
      case NextDialogAction::kCancel:
        under_test_->CancelDialog();
        break;
      case NextDialogAction::kAccept:
        under_test_->AcceptDialog();
        break;
      default:
        NOTREACHED_NORETURN();
    }
  }

  // Provides identity manager to the view.
  raw_ref<supervised_user::SupervisionMixin> supervision_mixin_;

  // `under_test_` is intercepted by OnTestParentPermissionDialogViewCreated.
  raw_ptr<ParentPermissionDialogView, DisableDanglingPtrDetection> under_test_;

  // `under_test_`'s underlying dialog.
  std::unique_ptr<ParentPermissionDialog> dialog_;

  // Closures that allow to block until async UI is done.
  base::OnceClosure quit_closure_;

  // Optional result, if dialog was interacted.
  absl::optional<ParentPermissionDialog::Result> result_;

  // When set to true, will not wait for completion of the Done callback.
  bool ignore_result_{false};

  absl::optional<NextDialogAction> next_dialog_action_;
};

// End to end test of ParentPermissionDialog that exercises the dialog's
// internal logic that orchestrates the parental permission process.
class ParentPermissionDialogViewTest
    : public SupportsTestDialog<MixinBasedInProcessBrowserTest> {
 public:
  ParentPermissionDialogViewTest() { supervision_mixin_.InitFeatures(); }

 protected:
  void ShowUi(const std::string& name) override {
    if (name == "default") {
      harness_.ShowUi(std::u16string(u"Test prompt message"), browser());
      return;
    } else if (name == "extension") {
      harness_.ShowUi(test_extension_.get(), browser());
      return;
    } else {
      NOTREACHED_NORETURN() << "Check the suffix of the test name.";
    }
  }

  void SetUpOnMainThread() override {
    // Default ::SetUpOnMainThread() of all dependent mixins are invoked here.
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();

    // Do not continue until family is loaded. Otherwise tests will start
    // without family permissions set (and crash or fail).
    family_fetched_lock_.Wait();

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
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  // Order is important: family_notifier_ will start observing family
  // preferences before supervision_mixin_ will launch fetches.
  supervised_user::FamilyFetchedLock family_fetched_lock_{mixin_host_, this};
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

  const extensions::Extension* test_extension() {
    return test_extension_.get();
  }

  extensions::ExtensionService* extension_service() {
    return extensions::ExtensionSystem::Get(browser()->profile())
        ->extension_service();
  }

  std::unique_ptr<extensions::SupervisedUserExtensionsDelegate>
      supervised_user_extensions_delegate_;

 private:
  scoped_refptr<const extensions::Extension> test_extension_;
};

// Tests that a plain dialog widget is shown using the TestBrowserUi
// infrastructure.
IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest, InvokeUi_default) {
  harness_.set_ignore_result();
  ShowAndVerifyUi();
}

// Tests that a plain dialog widget is shown using the TestBrowserUi
// infrastructure.
IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest, InvokeUi_extension) {
  harness_.set_ignore_result();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest,
                       PermissionReceived_default) {
  harness_.set_next_action(
      ParentPermissionDialogViewHarness::NextDialogAction::kAccept);

  ShowUi(NameFromTestCase());

  EXPECT_EQ(harness_.GetResult(),
            ParentPermissionDialog::Result::kParentPermissionReceived);
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest,
                       PermissionFailedInvalidPassword_default) {
  harness_.set_next_action(
      ParentPermissionDialogViewHarness::NextDialogAction::kAccept);
  supervision_mixin_.SetNextReAuthStatus(
      GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant);

  ShowUi(NameFromTestCase());

  EXPECT_TRUE(harness_.InvalidCredentialWasReceived());
  EXPECT_EQ(harness_.GetResult(),
            ParentPermissionDialog::Result::kParentPermissionFailed);
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest,
                       PermissionDialogCanceled_default) {
  harness_.set_next_action(
      ParentPermissionDialogViewHarness::NextDialogAction::kCancel);

  ShowUi(NameFromTestCase());

  EXPECT_EQ(harness_.GetResult(),
            ParentPermissionDialog::Result::kParentPermissionCanceled);
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogViewTest,
                       PermissionReceived_extension) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  supervision_mixin_.SetNextReAuthStatus(
      GaiaAuthConsumer::ReAuthProofTokenStatus::kSuccess);
  harness_.set_next_action(
      ParentPermissionDialogViewHarness::NextDialogAction::kAccept);

  ShowUi(NameFromTestCase());

  ASSERT_EQ(harness_.GetResult(),
            ParentPermissionDialog::Result::kParentPermissionReceived);

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
                       PermissionFailedInvalidPassword_extension) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  supervision_mixin_.SetNextReAuthStatus(
      GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant);
  harness_.set_next_action(
      ParentPermissionDialogViewHarness::NextDialogAction::kAccept);

  ShowUi(NameFromTestCase());

  ASSERT_TRUE(harness_.InvalidCredentialWasReceived());
  ASSERT_EQ(harness_.GetResult(),
            ParentPermissionDialog::Result::kParentPermissionFailed);

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
                       PermissionDialogCanceled_extension) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  harness_.set_next_action(
      ParentPermissionDialogViewHarness::NextDialogAction::kCancel);

  ShowUi(NameFromTestCase());

  ASSERT_EQ(harness_.GetResult(),
            ParentPermissionDialog::Result::kParentPermissionCanceled);

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
}  // namespace
