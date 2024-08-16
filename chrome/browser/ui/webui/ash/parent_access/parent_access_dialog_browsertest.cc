// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/parent_access/parent_access_dialog.h"

#include <memory>

#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_browsertest_base.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_metrics_utils.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
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

parent_access_ui::mojom::ParentAccessParamsPtr GetTestParamsForFlowType(
    parent_access_ui::mojom::ParentAccessParams::FlowType flow_type) {
  switch (flow_type) {
    case parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess:
      return parent_access_ui::mojom::ParentAccessParams::New(
          flow_type,
          parent_access_ui::mojom::FlowTypeParams::NewWebApprovalsParams(
              parent_access_ui::mojom::WebApprovalsParams::New()),
          /*is_disabled=*/false);
    case parent_access_ui::mojom::ParentAccessParams::FlowType::
        kExtensionAccess:
      return parent_access_ui::mojom::ParentAccessParams::New(
          flow_type,
          parent_access_ui::mojom::FlowTypeParams::NewExtensionApprovalsParams(
              parent_access_ui::mojom::ExtensionApprovalsParams::New()),
          /*is_disabled=*/false);
  }
}
}  // namespace

namespace ash {

class ParentAccessDialogBrowserTest
    : public ParentAccessChildUserBrowserTestBase,
      public testing::WithParamInterface<
          parent_access_ui::mojom::ParentAccessParams::FlowType> {
 public:
  ParentAccessDialogBrowserTest() = default;

  parent_access_ui::mojom::ParentAccessParams::FlowType GetTestedFlowType()
      const {
    return GetParam();
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ParentAccessDialogBrowserTest,
    testing::Values(
        parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess,
        parent_access_ui::mojom::ParentAccessParams::FlowType::
            kExtensionAccess));

// Verify that the dialog is shown and correctly configured.
IN_PROC_BROWSER_TEST_P(ParentAccessDialogBrowserTest, ShowDialog) {
  base::RunLoop run_loop;

  ParentAccessDialog::Callback callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<ParentAccessDialog::Result> result) -> void {
        EXPECT_EQ(result->status,
                  ParentAccessDialog::Result::Status::kCanceled);
        run_loop.Quit();
      });
  ParentAccessDialogProvider provider;
  ParentAccessDialogProvider::ShowError error = provider.Show(
      GetTestParamsForFlowType(GetTestedFlowType()), std::move(callback));

  // Verify dialog is showing.
  ASSERT_EQ(error, ParentAccessDialogProvider::ShowError::kNone);
  ASSERT_NE(ParentAccessDialog::GetInstance(), nullptr);
  const ParentAccessDialog* dialog = ParentAccessDialog::GetInstance();
  parent_access_ui::mojom::ParentAccessParams* params =
      dialog->GetParentAccessParamsForTest();
  EXPECT_EQ(params->flow_type, GetTestedFlowType());

  // Verify that it is correctly configured.
  EXPECT_EQ(dialog->GetDialogContentURL().spec(),
            chrome::kChromeUIParentAccessURL);
  EXPECT_FALSE(dialog->ShouldShowDialogTitle());
  EXPECT_FALSE(dialog->ShouldShowCloseButton());
  EXPECT_EQ(dialog->GetDialogModalType(), ui::mojom::ModalType::kSystem);

  // Send ESCAPE keypress.  EventGenerator requires the root window, which has
  // to be fetched from the Ash shell.
  ui::test::EventGenerator generator(Shell::Get()->GetPrimaryRootWindow());
  generator.PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);

  // The dialog instance should be gone after ESC is pressed.
  EXPECT_EQ(ParentAccessDialog::GetInstance(), nullptr);

  run_loop.Run();
}

// Verify that the dialog is closed on Approve.
IN_PROC_BROWSER_TEST_P(ParentAccessDialogBrowserTest, SetApproved) {
  base::RunLoop run_loop;

  ParentAccessDialog::Result expected_result;
  expected_result.status = ParentAccessDialog::Result::Status::kApproved;
  expected_result.parent_access_token = "TEST_TOKEN";
  expected_result.parent_access_token_expire_timestamp =
      base::Time::FromSecondsSinceUnixEpoch(123456L);

  ParentAccessDialog::Callback callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<ParentAccessDialog::Result> result) -> void {
        EXPECT_TRUE(DialogResultsEqual(*result, expected_result));
        run_loop.Quit();
      });

  ParentAccessDialogProvider provider;
  provider.Show(GetTestParamsForFlowType(GetTestedFlowType()),
                std::move(callback));

  ParentAccessDialog::GetInstance()->SetApproved(
      expected_result.parent_access_token,
      expected_result.parent_access_token_expire_timestamp);

  run_loop.Run();

  // The dialog instance should be gone after SetResult() is called.
  EXPECT_EQ(ParentAccessDialog::GetInstance(), nullptr);
}

