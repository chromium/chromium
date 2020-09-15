// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_install_dialog_view.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_icon_manager.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_prompt_test_helper.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/common/extension.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}  // namespace content

using extensions::Extension;
using extensions::ScopedTestDialogAutoConfirm;

class ExtensionInstallDialogViewTestSupervised
    : public extensions::ExtensionBrowserTest {
 public:
  ExtensionInstallDialogViewTestSupervised();
  ExtensionInstallDialogViewTestSupervised(
      const ExtensionInstallDialogViewTestSupervised&) = delete;
  ExtensionInstallDialogViewTestSupervised& operator=(
      const ExtensionInstallDialogViewTestSupervised&) = delete;

  void SetUpOnMainThread() override;

  // Creates and returns an install prompt.
  std::unique_ptr<ExtensionInstallPrompt::Prompt> CreatePrompt();

  content::WebContents* web_contents() { return web_contents_; }

 protected:
  ExtensionInstallDialogView* CreateAndShowPrompt(
      ExtensionInstallPromptTestHelper* helper,
      std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt);

  SupervisedUserExtensionsMetricsRecorder*
  supervised_user_extensions_metrics_recorder() {
    return supervised_user_extensions_metrics_recorder_.get();
  }

 private:
  const Extension* extension_;
  content::WebContents* web_contents_;
  std::unique_ptr<SupervisedUserExtensionsMetricsRecorder>
      supervised_user_extensions_metrics_recorder_;
};

ExtensionInstallDialogViewTestSupervised::
    ExtensionInstallDialogViewTestSupervised()
    : extension_(nullptr), web_contents_(nullptr) {}

void ExtensionInstallDialogViewTestSupervised::SetUpOnMainThread() {
  ExtensionBrowserTest::SetUpOnMainThread();

  extension_ = LoadExtension(test_data_dir_.AppendASCII(
      "install_prompt/permissions_scrollbar_regression"));

  web_contents_ = browser()->tab_strip_model()->GetWebContentsAt(0);

  supervised_user_extensions_metrics_recorder_ =
      std::make_unique<SupervisedUserExtensionsMetricsRecorder>();
}

std::unique_ptr<ExtensionInstallPrompt::Prompt>
ExtensionInstallDialogViewTestSupervised::CreatePrompt() {
  auto prompt = std::make_unique<ExtensionInstallPrompt::Prompt>(
      ExtensionInstallPrompt::INSTALL_PROMPT);
  prompt->set_extension(extension_);
  prompt->set_requires_parent_permission(true);
  prompt->AddObserver(supervised_user_extensions_metrics_recorder());

  auto icon_manager = std::make_unique<ExtensionIconManager>();
  prompt->set_icon(icon_manager->GetIcon(extension_->id()));

  return prompt;
}

