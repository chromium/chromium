// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace payments {

namespace {

const char16_t kNameFull[] = u"Kirby Puckett";
const char16_t kPhoneNumber[] = u"6515558946";
const char16_t kPhoneNumberInvalid[] = u"123";
const char16_t kEmailAddress[] = u"kirby@example.test";
const char16_t kEmailAddressInvalid[] = u"kirby";

std::string GetLocale() {
  return g_browser_process->GetApplicationLocale();
}

}  // namespace

#if BUILDFLAG(IS_MAC)
// Entire test suite is flaky on MacOS: https://crbug.com/1164438
#define MAYBE_PaymentRequestContactInfoEditorTest \
  DISABLED_PaymentRequestContactInfoEditorTest
#else
#define MAYBE_PaymentRequestContactInfoEditorTest \
  PaymentRequestContactInfoEditorTest
#endif

class MAYBE_PaymentRequestContactInfoEditorTest
    : public PaymentRequestBrowserTestBase {
 protected:
  MAYBE_PaymentRequestContactInfoEditorTest() = default;

  PersonalDataLoadedObserverMock personal_data_observer_;
};

IN_PROC_BROWSER_TEST_F(MAYBE_PaymentRequestContactInfoEditorTest, HappyPath) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_contact_details_test.html");

  // Show a Payment Request.
  InvokePaymentRequestUIWithJs(
      content::JsReplace("buyWithMethods([{supportedMethods:$1}"
                         ", {supportedMethods:$2}]);",
                         a_method_name, b_method_name));

  OpenContactInfoEditorScreen();

  SetEditorTextfieldValue(kNameFull, autofill::NAME_FULL);
  SetEditorTextfieldValue(kPhoneNumber, autofill::PHONE_HOME_WHOLE_NUMBER);
  SetEditorTextfieldValue(kEmailAddress, autofill::EMAIL_ADDRESS);

  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);
  data_loop.Run();

  personal_data_manager->RemoveObserver(&personal_data_observer_);
  ASSERT_EQ(1UL,
            personal_data_manager->address_data_manager().GetProfiles().size());
  const autofill::AutofillProfile* profile =
      personal_data_manager->address_data_manager().GetProfiles()[0];
  DCHECK(profile);

  EXPECT_EQ(kNameFull, profile->GetInfo(autofill::NAME_FULL, GetLocale()));
  EXPECT_EQ(u"16515558946",
            profile->GetInfo(autofill::PHONE_HOME_WHOLE_NUMBER, GetLocale()));
  EXPECT_EQ(kEmailAddress,
            profile->GetInfo(autofill::EMAIL_ADDRESS, GetLocale()));

  PaymentRequest* request = GetPaymentRequests().front();
  EXPECT_EQ(1U, request->state()->contact_profiles().size());
  EXPECT_EQ(request->state()->contact_profiles().back(),
            request->state()->selected_contact_profile());
}

IN_PROC_BROWSER_TEST_F(MAYBE_PaymentRequestContactInfoEditorTest,
                       EnterAcceleratorHappyPath) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_contact_details_test.html");

  // Show a Payment Request.
  InvokePaymentRequestUIWithJs(
      content::JsReplace("buyWithMethods([{supportedMethods:$1}"
                         ", {supportedMethods:$2}]);",
                         a_method_name, b_method_name));

  OpenContactInfoEditorScreen();

  SetEditorTextfieldValue(kNameFull, autofill::NAME_FULL);
  SetEditorTextfieldValue(kPhoneNumber, autofill::PHONE_HOME_WHOLE_NUMBER);
  SetEditorTextfieldValue(kEmailAddress, autofill::EMAIL_ADDRESS);

  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  views::View* editor_sheet = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::CONTACT_INFO_EDITOR_SHEET));
  EXPECT_TRUE(editor_sheet->AcceleratorPressed(
      ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE)));
  data_loop.Run();

  personal_data_manager->RemoveObserver(&personal_data_observer_);
  ASSERT_EQ(1UL,
            personal_data_manager->address_data_manager().GetProfiles().size());
  const autofill::AutofillProfile* profile =
      personal_data_manager->address_data_manager().GetProfiles()[0];
  DCHECK(profile);

  EXPECT_EQ(kNameFull, profile->GetInfo(autofill::NAME_FULL, GetLocale()));
  EXPECT_EQ(u"16515558946",
            profile->GetInfo(autofill::PHONE_HOME_WHOLE_NUMBER, GetLocale()));
  EXPECT_EQ(kEmailAddress,
            profile->GetInfo(autofill::EMAIL_ADDRESS, GetLocale()));
}

