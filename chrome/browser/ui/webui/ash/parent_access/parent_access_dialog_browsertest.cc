// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/parent_access/parent_access_dialog.h"

#include <memory>

#include "ash/shell.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_browsertest_base.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace {
bool DialogResultsEqual(const ash::ParentAccessDialog::Result& first,
                        const ash::ParentAccessDialog::Result& second) {
  return first.status == second.status &&
         first.parent_access_token == second.parent_access_token;
}
}  // namespace

namespace ash {

using ParentAccessDialogBrowserTest = ParentAccessChildUserBrowserTestBase;

// Verify that the dialog is shown and correctly configured.
IN_PROC_BROWSER_TEST_F(ParentAccessDialogBrowserTest, ShowDialog) {
  base::RunLoop run_loop;

  // Create the callback.
  ParentAccessDialog::Callback callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<ParentAccessDialog::Result> result) -> void {
        EXPECT_EQ(result->status,
                  ParentAccessDialog::Result::Status::kCanceled);
        run_loop.Quit();
      });

  // Show the dialog.
  ParentAccessDialogProvider provider;
  ParentAccessDialogProvider::ShowError error = provider.Show(
      parent_access_ui::mojom::ParentAccessParams::New(
          parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess,
          parent_access_ui::mojom::FlowTypeParams::NewWebApprovalsParams(
              parent_access_ui::mojom::WebApprovalsParams::New())),
      std::move(callback));

  // Verify it is showing.
  ASSERT_EQ(error, ParentAccessDialogProvider::ShowError::kNone);
  ASSERT_NE(ParentAccessDialog::GetInstance(), nullptr);
  const ParentAccessDialog* dialog = ParentAccessDialog::GetInstance();
  parent_access_ui::mojom::ParentAccessParams* params =
      dialog->GetParentAccessParamsForTest();
  EXPECT_EQ(
      params->flow_type,
      parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess);

  // Verify that it is correctly configured.
  EXPECT_EQ(dialog->GetDialogContentURL().spec(),
            chrome::kChromeUIParentAccessURL);
  EXPECT_TRUE(dialog->ShouldShowCloseButton());
  EXPECT_EQ(dialog->GetDialogModalType(), ui::ModalType::MODAL_TYPE_SYSTEM);

  // Send ESCAPE keypress.  EventGenerator requires the root window, which has
  // to be fetched from the Ash shell.
  ui::test::EventGenerator generator(Shell::Get()->GetPrimaryRootWindow());
  generator.PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);

  // The dialog instance should be gone after ESC is pressed.
  EXPECT_EQ(ParentAccessDialog::GetInstance(), nullptr);

  run_loop.Run();
}

// Verify that the dialog is closed on Approve.
IN_PROC_BROWSER_TEST_F(ParentAccessDialogBrowserTest, SetApproved) {
  base::RunLoop run_loop;

  ParentAccessDialog::Result expected_result;
  expected_result.status = ParentAccessDialog::Result::Status::kApproved;
  expected_result.parent_access_token = "TEST_TOKEN";
  expected_result.parent_access_token_expire_timestamp =
      base::Time::FromDoubleT(123456L);

  // Create the callback.
  ParentAccessDialog::Callback callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<ParentAccessDialog::Result> result) -> void {
        EXPECT_TRUE(DialogResultsEqual(*result, expected_result));
        run_loop.Quit();
      });

  // Show the dialog.
  ParentAccessDialogProvider provider;
  provider.Show(
      parent_access_ui::mojom::ParentAccessParams::New(
          parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess,
          parent_access_ui::mojom::FlowTypeParams::NewWebApprovalsParams(
              parent_access_ui::mojom::WebApprovalsParams::New())),
      std::move(callback));

  // Set the result.
  ParentAccessDialog::GetInstance()->SetApproved(
      expected_result.parent_access_token,
      expected_result.parent_access_token_expire_timestamp);

  run_loop.Run();

  // The dialog instance should be gone after SetResult() is called.
  EXPECT_EQ(ParentAccessDialog::GetInstance(), nullptr);
}