ExtensionInstallDialogView*
ExtensionInstallDialogViewTestSupervised::CreateAndShowPrompt(
    ExtensionInstallPromptTestHelper* helper,
    std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt) {
  auto dialog = std::make_unique<ExtensionInstallDialogView>(
      profile(), web_contents(), helper->GetCallback(), std::move(prompt));
  ExtensionInstallDialogView* delegate_view = dialog.get();

  views::Widget* modal_dialog = views::DialogDelegate::CreateDialogWidget(
      dialog.release(), nullptr,
      platform_util::GetViewForWindow(browser()->window()->GetNativeWindow()));
  modal_dialog->Show();

  return delegate_view;
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewTestSupervised, AskAParent) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt = CreatePrompt();

  // Manipulate the passage of time.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  supervised_user_extensions_metrics_recorder()->SetClockForTesting(
      task_runner->GetMockTickClock());
  // We need to start at some non-zero point in time or else
  // DCHECK(!start_time_.is_null()) throws.
  task_runner->FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Launch the extension install dialog.
  ExtensionInstallPrompt install_prompt(profile(), nullptr);
  base::RunLoop run_loop;
  ExtensionInstallPromptTestHelper helper(run_loop.QuitClosure());
  install_prompt.ShowDialog(
      helper.GetCallback(), prompt->extension(), nullptr, std::move(prompt),
      ExtensionInstallPrompt::GetDefaultShowDialogCallback());
  run_loop.Run();
  EXPECT_EQ(ExtensionInstallPrompt::Result::ACCEPTED, helper.result());
  helper.ClearResultForTesting();

  histogram_tester.ExpectUniqueSample(SupervisedUserExtensionsMetricsRecorder::
                                          kExtensionInstallDialogHistogramName,
                                      SupervisedUserExtensionsMetricsRecorder::
                                          ExtensionInstallDialogState::kOpened,
                                      1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   SupervisedUserExtensionsMetricsRecorder::
                       kExtensionInstallDialogOpenedActionName));

  // Simulate the passage of time (11 seconds).
  base::TimeDelta duration(base::TimeDelta::FromSeconds(11));
  // Suppose the user spends 11 seconds reading the dialog.
  task_runner->FastForwardBy(duration);

  // Supervised user presses "Ask a parent".
  ExtensionInstallDialogView::SetInstallButtonDelayForTesting(0);
  ExtensionInstallDialogView* delegate_view =
      CreateAndShowPrompt(&helper, install_prompt.GetPromptForTesting());
  base::RunLoop().RunUntilIdle();
  delegate_view->AcceptDialog();
  EXPECT_EQ(ExtensionInstallPrompt::Result::ACCEPTED, helper.result());

  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::
          kExtensionInstallDialogHistogramName,
      SupervisedUserExtensionsMetricsRecorder::ExtensionInstallDialogState::
          kAskedParent,
      1);
  histogram_tester.ExpectTotalCount(SupervisedUserExtensionsMetricsRecorder::
                                        kExtensionInstallDialogHistogramName,
                                    2);
  // We should have recorded that the user spent 11 seconds.
  histogram_tester.ExpectTimeBucketCount(
      SupervisedUserExtensionsMetricsRecorder::
          kExtensionInstallDialogAskedParentTimeHistogramName,
      duration, 1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::
          kExtensionInstallDialogAskedParentTimeHistogramName,
      1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   SupervisedUserExtensionsMetricsRecorder::
                       kExtensionInstallDialogAskedParentActionName));
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewTestSupervised,
                       ChildCanceled) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::CANCEL);

  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt = CreatePrompt();

  // Manipulate the passage of time.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  supervised_user_extensions_metrics_recorder()->SetClockForTesting(
      task_runner->GetMockTickClock());
  // We need to start at some non-zero point in time or else
  // DCHECK(!start_time_.is_null()) throws.
  task_runner->FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Launch the extension install dialog.
  ExtensionInstallPrompt install_prompt(profile(), nullptr);
  base::RunLoop run_loop;
  ExtensionInstallPromptTestHelper helper(run_loop.QuitClosure());
  install_prompt.ShowDialog(
      helper.GetCallback(), prompt->extension(), nullptr, std::move(prompt),
      ExtensionInstallPrompt::GetDefaultShowDialogCallback());
  run_loop.Run();
  EXPECT_EQ(ExtensionInstallPrompt::Result::USER_CANCELED, helper.result());
  helper.ClearResultForTesting();

  histogram_tester.ExpectUniqueSample(SupervisedUserExtensionsMetricsRecorder::
                                          kExtensionInstallDialogHistogramName,
                                      SupervisedUserExtensionsMetricsRecorder::
                                          ExtensionInstallDialogState::kOpened,
                                      1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   SupervisedUserExtensionsMetricsRecorder::
                       kExtensionInstallDialogOpenedActionName));

  // Simulate the passage of time (15 seconds);
  base::TimeDelta duration(base::TimeDelta::FromSeconds(15));
  // Suppose the user spends 15 seconds reading the dialog.
  task_runner->FastForwardBy(duration);

  // Supervised user presses "Cancel".
  ExtensionInstallDialogView* delegate_view =
      CreateAndShowPrompt(&helper, install_prompt.GetPromptForTesting());
  delegate_view->CancelDialog();
  EXPECT_EQ(ExtensionInstallPrompt::Result::USER_CANCELED, helper.result());

  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::
          kExtensionInstallDialogHistogramName,
      SupervisedUserExtensionsMetricsRecorder::ExtensionInstallDialogState::
          kChildCanceled,
      1);
  histogram_tester.ExpectTotalCount(SupervisedUserExtensionsMetricsRecorder::
                                        kExtensionInstallDialogHistogramName,
                                    2);
  // We should have recorded that the user spent 15 seconds.
  histogram_tester.ExpectTimeBucketCount(
      SupervisedUserExtensionsMetricsRecorder::
          kExtensionInstallDialogChildCanceledTimeHistogramName,
      duration, 1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::
          kExtensionInstallDialogChildCanceledTimeHistogramName,
      1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   SupervisedUserExtensionsMetricsRecorder::
                       kExtensionInstallDialogChildCanceledActionName));
}