IN_PROC_BROWSER_TEST_F(MAYBE_PaymentRequestContactInfoEditorTest, Validation) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_contact_details_test.html");

  // Show a Payment Request.
  InvokePaymentRequestUIWithJs(
      content::JsReplace("buyWithMethods([{supportedMethods:$1}"
                         ", {supportedMethods:$2}]);",
                         a_method_name, b_method_name));

  OpenContactInfoEditorScreen();

  // Insert invalid values into fields which have rules more complex than
  // just emptiness, and an empty string into simple required fields.
  SetEditorTextfieldValue(std::u16string(), autofill::NAME_FULL);
  SetEditorTextfieldValue(kPhoneNumberInvalid,
                          autofill::PHONE_HOME_WHOLE_NUMBER);
  SetEditorTextfieldValue(kEmailAddressInvalid, autofill::EMAIL_ADDRESS);

  EXPECT_TRUE(IsEditorTextfieldInvalid(autofill::NAME_FULL));
  EXPECT_TRUE(IsEditorTextfieldInvalid(autofill::PHONE_HOME_WHOLE_NUMBER));
  EXPECT_TRUE(IsEditorTextfieldInvalid(autofill::EMAIL_ADDRESS));

  // Correct the problems.
  SetEditorTextfieldValue(kNameFull, autofill::NAME_FULL);
  SetEditorTextfieldValue(kPhoneNumber, autofill::PHONE_HOME_WHOLE_NUMBER);
  SetEditorTextfieldValue(kEmailAddress, autofill::EMAIL_ADDRESS);

  EXPECT_FALSE(IsEditorTextfieldInvalid(autofill::NAME_FULL));
  EXPECT_FALSE(IsEditorTextfieldInvalid(autofill::PHONE_HOME_WHOLE_NUMBER));
  EXPECT_FALSE(IsEditorTextfieldInvalid(autofill::EMAIL_ADDRESS));

  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);
  data_loop.Run();

  personal_data_manager->RemoveObserver(&personal_data_observer_);
  ASSERT_EQ(1UL,
            personal_data_manager->address_data_manager().GetProfiles().size());
  const autofill::AutofillProfile* profile =
      personal_data_manager->address_data_manager().GetProfiles()[0];
  DCHECK(profile);

  EXPECT_EQ(kNameFull, profile->GetInfo(autofill::NAME_FULL, GetLocale()));
  EXPECT_EQ(u"16515558946",
            profile->GetInfo(autofill::PHONE_HOME_WHOLE_NUMBER, GetLocale()));
  EXPECT_EQ(kEmailAddress,
            profile->GetInfo(autofill::EMAIL_ADDRESS, GetLocale()));
}

IN_PROC_BROWSER_TEST_F(MAYBE_PaymentRequestContactInfoEditorTest,
                       ModifyExisting) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_contact_details_test.html");
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  autofill::AutofillProfile incomplete_profile(
      autofill::i18n_model_definition::kLegacyHierarchyCountryCode);
  incomplete_profile.SetInfo(autofill::NAME_FULL, kNameFull, GetLocale());
  AddAutofillProfile(incomplete_profile);

  // Show a Payment Request.
  InvokePaymentRequestUIWithJs(
      content::JsReplace("buyWithMethods([{supportedMethods:$1}"
                         ", {supportedMethods:$2}]);",
                         a_method_name, b_method_name));

  OpenContactInfoScreen();

  views::View* list_view = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::CONTACT_INFO_SHEET_LIST_VIEW));
  DCHECK(list_view);
  ClickOnDialogViewAndWait(list_view->children().front());

  // Do not set name: This should have been populated when opening the screen.
  EXPECT_EQ(kNameFull, GetEditorTextfieldValue(autofill::NAME_FULL));
  SetEditorTextfieldValue(kPhoneNumber, autofill::PHONE_HOME_WHOLE_NUMBER);
  SetEditorTextfieldValue(kEmailAddress, autofill::EMAIL_ADDRESS);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop save_data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&save_data_loop));
  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);
  save_data_loop.Run();

  personal_data_manager->RemoveObserver(&personal_data_observer_);
  ASSERT_EQ(1UL,
            personal_data_manager->address_data_manager().GetProfiles().size());
  const autofill::AutofillProfile* profile =
      personal_data_manager->address_data_manager().GetProfiles()[0];
  DCHECK(profile);

  EXPECT_EQ(kNameFull, profile->GetInfo(autofill::NAME_FULL, GetLocale()));
  EXPECT_EQ(u"16515558946",
            profile->GetInfo(autofill::PHONE_HOME_WHOLE_NUMBER, GetLocale()));
  EXPECT_EQ(kEmailAddress,
            profile->GetInfo(autofill::EMAIL_ADDRESS, GetLocale()));
}