// Verify that the dialog is closed on Decline.
IN_PROC_BROWSER_TEST_P(ParentAccessDialogBrowserTest, SetDeclined) {
  base::RunLoop run_loop;

  ParentAccessDialog::Result expected_result;
  expected_result.status = ParentAccessDialog::Result::Status::kDeclined;

  ParentAccessDialog::Callback callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<ParentAccessDialog::Result> result) -> void {
        EXPECT_TRUE(DialogResultsEqual(*result, expected_result));
        run_loop.Quit();
      });

  ParentAccessDialogProvider provider;
  provider.Show(GetTestParamsForFlowType(GetTestedFlowType()),
                std::move(callback));

  ParentAccessDialog::GetInstance()->SetDeclined();

  run_loop.Run();

  // The dialog instance should be gone after SetResult() is called.
  EXPECT_EQ(ParentAccessDialog::GetInstance(), nullptr);
}

// Verify that the dialog is closed on Cancel.
IN_PROC_BROWSER_TEST_P(ParentAccessDialogBrowserTest, SetCanceled) {
  base::RunLoop run_loop;

  ParentAccessDialog::Result expected_result;
  expected_result.status = ParentAccessDialog::Result::Status::kCanceled;

  ParentAccessDialog::Callback callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<ParentAccessDialog::Result> result) -> void {
        EXPECT_TRUE(DialogResultsEqual(*result, expected_result));
        run_loop.Quit();
      });

  ParentAccessDialogProvider provider;
  provider.Show(GetTestParamsForFlowType(GetTestedFlowType()),
                std::move(callback));

  ParentAccessDialog::GetInstance()->SetCanceled();

  run_loop.Run();

  // The dialog instance should be gone after SetResult() is called.
  EXPECT_EQ(ParentAccessDialog::GetInstance(), nullptr);
}

// Verify that the dialog is closed on Cancel.
IN_PROC_BROWSER_TEST_P(ParentAccessDialogBrowserTest, SetError) {
  base::RunLoop run_loop;

  ParentAccessDialog::Result expected_result;
  expected_result.status = ParentAccessDialog::Result::Status::kError;

  ParentAccessDialog::Callback callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<ParentAccessDialog::Result> result) -> void {
        EXPECT_TRUE(DialogResultsEqual(*result, expected_result));
        run_loop.Quit();
      });

  ParentAccessDialogProvider provider;
  provider.Show(GetTestParamsForFlowType(GetTestedFlowType()),
                std::move(callback));

  ParentAccessDialog::GetInstance()->SetError();

  // The dialog instance should not be closed in the error state.
  EXPECT_NE(ParentAccessDialog::GetInstance(), nullptr);
  // Ensure that the callback is run when the dialog is manually closed.
  ParentAccessDialog::GetInstance()->Close();

  run_loop.Run();
}

// Verify that if dialog is destroyed without a Result,  it reports being
// canceled.
IN_PROC_BROWSER_TEST_P(ParentAccessDialogBrowserTest, DestroyedWithoutResult) {
  base::RunLoop run_loop;

  ParentAccessDialog::Result expected_result;
  expected_result.status = ParentAccessDialog::Result::Status::kCanceled;

  ParentAccessDialog::Callback callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<ParentAccessDialog::Result> result) -> void {
        EXPECT_TRUE(DialogResultsEqual(*result, expected_result));
        run_loop.Quit();
      });

  ParentAccessDialogProvider provider;
  provider.Show(GetTestParamsForFlowType(GetTestedFlowType()),
                std::move(callback));

  ParentAccessDialog::GetInstance()->Close();

  run_loop.Run();

  // The dialog instance should be gone after SetResult() is called.
  EXPECT_EQ(ParentAccessDialog::GetInstance(), nullptr);
}

