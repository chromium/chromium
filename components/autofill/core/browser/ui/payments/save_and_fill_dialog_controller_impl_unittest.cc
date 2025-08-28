// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_controller_impl.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/metrics/payments/save_and_fill_metrics.h"
#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_view.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

class SaveAndFillDialogControllerImplTest : public testing::Test {
 protected:
  SaveAndFillDialogControllerImplTest() = default;
  ~SaveAndFillDialogControllerImplTest() override = default;

  void SetDialogState(SaveAndFillDialogState dialog_state) {
    controller_->dialog_state_ = dialog_state;
  }

  void SetUp() override {
    controller_ = std::make_unique<SaveAndFillDialogControllerImpl>();

    EXPECT_CALL(create_and_show_view_callback, Run())
        .WillOnce(testing::Return(std::make_unique<SaveAndFillDialogView>()));

    controller_->ShowLocalDialog(create_and_show_view_callback.Get(),
                                 card_save_and_fill_dialog_callback_.Get());
  }

  SaveAndFillDialogControllerImpl* controller() const {
    return controller_.get();
  }

 protected:
  std::unique_ptr<SaveAndFillDialogControllerImpl> controller_;
  base::MockCallback<
      base::OnceCallback<std::unique_ptr<SaveAndFillDialogView>()>>
      create_and_show_view_callback;
  base::MockCallback<
      payments::PaymentsAutofillClient::CardSaveAndFillDialogCallback>
      card_save_and_fill_dialog_callback_;
};

namespace {

std::u16string GenerateExpirationDateString(
    const base::Time::Exploded& base_time_exploded,
    int month_offset,
    int year_offset) {
  int target_month = base_time_exploded.month + month_offset;
  int target_year = base_time_exploded.year + year_offset;

  while (target_month <= 0) {
    target_month += 12;
    target_year--;
  }
  while (target_month > 12) {
    target_month -= 12;
    target_year++;
  }

  std::string month_str = base::StringPrintf("%02d", target_month);
  std::string year_str = base::StringPrintf("%02d", target_year % 100);

  return base::UTF8ToUTF16(month_str + "/" + year_str);
}

}  // namespace

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(SaveAndFillDialogControllerImplTest, CorrectStringsAreReturned) {
  EXPECT_EQ(controller()->GetWindowTitle(),
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_TITLE));

  SetDialogState(SaveAndFillDialogState::kLocalDialog);
  EXPECT_EQ(controller()->GetExplanatoryMessage(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_EXPLANATION_LOCAL));

  EXPECT_EQ(controller()->GetCardNumberLabel(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_CARD_NUMBER_LABEL));

  EXPECT_EQ(
      controller()->GetCvcLabel(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_CVC_LABEL));

  EXPECT_EQ(controller()->GetExpirationDateLabel(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_EXPIRATION_DATE_LABEL));

  EXPECT_EQ(controller()->GetNameOnCardLabel(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_NAME_ON_CARD_LABEL));

  EXPECT_EQ(
      controller()->GetAcceptButtonText(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_ACCEPT));

  EXPECT_EQ(controller()->GetInvalidCardNumberErrorMessage(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_INVALID_CARD_NUMBER));

  // Test for upload Save and Fill explanatory message.
  SetDialogState(SaveAndFillDialogState::kUploadDialog);
  EXPECT_EQ(controller()->GetExplanatoryMessage(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_EXPLANATION_UPLOAD));
}