IN_PROC_BROWSER_TEST_F(MAYBE_PaymentRequestContactInfoEditorTest,
                       ModifyExistingSelectsIt) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_contact_details_test.html");
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  autofill::AutofillProfile incomplete_profile(
      autofill::i18n_model_definition::kLegacyHierarchyCountryCode);
  incomplete_profile.SetInfo(autofill::NAME_FULL, kNameFull, GetLocale());
  AddAutofillProfile(incomplete_profile);

  autofill::AutofillProfile other_incomplete_profile(
      autofill::i18n_model_definition::kLegacyHierarchyCountryCode);
  other_incomplete_profile.SetInfo(autofill::NAME_FULL, u"other", GetLocale());
  AddAutofillProfile(other_incomplete_profile);

  // Show a Payment Request.
  InvokePaymentRequestUIWithJs(
      content::JsReplace("buyWithMethods([{supportedMethods:$1}"
                         ", {supportedMethods:$2}]);",
                         a_method_name, b_method_name));

  OpenContactInfoScreen();

  PaymentRequest* request = GetPaymentRequests().front();

  // No contact profiles are selected because both are incomplete.
  EXPECT_EQ(nullptr, request->state()->selected_contact_profile());

  views::View* list_view = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::CONTACT_INFO_SHEET_LIST_VIEW));
  DCHECK(list_view);
  ClickOnDialogViewAndWait(list_view->children()[1]);

  SetEditorTextfieldValue(kPhoneNumber, autofill::PHONE_HOME_WHOLE_NUMBER);
  SetEditorTextfieldValue(kEmailAddress, autofill::EMAIL_ADDRESS);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop save_data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&save_data_loop));
  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);
  save_data_loop.Run();

  personal_data_manager->RemoveObserver(&personal_data_observer_);
  autofill::AutofillProfile* profile =
      request->state()->selected_contact_profile();
  DCHECK(profile);

  EXPECT_EQ(u"16515558946",
            profile->GetInfo(autofill::PHONE_HOME_WHOLE_NUMBER, GetLocale()));
  EXPECT_EQ(kEmailAddress,
            profile->GetInfo(autofill::EMAIL_ADDRESS, GetLocale()));

  // Expect the newly-completed profile to be selected.
  EXPECT_EQ(2U, request->state()->contact_profiles().size());
  EXPECT_EQ(request->state()->contact_profiles().back(), profile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_PaymentRequestContactInfoEditorTest,
                       HappyPathInIncognito) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  SetIncognito();
  NavigateTo("/payment_request_contact_details_test.html");

  // Show a Payment Request.
  InvokePaymentRequestUIWithJs(
      content::JsReplace("buyWithMethods([{supportedMethods:$1}"
                         ", {supportedMethods:$2}]);",
                         a_method_name, b_method_name));

  OpenContactInfoEditorScreen();

  SetEditorTextfieldValue(kNameFull, autofill::NAME_FULL);
  SetEditorTextfieldValue(kPhoneNumber, autofill::PHONE_HOME_WHOLE_NUMBER);
  SetEditorTextfieldValue(kEmailAddress, autofill::EMAIL_ADDRESS);

  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(0);
  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);

  personal_data_manager->RemoveObserver(&personal_data_observer_);
  // In incognito, the profile should be available in contact_profiles but it
  // shouldn't be saved to the PersonalDataManager.
  ASSERT_EQ(0UL,
            personal_data_manager->address_data_manager().GetProfiles().size());
  PaymentRequest* request = GetPaymentRequests().front();
  EXPECT_EQ(1U, request->state()->contact_profiles().size());
  EXPECT_EQ(request->state()->contact_profiles().back(),
            request->state()->selected_contact_profile());

  autofill::AutofillProfile* profile =
      request->state()->contact_profiles().back();
  DCHECK(profile);

  EXPECT_EQ(kNameFull, profile->GetInfo(autofill::NAME_FULL, GetLocale()));
  EXPECT_EQ(u"16515558946",
            profile->GetInfo(autofill::PHONE_HOME_WHOLE_NUMBER, GetLocale()));
  EXPECT_EQ(kEmailAddress,
            profile->GetInfo(autofill::EMAIL_ADDRESS, GetLocale()));
}

