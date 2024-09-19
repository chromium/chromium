// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/payments/editor_view_controller.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/validating_textfield.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/test_region_data_loader.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/ui/address_combobox_model.h"
#include "components/payments/content/payment_request.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/core/features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/styled_label.h"

namespace payments {

namespace {

const base::Time kJanuary2017 =
    base::Time::FromSecondsSinceUnixEpoch(1484505871);
const base::Time kJune2017 = base::Time::FromSecondsSinceUnixEpoch(1497552271);

}  // namespace

// This test suite is flaky on desktop platforms (crbug.com/1073972) and tests
// UI that is soon to be deprecated, so it is disabled.
class DISABLED_PaymentRequestCreditCardEditorTest
    : public PaymentRequestBrowserTestBase {
 protected:
  DISABLED_PaymentRequestCreditCardEditorTest() = default;

  PersonalDataLoadedObserverMock personal_data_observer_;
};

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestCreditCardEditorTest,
                       EnteringValidData) {
  NavigateTo("/payment_request_no_shipping_test.html");
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  InvokePaymentRequestUI();

  // No apps are available.
  PaymentRequest* request = GetPaymentRequests().front();
  EXPECT_EQ(0U, request->state()->available_apps().size());
  EXPECT_EQ(nullptr, request->state()->selected_app());

  // But there must be at least one address available for billing.
  autofill::AutofillProfile billing_profile(autofill::test::GetFullProfile());
  AddAutofillProfile(billing_profile);

  OpenCreditCardEditorScreen();

  SetEditorTextfieldValue(u"Bob Jones", autofill::CREDIT_CARD_NAME_FULL);
  SetEditorTextfieldValue(u" 4111 1111-1111 1111-",
                          autofill::CREDIT_CARD_NUMBER);
  SetComboboxValue(u"05", autofill::CREDIT_CARD_EXP_MONTH);
  SetComboboxValue(u"2026", autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR);
  SelectBillingAddress(billing_profile.guid());

  // Verifying the data is in the DB.
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  ResetEventWaiter(DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);
  data_loop.Run();

  EXPECT_EQ(
      1u,
      personal_data_manager->payments_data_manager().GetCreditCards().size());
  const autofill::CreditCard* credit_card =
      personal_data_manager->payments_data_manager().GetCreditCards()[0];
  EXPECT_EQ(5, credit_card->expiration_month());
  EXPECT_EQ(2026, credit_card->expiration_year());
  EXPECT_EQ(u"1111", credit_card->LastFourDigits());
  EXPECT_EQ(u"Bob Jones",
            credit_card->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL));

  // One app is available and selected.
  EXPECT_EQ(1U, request->state()->available_apps().size());
  EXPECT_EQ(request->state()->available_apps().back().get(),
            request->state()->selected_app());
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestCreditCardEditorTest,
                       EnterConfirmsValidData) {
  NavigateTo("/payment_request_no_shipping_test.html");
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  // An address is needed so that the UI can choose it as a billing address.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);

  InvokePaymentRequestUI();

  // No apps are available.
  PaymentRequest* request = GetPaymentRequests().front();
  EXPECT_EQ(0U, request->state()->available_apps().size());
  EXPECT_EQ(nullptr, request->state()->selected_app());

  OpenCreditCardEditorScreen();

  SetEditorTextfieldValue(u"Bob Jones", autofill::CREDIT_CARD_NAME_FULL);
  SetEditorTextfieldValue(u"4111111111111111", autofill::CREDIT_CARD_NUMBER);
  SetComboboxValue(u"05", autofill::CREDIT_CARD_EXP_MONTH);
  SetComboboxValue(u"2026", autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR);
  SelectBillingAddress(billing_address.guid());

  // Verifying the data is in the DB.
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  ResetEventWaiter(DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  views::View* editor_sheet = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::CREDIT_CARD_EDITOR_SHEET));
  editor_sheet->AcceleratorPressed(
      ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));
  data_loop.Run();

  EXPECT_EQ(
      1u,
      personal_data_manager->payments_data_manager().GetCreditCards().size());
  const autofill::CreditCard* credit_card =
      personal_data_manager->payments_data_manager().GetCreditCards()[0];
  EXPECT_EQ(5, credit_card->expiration_month());
  EXPECT_EQ(2026, credit_card->expiration_year());
  EXPECT_EQ(u"1111", credit_card->LastFourDigits());
  EXPECT_EQ(u"Bob Jones",
            credit_card->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL));

  // One app is available and selected.
  EXPECT_EQ(1U, request->state()->available_apps().size());
  EXPECT_EQ(request->state()->available_apps().back().get(),
            request->state()->selected_app());
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestCreditCardEditorTest,
                       CancelFromEditor) {
  NavigateTo("/payment_request_no_shipping_test.html");
  InvokePaymentRequestUI();

  OpenCreditCardEditorScreen();

  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

  ClickOnDialogViewAndWait(DialogViewID::CANCEL_BUTTON,
                           /*wait_for_animation=*/false);
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestCreditCardEditorTest,
                       EnteringExpiredCard) {
  NavigateTo("/payment_request_no_shipping_test.html");
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  autofill::AutofillProfile billing_profile(autofill::test::GetFullProfile());
  AddAutofillProfile(billing_profile);

  InvokePaymentRequestUI();

  OpenCreditCardEditorScreen();

  SetEditorTextfieldValue(u"Bob Jones", autofill::CREDIT_CARD_NAME_FULL);
  SetEditorTextfieldValue(u"4111111111111111", autofill::CREDIT_CARD_NUMBER);

  SelectBillingAddress(billing_profile.guid());

  // The card is expired.
  SetComboboxValue(u"01", autofill::CREDIT_CARD_EXP_MONTH);
  SetComboboxValue(u"2017", autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR);

  EXPECT_FALSE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NAME_FULL));
  EXPECT_FALSE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NUMBER));
  EXPECT_TRUE(IsEditorComboboxInvalid(autofill::CREDIT_CARD_EXP_MONTH));
  EXPECT_TRUE(IsEditorComboboxInvalid(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRED),
            GetErrorLabelForType(autofill::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR));

  views::View* save_button = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::EDITOR_SAVE_BUTTON));

  EXPECT_FALSE(save_button->GetEnabled());
  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);

  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  EXPECT_EQ(
      0u,
      personal_data_manager->payments_data_manager().GetCreditCards().size());

  SetComboboxValue(u"12", autofill::CREDIT_CARD_EXP_MONTH);

  EXPECT_TRUE(save_button->GetEnabled());
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestCreditCardEditorTest,
                       EnteringNothingInARequiredField) {
  NavigateTo("/payment_request_no_shipping_test.html");
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  InvokePaymentRequestUI();

  OpenCreditCardEditorScreen();

  // This field is required. Entering nothing and blurring out will show
  // "Required field".
  SetEditorTextfieldValue(u"", autofill::CREDIT_CARD_NUMBER);
  EXPECT_TRUE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NUMBER));
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PREF_EDIT_DIALOG_FIELD_REQUIRED_VALIDATION_MESSAGE),
            GetErrorLabelForType(autofill::CREDIT_CARD_NUMBER));

  // Set the value to something which is not a valid card number. The "invalid
  // card number" string takes precedence over "required field"
  SetEditorTextfieldValue(u"41111111invalidcard", autofill::CREDIT_CARD_NUMBER);
  EXPECT_TRUE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NUMBER));
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PAYMENTS_CARD_NUMBER_INVALID_VALIDATION_MESSAGE),
            GetErrorLabelForType(autofill::CREDIT_CARD_NUMBER));

  // Set the value to a valid number now. No more errors!
  SetEditorTextfieldValue(u"4111111111111111", autofill::CREDIT_CARD_NUMBER);
  EXPECT_FALSE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NUMBER));
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestCreditCardEditorTest,
                       EnteringInvalidCardNumber) {
  NavigateTo("/payment_request_no_shipping_test.html");
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  InvokePaymentRequestUI();

  OpenCreditCardEditorScreen();

  SetEditorTextfieldValue(u"Bob Jones", autofill::CREDIT_CARD_NAME_FULL);
  SetEditorTextfieldValue(u"41111111invalidcard", autofill::CREDIT_CARD_NUMBER);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PAYMENTS_CARD_NUMBER_INVALID_VALIDATION_MESSAGE),
            GetErrorLabelForType(autofill::CREDIT_CARD_NUMBER));
  SetComboboxValue(u"05", autofill::CREDIT_CARD_EXP_MONTH);
  SetComboboxValue(u"2026", autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR);

  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);

  EXPECT_FALSE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NAME_FULL));
  EXPECT_TRUE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NUMBER));
  EXPECT_FALSE(IsEditorComboboxInvalid(autofill::CREDIT_CARD_EXP_MONTH));
  EXPECT_FALSE(IsEditorComboboxInvalid(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR));

  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  EXPECT_EQ(
      0u,
      personal_data_manager->payments_data_manager().GetCreditCards().size());
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestCreditCardEditorTest,
                       EnteringUnsupportedCardType) {
  NavigateTo("/payment_request_no_shipping_test.html");
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  InvokePaymentRequestUI();

  OpenCreditCardEditorScreen();

  SetEditorTextfieldValue(u"Bob Jones", autofill::CREDIT_CARD_NAME_FULL);
  // In this test case, only "visa" and "mastercard" are supported, so entering
  // a MIR card will fail.
  SetEditorTextfieldValue(u"22002222invalidcard", autofill::CREDIT_CARD_NUMBER);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PAYMENTS_VALIDATION_UNSUPPORTED_CREDIT_CARD_TYPE),
            GetErrorLabelForType(autofill::CREDIT_CARD_NUMBER));
  SetComboboxValue(u"05", autofill::CREDIT_CARD_EXP_MONTH);
  SetComboboxValue(u"2026", autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR);

  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);

  EXPECT_FALSE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NAME_FULL));
  EXPECT_TRUE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NUMBER));
  EXPECT_FALSE(IsEditorComboboxInvalid(autofill::CREDIT_CARD_EXP_MONTH));
  EXPECT_FALSE(IsEditorComboboxInvalid(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR));

  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  EXPECT_EQ(
      0u,
      personal_data_manager->payments_data_manager().GetCreditCards().size());
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestCreditCardEditorTest,
                       EnteringInvalidCardNumber_AndFixingIt) {
  NavigateTo("/payment_request_no_shipping_test.html");
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);
  autofill::AutofillProfile billing_profile(autofill::test::GetFullProfile());
  AddAutofillProfile(billing_profile);

  InvokePaymentRequestUI();

  OpenCreditCardEditorScreen();

  SetEditorTextfieldValue(u"Bob Jones", autofill::CREDIT_CARD_NAME_FULL);
  SetEditorTextfieldValue(u"41111111invalidcard", autofill::CREDIT_CARD_NUMBER);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PAYMENTS_CARD_NUMBER_INVALID_VALIDATION_MESSAGE),
            GetErrorLabelForType(autofill::CREDIT_CARD_NUMBER));
  SetComboboxValue(u"05", autofill::CREDIT_CARD_EXP_MONTH);
  SetComboboxValue(u"2026", autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR);
  SelectBillingAddress(billing_profile.guid());

  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);

  EXPECT_FALSE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NAME_FULL));
  EXPECT_TRUE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NUMBER));
  EXPECT_FALSE(IsEditorComboboxInvalid(autofill::CREDIT_CARD_EXP_MONTH));
  EXPECT_FALSE(IsEditorComboboxInvalid(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR));

  // Fixing the card number.
  SetEditorTextfieldValue(u"4111111111111111", autofill::CREDIT_CARD_NUMBER);
  // The error has gone.
  EXPECT_FALSE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NUMBER));

  // Verifying the data is in the DB.
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  ResetEventWaiter(DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);
  data_loop.Run();

  EXPECT_EQ(
      1u,
      personal_data_manager->payments_data_manager().GetCreditCards().size());
  const autofill::CreditCard* credit_card =
      personal_data_manager->payments_data_manager().GetCreditCards()[0];
  EXPECT_EQ(5, credit_card->expiration_month());
  EXPECT_EQ(2026, credit_card->expiration_year());
  EXPECT_EQ(u"1111", credit_card->LastFourDigits());
  EXPECT_EQ(u"Bob Jones",
            credit_card->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL));
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestCreditCardEditorTest,
                       EditingExpiredCard) {
  NavigateTo("/payment_request_no_shipping_test.html");
  // Add expired card.
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_use_count(5U);
  card.set_use_date(kJanuary2017);
  card.SetExpirationMonth(1);
  card.SetExpirationYear(2017);
  autofill::AutofillProfile billing_profile(autofill::test::GetFullProfile());
  AddAutofillProfile(billing_profile);
  card.set_billing_address_id(billing_profile.guid());
  AddCreditCard(card);
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  InvokePaymentRequestUI();

  // Focus expectations are different in Keyboard Accessible mode.
  dialog_view()->GetFocusManager()->SetKeyboardAccessible(false);

  // One app is available, and it's selected because that's allowed for expired
  // credit cards.
  PaymentRequest* request = GetPaymentRequests().front();
  EXPECT_EQ(1U, request->state()->available_apps().size());
  EXPECT_NE(nullptr, request->state()->selected_app());

  OpenPaymentMethodScreen();

  // Opening the credit card editor by clicking the edit button.
  views::View* list_view = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::PAYMENT_METHOD_SHEET_LIST_VIEW));
  EXPECT_TRUE(list_view);
  EXPECT_EQ(1u, list_view->children().size());

  views::View* edit_button = list_view->children().front()->GetViewByID(
      static_cast<int>(DialogViewID::EDIT_ITEM_BUTTON));

  ResetEventWaiter(DialogEvent::CREDIT_CARD_EDITOR_OPENED);
  ClickOnDialogViewAndWait(edit_button);

  EXPECT_EQ(u"Test User",
            GetEditorTextfieldValue(autofill::CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"4111 1111 1111 1111",
            GetEditorTextfieldValue(autofill::CREDIT_CARD_NUMBER));
  EXPECT_EQ(u"01", GetComboboxValue(autofill::CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(u"2017", GetComboboxValue(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR));
  // Should show as expired when the editor opens.
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRED),
            GetErrorLabelForType(autofill::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR));

  views::Combobox* combobox = static_cast<views::Combobox*>(
      dialog_view()->GetViewByID(EditorViewController::GetInputFieldViewId(
          autofill::CREDIT_CARD_EXP_MONTH)));
  EXPECT_TRUE(combobox->HasFocus());

  // Fixing the expiration date.
  SetComboboxValue(u"11", autofill::CREDIT_CARD_EXP_MONTH);

  // Verifying the data is in the DB.
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  ResetEventWaiter(DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);
  data_loop.Run();

  EXPECT_EQ(
      1u,
      personal_data_manager->payments_data_manager().GetCreditCards().size());
  const autofill::CreditCard* credit_card =
      personal_data_manager->payments_data_manager().GetCreditCards()[0];
  EXPECT_EQ(11, credit_card->expiration_month());
  EXPECT_EQ(2017, credit_card->expiration_year());
  // It retains other properties.
  EXPECT_EQ(card.guid(), credit_card->guid());
  EXPECT_EQ(5U, credit_card->use_count());
  EXPECT_EQ(kJanuary2017, credit_card->use_date());
  EXPECT_EQ(u"4111111111111111", credit_card->number());
  EXPECT_EQ(u"Test User",
            credit_card->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL));

  // Still have one app, and it's still selected.
  EXPECT_EQ(1U, request->state()->available_apps().size());
  EXPECT_EQ(request->state()->available_apps().back().get(),
            request->state()->selected_app());
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestCreditCardEditorTest,
                       EditingCardWithoutBillingAddress) {
  NavigateTo("/payment_request_no_shipping_test.html");
  autofill::CreditCard card = autofill::test::GetCreditCard();
  // Make sure to clear billing address.
  card.set_billing_address_id("");
  AddCreditCard(card);

  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);
  autofill::AutofillProfile billing_profile(autofill::test::GetFullProfile());
  AddAutofillProfile(billing_profile);

  InvokePaymentRequestUI();

  // One app is available, but it's not selected.
  PaymentRequest* request = GetPaymentRequests().front();
  EXPECT_EQ(1U, request->state()->available_apps().size());
  EXPECT_EQ(nullptr, request->state()->selected_app());

  OpenPaymentMethodScreen();

  ResetEventWaiter(DialogEvent::CREDIT_CARD_EDITOR_OPENED);
  ClickOnChildInListViewAndWait(/*child_index=*/0, /*num_children=*/1,
                                DialogViewID::PAYMENT_METHOD_SHEET_LIST_VIEW);

  // Proper error shown.
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAYMENTS_BILLING_ADDRESS_REQUIRED),
            GetErrorLabelForType(autofill::ADDRESS_HOME_LINE1));

  // Fixing the billing address.
  SelectBillingAddress(billing_profile.guid());

  // Verifying the data is in the DB.
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  ResetEventWaiter(DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);
  data_loop.Run();

  EXPECT_EQ(
      1u,
      personal_data_manager->payments_data_manager().GetCreditCards().size());
  const autofill::CreditCard* credit_card =
      personal_data_manager->payments_data_manager().GetCreditCards()[0];
  EXPECT_EQ(billing_profile.guid(), credit_card->billing_address_id());
  // It retains other properties.
  EXPECT_EQ(card.guid(), credit_card->guid());
  EXPECT_EQ(u"4111111111111111", credit_card->number());
  EXPECT_EQ(u"Test User",
            credit_card->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL));

  // Still have one app, but now it's selected.
  EXPECT_EQ(1U, request->state()->available_apps().size());
  EXPECT_EQ(request->state()->available_apps().back().get(),
            request->state()->selected_app());
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestCreditCardEditorTest,
                       EditingCardWithoutCardholderName) {
  NavigateTo("/payment_request_no_shipping_test.html");
  autofill::CreditCard card = autofill::test::GetCreditCard();
  autofill::AutofillProfile billing_profile(autofill::test::GetFullProfile());
  AddAutofillProfile(billing_profile);
  card.set_billing_address_id(billing_profile.guid());
  // Clear the name.
  card.SetInfo(autofill::CREDIT_CARD_NAME_FULL, std::u16string(), "en-US");
  AddCreditCard(card);

  InvokePaymentRequestUI();

  // One app is available, but it's not selected.
  PaymentRequest* request = GetPaymentRequests().front();
  EXPECT_EQ(1U, request->state()->available_apps().size());
  EXPECT_EQ(nullptr, request->state()->selected_app());

  OpenPaymentMethodScreen();

  ResetEventWaiter(DialogEvent::CREDIT_CARD_EDITOR_OPENED);
  ClickOnChildInListViewAndWait(/*child_index=*/0, /*num_children=*/1,
                                DialogViewID::PAYMENT_METHOD_SHEET_LIST_VIEW);

  // Proper error shown.
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PREF_EDIT_DIALOG_FIELD_REQUIRED_VALIDATION_MESSAGE),
            GetErrorLabelForType(autofill::CREDIT_CARD_NAME_FULL));

  // Fixing the name.
  SetEditorTextfieldValue(u"Bob Newname", autofill::CREDIT_CARD_NAME_FULL);

  // Verifying the data is in the DB.
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  ResetEventWaiter(DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);
  data_loop.Run();

  EXPECT_EQ(
      1u,
      personal_data_manager->payments_data_manager().GetCreditCards().size());
  const autofill::CreditCard* credit_card =
      personal_data_manager->payments_data_manager().GetCreditCards()[0];
  EXPECT_EQ(u"Bob Newname",
            credit_card->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL));
  // It retains other properties.
  EXPECT_EQ(card.guid(), credit_card->guid());
  EXPECT_EQ(u"4111111111111111", credit_card->number());
  EXPECT_EQ(billing_profile.guid(), credit_card->billing_address_id());

  // Still have one app, but now it's selected.
  EXPECT_EQ(1U, request->state()->available_apps().size());
  EXPECT_EQ(request->state()->available_apps().back().get(),
            request->state()->selected_app());
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestCreditCardEditorTest,
                       ChangeCardholderName) {
  NavigateTo("/payment_request_no_shipping_test.html");
  autofill::AutofillProfile billing_profile(autofill::test::GetFullProfile());
  AddAutofillProfile(billing_profile);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  // Don't set billing address yet, so we can simply click on list view to edit.
  card.set_billing_address_id("");
  AddCreditCard(card);

  InvokePaymentRequestUI();

  // One app is available, it is not selected, but is properly named.
  PaymentRequest* request = GetPaymentRequests().front();
  EXPECT_EQ(1U, request->state()->available_apps().size());
  EXPECT_EQ(nullptr, request->state()->selected_app());
  EXPECT_EQ(card.GetInfo(autofill::CREDIT_CARD_NAME_FULL,
                         request->state()->GetApplicationLocale()),
            request->state()->available_apps()[0]->GetSublabel());

  OpenPaymentMethodScreen();

  ResetEventWaiter(DialogEvent::CREDIT_CARD_EDITOR_OPENED);
  ClickOnChildInListViewAndWait(/*child_index=*/0, /*num_children=*/1,
                                DialogViewID::PAYMENT_METHOD_SHEET_LIST_VIEW);
  // Change the name.
  SetEditorTextfieldValue(u"Bob the second", autofill::CREDIT_CARD_NAME_FULL);
  // Make the card valid.
  SelectBillingAddress(billing_profile.guid());

  // Verifying the data is in the DB.
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  ResetEventWaiter(DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);
  data_loop.Run();

  // One app is available, is selected, and is properly named.
  EXPECT_EQ(1U, request->state()->available_apps().size());
  EXPECT_NE(nullptr, request->state()->selected_app());
  EXPECT_EQ(u"Bob the second", request->state()->selected_app()->GetSublabel());
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestCreditCardEditorTest,
                       CreateNewBillingAddress) {
  NavigateTo("/payment_request_no_shipping_test.html");
  autofill::CreditCard card = autofill::test::GetCreditCard();
  // Make sure to clear billing address and have none available.
  card.set_billing_address_id("");
  AddCreditCard(card);

  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  InvokePaymentRequestUI();

  // One app is available, but it's not selected.
  PaymentRequest* request = GetPaymentRequests().front();
  EXPECT_EQ(1U, request->state()->available_apps().size());
  EXPECT_EQ(nullptr, request->state()->selected_app());

  OpenPaymentMethodScreen();

  ResetEventWaiter(DialogEvent::CREDIT_CARD_EDITOR_OPENED);
  ClickOnChildInListViewAndWait(/*child_index=*/0, /*num_children=*/1,
                                DialogViewID::PAYMENT_METHOD_SHEET_LIST_VIEW);
  // Billing address combobox must be disabled since there are no saved address.
  views::View* billing_address_combobox = dialog_view()->GetViewByID(
      EditorViewController::GetInputFieldViewId(autofill::ADDRESS_HOME_LINE1));
  ASSERT_NE(nullptr, billing_address_combobox);
  EXPECT_FALSE(billing_address_combobox->GetEnabled());

  // Add some region data to load synchonously.
  autofill::TestRegionDataLoader test_region_data_loader_;
  SetRegionDataLoader(&test_region_data_loader_);
  test_region_data_loader_.set_synchronous_callback(true);
  std::vector<std::pair<std::string, std::string>> regions1;
  regions1.push_back(std::make_pair("AL", "Alabama"));
  regions1.push_back(std::make_pair("CA", "California"));
  test_region_data_loader_.SetRegionData(regions1);

  // Click to open the address editor
  ResetEventWaiter(DialogEvent::SHIPPING_ADDRESS_EDITOR_OPENED);
  ClickOnDialogViewAndWait(DialogViewID::ADD_BILLING_ADDRESS_BUTTON);

  // Set valid address values.
  SetEditorTextfieldValue(u"Bob", autofill::NAME_FULL);
  SetEditorTextfieldValue(u"42 BobStreet",
                          autofill::ADDRESS_HOME_STREET_ADDRESS);
  SetEditorTextfieldValue(u"BobCity", autofill::ADDRESS_HOME_CITY);
  SetComboboxValue(u"California", autofill::ADDRESS_HOME_STATE);
  SetEditorTextfieldValue(u"BobZip", autofill::ADDRESS_HOME_ZIP);
  SetEditorTextfieldValue(u"+15755555555", autofill::PHONE_HOME_WHOLE_NUMBER);

  // Come back to credit card editor.
  ResetEventWaiter(DialogEvent::BACK_NAVIGATION);
  ClickOnDialogViewAndWait(DialogViewID::SAVE_ADDRESS_BUTTON);

  // The billing address must be properly selected and valid.
  views::Combobox* billing_combobox = static_cast<views::Combobox*>(
      dialog_view()->GetViewByID(EditorViewController::GetInputFieldViewId(
          autofill::ADDRESS_HOME_LINE1)));
  ASSERT_NE(nullptr, billing_combobox);
  EXPECT_FALSE(billing_combobox->GetInvalid());
  EXPECT_TRUE(billing_combobox->GetEnabled());

  // And then save credit card state and come back to payment sheet.
  ResetEventWaiter(DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION);

  // Verifying the data is in the DB.
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);
  data_loop.Run();

  // Still have one app, but now it's selected.
  EXPECT_EQ(1U, request->state()->available_apps().size());
  EXPECT_EQ(request->state()->available_apps().back().get(),
            request->state()->selected_app());
  EXPECT_TRUE(request->state()->selected_app()->IsCompleteForPayment());
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestCreditCardEditorTest,
                       NonexistentBillingAddres) {
  NavigateTo("/payment_request_no_shipping_test.html");
  autofill::CreditCard card = autofill::test::GetCreditCard();
  // Set a billing address that is not yet added to the personal data.
  autofill::AutofillProfile billing_profile(autofill::test::GetFullProfile());
  card.set_billing_address_id(billing_profile.guid());
  AddCreditCard(card);

  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  InvokePaymentRequestUI();

  // One app is available, but it's not selected.
  PaymentRequest* request = GetPaymentRequests().front();
  EXPECT_EQ(1U, request->state()->available_apps().size());
  EXPECT_EQ(nullptr, request->state()->selected_app());

  // Now add the billing address to the personal data.
  AddAutofillProfile(billing_profile);

  // Go back and re-invoke.
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  ClickOnDialogViewAndWait(DialogViewID::CANCEL_BUTTON,
                           /*wait_for_animation=*/false);
  InvokePaymentRequestUI();

  // Still have one app, but now it's selected.
  request = GetPaymentRequests().front();
  EXPECT_EQ(1U, request->state()->available_apps().size());
  EXPECT_EQ(request->state()->available_apps().back().get(),
            request->state()->selected_app());
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestCreditCardEditorTest,
                       EnteringEmptyData) {
  NavigateTo("/payment_request_no_shipping_test.html");
  InvokePaymentRequestUI();

  OpenCreditCardEditorScreen();

  // Setting empty data and unfocusing a required textfield will make it
  // invalid.
  SetEditorTextfieldValue(u"", autofill::CREDIT_CARD_NAME_FULL);

  ValidatingTextfield* textfield = static_cast<ValidatingTextfield*>(
      dialog_view()->GetViewByID(EditorViewController::GetInputFieldViewId(
          autofill::CREDIT_CARD_NAME_FULL)));
  EXPECT_TRUE(textfield);
  EXPECT_FALSE(textfield->IsValid());
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestCreditCardEditorTest,
                       DoneButtonDisabled) {
  NavigateTo("/payment_request_no_shipping_test.html");
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);
  InvokePaymentRequestUI();

  autofill::AutofillProfile billing_profile(autofill::test::GetFullProfile());
  AddAutofillProfile(billing_profile);

  OpenCreditCardEditorScreen();

  views::View* save_button = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::EDITOR_SAVE_BUTTON));

  EXPECT_FALSE(save_button->GetEnabled());

  // Set all fields but one:
  SetEditorTextfieldValue(u"Bob Jones", autofill::CREDIT_CARD_NAME_FULL);
  SetEditorTextfieldValue(u"4111111111111111", autofill::CREDIT_CARD_NUMBER);
  SetComboboxValue(u"05", autofill::CREDIT_CARD_EXP_MONTH);
  SetComboboxValue(u"2026", autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR);

  // Still disabled.
  EXPECT_FALSE(save_button->GetEnabled());

  // Set the last field.
  SelectBillingAddress(billing_profile.guid());

  // Should be good to go.
  EXPECT_TRUE(save_button->GetEnabled());

  // Change a field to something invalid, to make sure it works both ways.
  SetEditorTextfieldValue(u"Ni!", autofill::CREDIT_CARD_NUMBER);

  // Back to being disabled.
  EXPECT_FALSE(save_button->GetEnabled());
}