TEST_F(SaveAndFillDialogControllerImplTest, FormatExpirationDateInput) {
  size_t new_cursor_position;

  // `old_cursor_position = 2` simulates cursor being after '2' when '3' is
  // typed.
  EXPECT_EQ(controller()->FormatExpirationDateInput(
                /*input=*/u"123", /*old_cursor_position=*/2,
                /*new_cursor_position=*/new_cursor_position),
            u"12/3");
  EXPECT_EQ(new_cursor_position, 3U);

  EXPECT_EQ(controller()->FormatExpirationDateInput(
                /*input=*/u"12/34", /*old_cursor_position=*/5,
                /*new_cursor_position=*/new_cursor_position),
            u"12/34");
  EXPECT_EQ(new_cursor_position, 5U);

  // Input is too long and contains non-digits. It should be cleaned and
  // truncated.
  EXPECT_EQ(controller()->FormatExpirationDateInput(
                /*input=*/u"1a23b45", /*old_cursor_position=*/7,
                /*new_cursor_position=*/new_cursor_position),
            u"12/34");
  EXPECT_EQ(new_cursor_position, 5U);
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

TEST_F(SaveAndFillDialogControllerImplTest, IsValidCvc) {
  // Empty CVC is valid since it's optional.
  EXPECT_TRUE(controller()->IsValidCvc(u""));

  // Valid 3-digit CVC.
  EXPECT_TRUE(controller()->IsValidCvc(u"123"));

  // Valid 4-digit CVC.
  EXPECT_TRUE(controller()->IsValidCvc(u"1234"));

  // CVC with less than 3 digits is invalid.
  EXPECT_FALSE(controller()->IsValidCvc(u"12"));
  EXPECT_FALSE(controller()->IsValidCvc(u"1"));

  // CVC with more than 4 digits is invalid.
  EXPECT_FALSE(controller()->IsValidCvc(u"12345"));

  // CVC with non-digit characters is invalid.
  EXPECT_FALSE(controller()->IsValidCvc(u"12A"));
  EXPECT_FALSE(controller()->IsValidCvc(u"ABC"));
  EXPECT_FALSE(controller()->IsValidCvc(u"1 3"));
}

TEST_F(SaveAndFillDialogControllerImplTest, IsValidExpirationDate) {
  base::Time::Exploded now_exploded;
  AutofillClock::Now().LocalExplode(&now_exploded);

  // Expiration date is required.
  EXPECT_FALSE(controller()->IsValidExpirationDate(u""));

  // Invalid cases due to month value.
  EXPECT_FALSE(controller()->IsValidExpirationDate(u"00/26"));
  EXPECT_FALSE(controller()->IsValidExpirationDate(u"13/26"));
  EXPECT_FALSE(controller()->IsValidExpirationDate(u"88/26"));

  // Expired a year ago.
  EXPECT_FALSE(controller()->IsValidExpirationDate(GenerateExpirationDateString(
      now_exploded, /*month_offset=*/0, /*year_offset=*/-1)));
  // Expired a month ago.
  EXPECT_FALSE(controller()->IsValidExpirationDate(GenerateExpirationDateString(
      now_exploded, /*month_offset=*/-1, /*year_offset=*/0)));

  // One month from now.
  EXPECT_TRUE(controller()->IsValidExpirationDate(GenerateExpirationDateString(
      now_exploded, /*month_offset=*/1, /*year_offset=*/0)));
  // Five years from now.
  EXPECT_TRUE(controller()->IsValidExpirationDate(GenerateExpirationDateString(
      now_exploded, /*month_offset=*/0, /*year_offset=*/5)));
}

TEST_F(SaveAndFillDialogControllerImplTest, IsValidNameOnCard) {
  EXPECT_TRUE(controller()->IsValidNameOnCard(u"John Doe"));
  EXPECT_TRUE(controller()->IsValidNameOnCard(u"Jane A. Smith-Jones"));
  EXPECT_TRUE(controller()->IsValidNameOnCard(u"O'Malley"));

  // The name on card field is required for this flow so an empty name is
  // considered invalid.
  EXPECT_FALSE(controller()->IsValidNameOnCard(u""));
  EXPECT_FALSE(controller()->IsValidNameOnCard(u"John123"));
  EXPECT_FALSE(controller()->IsValidNameOnCard(u"Invalid@Name"));
  EXPECT_FALSE(
      controller()->IsValidNameOnCard(u"This name is way too long for a card"));
}

TEST_F(SaveAndFillDialogControllerImplTest,
       Metrics_DialogResult_AcceptedWithCvc) {
  base::HistogramTester histogram_tester;
  payments::PaymentsAutofillClient::UserProvidedCardSaveAndFillDetails details;
  details.security_code = u"123";

  EXPECT_CALL(card_save_and_fill_dialog_callback_,
              Run(payments::PaymentsAutofillClient::
                      CardSaveAndFillDialogUserDecision::kAccepted,
                  testing::_));

  controller()->OnUserAcceptedDialog(details);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveAndFill.DialogResult",
      autofill_metrics::SaveAndFillDialogResult::kAcceptedWithCvc,
      /*expected_bucket_count=*/1);
}