// Verify that the dialog is closed on Decline.
IN_PROC_BROWSER_TEST_F(ParentAccessDialogBrowserTest, SetDeclined) {
  base::RunLoop run_loop;

  ParentAccessDialog::Result expected_result;
  expected_result.status = ParentAccessDialog::Result::Status::kDeclined;

  // Create the callback.
  ParentAccessDialog::Callback callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<ParentAccessDialog::Result> result) -> void {
        EXPECT_TRUE(DialogResultsEqual(*result, expected_result));
        run_loop.Quit();
      });

  // Show the dialog.
  ParentAccessDialogProvider provider;
  provider.Show(
      parent_access_ui::mojom::ParentAccessParams::New(
          parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess,
          parent_access_ui::mojom::FlowTypeParams::NewWebApprovalsParams(
              parent_access_ui::mojom::WebApprovalsParams::New())),
      std::move(callback));

  // Set the result.
  ParentAccessDialog::GetInstance()->SetDeclined();

  run_loop.Run();

  // The dialog instance should be gone after SetResult() is called.
  EXPECT_EQ(ParentAccessDialog::GetInstance(), nullptr);
}

// Verify that the dialog is closed on Cancel.
IN_PROC_BROWSER_TEST_F(ParentAccessDialogBrowserTest, SetCanceled) {
  base::RunLoop run_loop;

  ParentAccessDialog::Result expected_result;
  expected_result.status = ParentAccessDialog::Result::Status::kCanceled;

  // Create the callback.
  ParentAccessDialog::Callback callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<ParentAccessDialog::Result> result) -> void {
        EXPECT_TRUE(DialogResultsEqual(*result, expected_result));
        run_loop.Quit();
      });

  // Show the dialog.
  ParentAccessDialogProvider provider;
  provider.Show(
      parent_access_ui::mojom::ParentAccessParams::New(
          parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess,
          parent_access_ui::mojom::FlowTypeParams::NewWebApprovalsParams(
              parent_access_ui::mojom::WebApprovalsParams::New())),
      std::move(callback));

  // Set the result.
  ParentAccessDialog::GetInstance()->SetCanceled();

  run_loop.Run();

  // The dialog instance should be gone after SetResult() is called.
  EXPECT_EQ(ParentAccessDialog::GetInstance(), nullptr);
}

// Verify that the dialog is closed on Cancel.
IN_PROC_BROWSER_TEST_F(ParentAccessDialogBrowserTest, SetError) {
  base::RunLoop run_loop;

  ParentAccessDialog::Result expected_result;
  expected_result.status = ParentAccessDialog::Result::Status::kError;

  // Create the callback.
  ParentAccessDialog::Callback callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<ParentAccessDialog::Result> result) -> void {
        EXPECT_TRUE(DialogResultsEqual(*result, expected_result));
        run_loop.Quit();
      });

  // Show the dialog.
  ParentAccessDialogProvider provider;
  provider.Show(
      parent_access_ui::mojom::ParentAccessParams::New(
          parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess,
          parent_access_ui::mojom::FlowTypeParams::NewWebApprovalsParams(
              parent_access_ui::mojom::WebApprovalsParams::New())),
      std::move(callback));

  // Set the result.
  ParentAccessDialog::GetInstance()->SetError();

  // The dialog instance should not be closed in the error state.
  EXPECT_NE(ParentAccessDialog::GetInstance(), nullptr);
  // Ensure that the callback is run when the dialog is manually closed.
  ParentAccessDialog::GetInstance()->Close();

  run_loop.Run();
}

// Verify that if dialog is destroyed without a Result,  it reports being
// canceled.
IN_PROC_BROWSER_TEST_F(ParentAccessDialogBrowserTest, DestroyedWithoutResult) {
  base::RunLoop run_loop;

  ParentAccessDialog::Result expected_result;
  expected_result.status = ParentAccessDialog::Result::Status::kCanceled;

  // Create the callback.
  ParentAccessDialog::Callback callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<ParentAccessDialog::Result> result) -> void {
        EXPECT_TRUE(DialogResultsEqual(*result, expected_result));
        run_loop.Quit();
      });

  // Show the dialog.
  ParentAccessDialogProvider provider;
  provider.Show(
      parent_access_ui::mojom::ParentAccessParams::New(
          parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess,
          parent_access_ui::mojom::FlowTypeParams::NewWebApprovalsParams(
              parent_access_ui::mojom::WebApprovalsParams::New())),
      std::move(callback));

  // Set the result.
  ParentAccessDialog::GetInstance()->Close();

  run_loop.Run();

  // The dialog instance should be gone after SetResult() is called.
  EXPECT_EQ(ParentAccessDialog::GetInstance(), nullptr);
}

