// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/autofill_error_dialog_controller_impl.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/ui/payments/autofill_error_dialog_view.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

class TestAutofillErrorDialogView : public AutofillErrorDialogView {
 public:
  TestAutofillErrorDialogView() = default;
  TestAutofillErrorDialogView(const TestAutofillErrorDialogView&) = delete;
  TestAutofillErrorDialogView& operator=(const TestAutofillErrorDialogView&) =
      delete;
  ~TestAutofillErrorDialogView() override = default;

  void Dismiss() override {}

  base::WeakPtr<AutofillErrorDialogView> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TestAutofillErrorDialogView> weak_ptr_factory_{this};
};

class AutofillErrorDialogControllerImplTest : public testing::Test {
 public:
  AutofillErrorDialogControllerImplTest() = default;

  void ShowPrompt(const AutofillErrorDialogContext& context) {
    controller_ = std::make_unique<AutofillErrorDialogControllerImpl>(context);
    controller_->Show(base::BindOnce(
        &AutofillErrorDialogControllerImplTest::CreateErrorDialogView,
        base::Unretained(this)));
  }

  base::WeakPtr<AutofillErrorDialogView> CreateErrorDialogView() {
    view_ = std::make_unique<TestAutofillErrorDialogView>();
    return view_->GetWeakPtr();
  }

  AutofillErrorDialogControllerImpl* controller() { return controller_.get(); }

 private:
  std::unique_ptr<TestAutofillErrorDialogView> view_;
  std::unique_ptr<AutofillErrorDialogControllerImpl> controller_;
};

#if BUILDFLAG(IS_IOS)
TEST_F(AutofillErrorDialogControllerImplTest, CreditCardUploadError) {
  AutofillErrorDialogContext context;
  context.type = AutofillErrorDialogType::kCreditCardUploadError;

  ShowPrompt(context);

  EXPECT_EQ(controller()->GetTitle(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_FAILURE_TITLE_TEXT));
  EXPECT_EQ(controller()->GetDescription(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_FAILURE_DESCRIPTION_TEXT));
  EXPECT_EQ(controller()->GetButtonLabel(), l10n_util::GetStringUTF16(IDS_OK));
}

TEST_F(AutofillErrorDialogControllerImplTest,
       VirtualCardEnrollmentTemporaryError) {
  AutofillErrorDialogContext context;
  context.type = AutofillErrorDialogType::kVirtualCardEnrollmentTemporaryError;

  ShowPrompt(context);

  EXPECT_EQ(controller()->GetTitle(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_VIRTUAL_CARD_TEMPORARY_ERROR_TITLE));
  EXPECT_EQ(controller()->GetDescription(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_VIRTUAL_CARD_TEMPORARY_ERROR_DESCRIPTION));
  EXPECT_EQ(controller()->GetButtonLabel(), l10n_util::GetStringUTF16(IDS_OK));
}
#endif  // BUILDFLAG(IS_IOS)

// Param of the AutofillErrorDialogControllerImplTest:
// -- bool server_did_return_decline_details;
class AutofillErrorDialogControllerImplParameterizedTest
    : public AutofillErrorDialogControllerImplTest,
      public testing::WithParamInterface<bool> {};

INSTANTIATE_TEST_SUITE_P(,
                         AutofillErrorDialogControllerImplParameterizedTest,
                         testing::Bool());

TEST_P(AutofillErrorDialogControllerImplParameterizedTest, MetricsTest) {
  base::HistogramTester histogram_tester;
  bool server_did_return_decline_details = GetParam();
  AutofillErrorDialogContext context;
  context.type = AutofillErrorDialogType::kVirtualCardTemporaryError;
  if (server_did_return_decline_details) {
    context.server_returned_title = "test_server_returned_title";
    context.server_returned_description = "test_server_returned_description";
  }

  ShowPrompt(context);

  if (server_did_return_decline_details) {
    EXPECT_EQ(controller()->GetTitle(),
              base::UTF8ToUTF16(*context.server_returned_title));
    EXPECT_EQ(controller()->GetDescription(),
              base::UTF8ToUTF16(*context.server_returned_description));
  }

  // Verify that the metric for shown is incremented.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ErrorDialogShown",
      AutofillErrorDialogType::kVirtualCardTemporaryError, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.ErrorDialogShown.WithServerText",
      AutofillErrorDialogType::kVirtualCardTemporaryError,
      /*expected_count=*/server_did_return_decline_details ? 1 : 0);
}

}  // namespace autofill