TEST_F(SaveAndFillDialogControllerImplTest,
       Metrics_DialogResult_AcceptedWithoutCvc) {
  base::HistogramTester histogram_tester;
  payments::PaymentsAutofillClient::UserProvidedCardSaveAndFillDetails details;
  details.security_code = u"";

  EXPECT_CALL(card_save_and_fill_dialog_callback_,
              Run(payments::PaymentsAutofillClient::
                      CardSaveAndFillDialogUserDecision::kAccepted,
                  testing::_));

  controller()->OnUserAcceptedDialog({});

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveAndFill.DialogResult",
      autofill_metrics::SaveAndFillDialogResult::kAcceptedWithoutCvc,
      /*expected_bucket_count=*/1);
}

TEST_F(SaveAndFillDialogControllerImplTest, Metrics_DialogResult_Canceled) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(card_save_and_fill_dialog_callback_,
              Run(payments::PaymentsAutofillClient::
                      CardSaveAndFillDialogUserDecision::kDeclined,
                  testing::_));

  controller()->OnUserCanceledDialog();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveAndFill.DialogResult",
      autofill_metrics::SaveAndFillDialogResult::kCanceled,
      /*expected_bucket_count=*/1);
}

TEST_F(SaveAndFillDialogControllerImplTest, Metrics_DialogShown_Local) {
  base::HistogramTester histogram_tester;
  auto controller = std::make_unique<SaveAndFillDialogControllerImpl>();
  base::MockCallback<
      base::OnceCallback<std::unique_ptr<SaveAndFillDialogView>()>>
      create_and_show_view_callback;
  base::MockCallback<
      payments::PaymentsAutofillClient::CardSaveAndFillDialogCallback>
      card_save_and_fill_dialog_callback;

  EXPECT_CALL(create_and_show_view_callback, Run())
      .WillOnce(testing::Return(std::make_unique<SaveAndFillDialogView>()));

  controller->ShowLocalDialog(create_and_show_view_callback.Get(),
                              card_save_and_fill_dialog_callback.Get());

  histogram_tester.ExpectUniqueSample("Autofill.SaveAndFill.DialogShown.Local",
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
}

TEST_F(SaveAndFillDialogControllerImplTest, Metrics_DialogShown_Upload) {
  base::HistogramTester histogram_tester;
  auto controller = std::make_unique<SaveAndFillDialogControllerImpl>();
  base::MockCallback<
      base::OnceCallback<std::unique_ptr<SaveAndFillDialogView>()>>
      create_and_show_view_callback;
  base::MockCallback<
      payments::PaymentsAutofillClient::CardSaveAndFillDialogCallback>
      card_save_and_fill_dialog_callback;

  EXPECT_CALL(create_and_show_view_callback, Run())
      .WillOnce(testing::Return(std::make_unique<SaveAndFillDialogView>()));

  controller->ShowUploadDialog({}, create_and_show_view_callback.Get(),
                               card_save_and_fill_dialog_callback.Get());

  histogram_tester.ExpectUniqueSample("Autofill.SaveAndFill.DialogShown.Upload",
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
}

}  // namespace autofill