IN_PROC_BROWSER_TEST_P(ParentAccessDialogBrowserTest,
                       ErrorOnDialogAlreadyVisible) {
  base::HistogramTester histogram_tester;
  ParentAccessDialogProvider provider;
  ParentAccessDialogProvider::ShowError error = provider.Show(
      GetTestParamsForFlowType(GetTestedFlowType()), base::DoNothing());

  // Verify the dialog is showing.
  ASSERT_EQ(error, ParentAccessDialogProvider::ShowError::kNone);
  ASSERT_NE(ParentAccessDialog::GetInstance(), nullptr);
  parent_access_ui::mojom::ParentAccessParams* params =
      ParentAccessDialog::GetInstance()->GetParentAccessParamsForTest();
  EXPECT_EQ(params->flow_type, GetTestedFlowType());

  error = provider.Show(GetTestParamsForFlowType(GetTestedFlowType()),
                        base::DoNothing());

  // Verify an error was returned indicating it can't be shown again.
  EXPECT_EQ(error,
            ParentAccessDialogProvider::ShowError::kDialogAlreadyVisible);
  EXPECT_NE(ParentAccessDialog::GetInstance(), nullptr);

  // Verify that metrics were recorded.
  histogram_tester.ExpectUniqueSample(
      parent_access::GetHistogramTitleForFlowType(
          parent_access::kParentAccessWidgetShowDialogErrorHistogramBase,
          std::nullopt),
      ParentAccessDialogProvider::ShowErrorType::kAlreadyVisible, 1);
  histogram_tester.ExpectUniqueSample(
      parent_access::GetHistogramTitleForFlowType(
          parent_access::kParentAccessWidgetShowDialogErrorHistogramBase,
          GetTestedFlowType()),
      ParentAccessDialogProvider::ShowErrorType::kAlreadyVisible, 1);
}

using ParentAccessDialogExtensionApprovalsDisabledTest =
    ParentAccessChildUserBrowserTestBase;

// Only test disabled case for Extension flow because it is not possible for
// other flows.
IN_PROC_BROWSER_TEST_F(ParentAccessDialogExtensionApprovalsDisabledTest,
                       SetDisabled) {
  base::RunLoop run_loop;

  ParentAccessDialog::Result expected_result;
  expected_result.status = ParentAccessDialog::Result::Status::kDisabled;

  ParentAccessDialog::Callback callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<ParentAccessDialog::Result> result) -> void {
        EXPECT_TRUE(DialogResultsEqual(*result, expected_result));
        run_loop.Quit();
      });

  ParentAccessDialogProvider provider;
  provider.Show(
      parent_access_ui::mojom::ParentAccessParams::New(
          parent_access_ui::mojom::ParentAccessParams::FlowType::
              kExtensionAccess,
          parent_access_ui::mojom::FlowTypeParams::NewExtensionApprovalsParams(
              parent_access_ui::mojom::ExtensionApprovalsParams::New()),
          /*is_disabled=*/true),
      std::move(callback));

  ParentAccessDialog::GetInstance()->SetDisabled();

  run_loop.Run();

  // The dialog instance should be gone after SetResult() is called.
  EXPECT_EQ(ParentAccessDialog::GetInstance(), nullptr);
}

class ParentAccessDialogRegularUserBrowserTest
    : public ParentAccessRegularUserBrowserTestBase,
      public testing::WithParamInterface<
          parent_access_ui::mojom::ParentAccessParams::FlowType> {
 public:
  ParentAccessDialogRegularUserBrowserTest() = default;

  parent_access_ui::mojom::ParentAccessParams::FlowType GetTestedFlowType()
      const {
    return GetParam();
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ParentAccessDialogRegularUserBrowserTest,
    testing::Values(
        parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess,
        parent_access_ui::mojom::ParentAccessParams::FlowType::
            kExtensionAccess));

// Verify that the dialog is not shown for non child users.
IN_PROC_BROWSER_TEST_P(ParentAccessDialogRegularUserBrowserTest,
                       ErrorForNonChildUser) {
  base::HistogramTester histogram_tester;

  ParentAccessDialogProvider provider;
  ParentAccessDialogProvider::ShowError error = provider.Show(
      GetTestParamsForFlowType(GetTestedFlowType()), base::DoNothing());

  // Verify the dialog is not showing and metrics were recorded.
  EXPECT_EQ(error, ParentAccessDialogProvider::ShowError::kNotAChildUser);
  EXPECT_EQ(ParentAccessDialog::GetInstance(), nullptr);
  histogram_tester.ExpectUniqueSample(
      parent_access::GetHistogramTitleForFlowType(
          parent_access::kParentAccessWidgetShowDialogErrorHistogramBase,
          std::nullopt),
      ParentAccessDialogProvider::ShowErrorType::kNotAChildUser, 1);
  histogram_tester.ExpectUniqueSample(
      parent_access::GetHistogramTitleForFlowType(
          parent_access::kParentAccessWidgetShowDialogErrorHistogramBase,
          GetTestedFlowType()),
      ParentAccessDialogProvider::ShowErrorType::kNotAChildUser, 1);
}

}  // namespace ash