IN_PROC_BROWSER_TEST_F(DISABLED_PaymentRequestCreditCardEditorTest,
                       EnteringValidDataInIncognito) {
  SetIncognito();
  NavigateTo("/payment_request_no_shipping_test.html");
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  InvokePaymentRequestUI();

  // No apps are available.
  PaymentRequest* request = GetPaymentRequests().front();
  EXPECT_EQ(0U, request->state()->available_apps().size());
  EXPECT_EQ(nullptr, request->state()->selected_app());

  // But there must be at least one address available for billing.
  autofill::AutofillProfile billing_profile(autofill::test::GetFullProfile());
  AddAutofillProfile(billing_profile);

  OpenCreditCardEditorScreen();

  SetEditorTextfieldValue(u"Bob Jones", autofill::CREDIT_CARD_NAME_FULL);
  SetEditorTextfieldValue(u" 4111 1111-1111 1111-",
                          autofill::CREDIT_CARD_NUMBER);
  SetComboboxValue(u"05", autofill::CREDIT_CARD_EXP_MONTH);
  SetComboboxValue(u"2026", autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR);
  SelectBillingAddress(billing_profile.guid());

  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  ResetEventWaiter(DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION);

  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(0);
  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);

  // Since this is incognito, the credit card shouldn't have been added to the
  // PersonalDataManager but it should be available in available_apps.
  EXPECT_EQ(
      0U,
      personal_data_manager->payments_data_manager().GetCreditCards().size());

  // One app is available and selected.
  EXPECT_EQ(1U, request->state()->available_apps().size());
  EXPECT_EQ(request->state()->available_apps().back().get(),
            request->state()->selected_app());
}

}  // namespace payments