IN_PROC_BROWSER_TEST_F(ParentAccessDialogBrowserTest,
                       ErrorOnDialogAlreadyVisible) {
  base::HistogramTester histogram_tester;
  // Show the dialog.
  ParentAccessDialogProvider provider;
  ParentAccessDialogProvider::ShowError error = provider.Show(
      parent_access_ui::mojom::ParentAccessParams::New(
          parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess,
          parent_access_ui::mojom::FlowTypeParams::NewWebApprovalsParams(
              parent_access_ui::mojom::WebApprovalsParams::New())),
      base::DoNothing());

  // Verify it is showing.
  ASSERT_EQ(error, ParentAccessDialogProvider::ShowError::kNone);
  ASSERT_NE(ParentAccessDialog::GetInstance(), nullptr);
  parent_access_ui::mojom::ParentAccessParams* params =
      ParentAccessDialog::GetInstance()->GetParentAccessParamsForTest();
  EXPECT_EQ(
      params->flow_type,
      parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess);

  error = provider.Show(
      parent_access_ui::mojom::ParentAccessParams::New(
          parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess,
          parent_access_ui::mojom::FlowTypeParams::NewWebApprovalsParams(
              parent_access_ui::mojom::WebApprovalsParams::New())),
      base::DoNothing());

  // Verify an error was returned indicating it can't be shown again.
  EXPECT_EQ(error,
            ParentAccessDialogProvider::ShowError::kDialogAlreadyVisible);
  EXPECT_NE(ParentAccessDialog::GetInstance(), nullptr);

  // Verify that metrics were recorded.
  histogram_tester.ExpectUniqueSample(
      ParentAccessDialogProvider::
          GetParentAccessWidgetShowDialogErrorHistogramForFlowType(
              absl::nullopt),
      ParentAccessDialogProvider::ShowErrorType::kAlreadyVisible, 1);
  histogram_tester.ExpectUniqueSample(
      ParentAccessDialogProvider::
          GetParentAccessWidgetShowDialogErrorHistogramForFlowType(
              parent_access_ui::mojom::ParentAccessParams::FlowType::
                  kWebsiteAccess),
      ParentAccessDialogProvider::ShowErrorType::kAlreadyVisible, 1);
}

using ParentAccessDialogRegularUserBrowserTest =
    ParentAccessRegularUserBrowserTestBase;

// Verify that the dialog is not shown for non child users.
IN_PROC_BROWSER_TEST_F(ParentAccessDialogRegularUserBrowserTest,
                       ErrorForNonChildUser) {
  base::HistogramTester histogram_tester;
  // Show the dialog.
  ParentAccessDialogProvider provider;
  ParentAccessDialogProvider::ShowError error = provider.Show(
      parent_access_ui::mojom::ParentAccessParams::New(
          parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess,
          parent_access_ui::mojom::FlowTypeParams::NewWebApprovalsParams(
              parent_access_ui::mojom::WebApprovalsParams::New())),
      base::DoNothing());

  // Verify it is not showing.
  EXPECT_EQ(error, ParentAccessDialogProvider::ShowError::kNotAChildUser);
  EXPECT_EQ(ParentAccessDialog::GetInstance(), nullptr);

  // Verify that metrics were recorded.
  histogram_tester.ExpectUniqueSample(
      ParentAccessDialogProvider::
          GetParentAccessWidgetShowDialogErrorHistogramForFlowType(
              absl::nullopt),
      ParentAccessDialogProvider::ShowErrorType::kNotAChildUser, 1);
  histogram_tester.ExpectUniqueSample(
      ParentAccessDialogProvider::
          GetParentAccessWidgetShowDialogErrorHistogramForFlowType(
              parent_access_ui::mojom::ParentAccessParams::FlowType::
                  kWebsiteAccess),
      ParentAccessDialogProvider::ShowErrorType::kNotAChildUser, 1);
}

}  // namespace ash