IN_PROC_BROWSER_TEST_F(MAYBE_PaymentRequestContactInfoEditorTest,
                       RetryWithPayerErrors) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_retry_with_payer_errors.html");

  autofill::AutofillProfile contact = autofill::test::GetFullProfile();
  AddAutofillProfile(contact);

  // Show a Payment Request.
  InvokePaymentRequestUIWithJs(
      content::JsReplace("buyWithMethods([{supportedMethods:$1}"
                         ", {supportedMethods:$2}]);",
                         a_method_name, b_method_name));

  // Click on pay.
  EXPECT_TRUE(IsPayButtonEnabled());
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  // Wait for the response to settle.
  ASSERT_TRUE(
      content::ExecJs(GetActiveWebContents(), "processShowResponse();"));

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::SPEC_DONE_UPDATING,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION,
                               DialogEvent::CONTACT_INFO_EDITOR_OPENED});
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(),
                              "retry({"
                              "  payer: {"
                              "    email: 'EMAIL ERROR',"
                              "    name: 'NAME ERROR',"
                              "    phone: 'PHONE ERROR'"
                              "  }"
                              "});"));
  ASSERT_TRUE(WaitForObservedEvent());

  EXPECT_EQ(u"EMAIL ERROR", GetErrorLabelForType(autofill::EMAIL_ADDRESS));
  EXPECT_EQ(u"NAME ERROR", GetErrorLabelForType(autofill::NAME_FULL));
  EXPECT_EQ(u"PHONE ERROR",
            GetErrorLabelForType(autofill::PHONE_HOME_WHOLE_NUMBER));
}

IN_PROC_BROWSER_TEST_F(
    MAYBE_PaymentRequestContactInfoEditorTest,
    RetryWithPayerErrors_HasSameValueButDifferentErrorsShown) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_retry_with_payer_errors.html");

  autofill::AutofillProfile contact = autofill::test::GetFullProfile();
  // Set the same value in both of email and name field.
  contact.SetRawInfo(autofill::EMAIL_ADDRESS, u"johndoe@hades.com");
  contact.SetRawInfo(autofill::NAME_FULL, u"johndoe@hades.com");
  AddAutofillProfile(contact);

  // Show a Payment Request.
  InvokePaymentRequestUIWithJs(
      content::JsReplace("buyWithMethods([{supportedMethods:$1}"
                         ", {supportedMethods:$2}]);",
                         a_method_name, b_method_name));

  // Click on pay.
  EXPECT_TRUE(IsPayButtonEnabled());
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  // Wait for the response to settle.
  ASSERT_TRUE(
      content::ExecJs(GetActiveWebContents(), "processShowResponse();"));

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::SPEC_DONE_UPDATING,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION,
                               DialogEvent::CONTACT_INFO_EDITOR_OPENED});
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(),
                              "retry({"
                              "  payer: {"
                              "    email: 'EMAIL ERROR',"
                              "    name: 'NAME ERROR',"
                              "    phone: 'PHONE ERROR'"
                              "  }"
                              "});"));
  ASSERT_TRUE(WaitForObservedEvent());

  EXPECT_EQ(u"EMAIL ERROR", GetErrorLabelForType(autofill::EMAIL_ADDRESS));
  EXPECT_EQ(u"NAME ERROR", GetErrorLabelForType(autofill::NAME_FULL));
  EXPECT_EQ(u"PHONE ERROR",
            GetErrorLabelForType(autofill::PHONE_HOME_WHOLE_NUMBER));
}

IN_PROC_BROWSER_TEST_F(MAYBE_PaymentRequestContactInfoEditorTest,
                       RetryWithPayerErrors_NoPaymentOptions) {
  // Installs two apps so that the Payment Request UI will be shown.
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);

  NavigateTo("/payment_request_retry_with_no_payment_options.html");

  autofill::AutofillProfile contact = autofill::test::GetFullProfile();
  AddAutofillProfile(contact);

  // Show a Payment Request.
  InvokePaymentRequestUIWithJs(
      content::JsReplace("buyWithMethods([{supportedMethods:$1}"
                         ", {supportedMethods:$2}]);",
                         a_method_name, b_method_name));

  // Click on pay.
  EXPECT_TRUE(IsPayButtonEnabled());
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  // Wait for the response to settle.
  ASSERT_TRUE(
      content::ExecJs(GetActiveWebContents(), "processShowResponse();"));

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::SPEC_DONE_UPDATING,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION});
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(),
                              "retry({"
                              "  payer: {"
                              "    email: 'EMAIL ERROR',"
                              "    name: 'NAME ERROR',"
                              "    phone: 'PHONE ERROR'"
                              "  }"
                              "});"));
  ASSERT_TRUE(WaitForObservedEvent());

  const int kErrorLabelOffset =
      static_cast<int>(DialogViewID::ERROR_LABEL_OFFSET);
  EXPECT_EQ(nullptr, dialog_view()->GetViewByID(kErrorLabelOffset +
                                                autofill::EMAIL_ADDRESS));
  EXPECT_EQ(nullptr, dialog_view()->GetViewByID(kErrorLabelOffset +
                                                autofill::NAME_FULL));
  EXPECT_EQ(nullptr,
            dialog_view()->GetViewByID(kErrorLabelOffset +
                                       autofill::PHONE_HOME_WHOLE_NUMBER));
}

}  // namespace payments
