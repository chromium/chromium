// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace payments {

namespace {

const base::Time kSomeDate = base::Time::FromSecondsSinceUnixEpoch(1484505871);
const base::Time kSomeLaterDate =
    base::Time::FromSecondsSinceUnixEpoch(1497552271);

}  // namespace

using PaymentRequestShippingAddressUseStatsTest = PaymentRequestBrowserTestBase;

// Tests that use stats for the shipping address used in a Payment Request are
// properly updated upon completion.
// Flaky. https://crbug.com/1495539.
IN_PROC_BROWSER_TEST_F(PaymentRequestShippingAddressUseStatsTest,
                       DISABLED_RecordUse) {
  std::string payment_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &payment_method_name);

  NavigateTo("/payment_request_free_shipping_test.html");
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kSomeDate);

  // Create two addresses, one with a higher frequency score so that it is
  // selected as the default shipping address.
  autofill::AutofillProfile shipping_address1 =
      autofill::test::GetFullProfile();
  AddAutofillProfile(shipping_address1);
  autofill::AutofillProfile shipping_address2 =
      autofill::test::GetFullProfile2();
  shipping_address2.set_use_count(3);
  AddAutofillProfile(shipping_address2);

  // Check that the initial use stats were set correctly.
  const autofill::AutofillProfile* initial_shipping =
      GetDataManager()->address_data_manager().GetProfileByGUID(
          shipping_address2.guid());
  EXPECT_EQ(3U, initial_shipping->use_count());
  EXPECT_EQ(kSomeDate, initial_shipping->use_date());

  // Complete the Payment Request.
  test_clock.SetNow(kSomeLaterDate);
  InvokePaymentRequestUIWithJs("buyWithMethods([{supportedMethods:'" +
                               payment_method_name + "'}]);");
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());
  WaitForOnPersonalDataChanged();

  // Check that the usage of the profile was recorded.
  const autofill::AutofillProfile* updated_shipping =
      GetDataManager()->address_data_manager().GetProfileByGUID(
          shipping_address2.guid());
  EXPECT_EQ(4U, updated_shipping->use_count());
  EXPECT_EQ(kSomeLaterDate, updated_shipping->use_date());
}

using PaymentRequestContactAddressUseStatsTest = PaymentRequestBrowserTestBase;

// Tests that use stats for the contact address used in a Payment Request are
// properly updated upon completion.
// Flaky. https://crbug.com/1495539.
IN_PROC_BROWSER_TEST_F(PaymentRequestContactAddressUseStatsTest,
                       DISABLED_RecordUse) {
  std::string payment_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &payment_method_name);

  NavigateTo("/payment_request_name_test.html");
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kSomeDate);

  // Create two addresses, one with a higher frequency score so that it is
  // selected as the default contact address.
  autofill::AutofillProfile contact_address1 = autofill::test::GetFullProfile();
  AddAutofillProfile(contact_address1);
  autofill::AutofillProfile contact_address2 =
      autofill::test::GetFullProfile2();
  contact_address2.set_use_count(3);
  AddAutofillProfile(contact_address2);

  // Check that the initial use stats were set correctly.
  const autofill::AutofillProfile* initial_contact =
      GetDataManager()->address_data_manager().GetProfileByGUID(
          contact_address2.guid());
  EXPECT_EQ(3U, initial_contact->use_count());
  EXPECT_EQ(kSomeDate, initial_contact->use_date());

  // Complete the Payment Request.
  test_clock.SetNow(kSomeLaterDate);
  InvokePaymentRequestUIWithJs("buyWithMethods([{supportedMethods:'" +
                               payment_method_name + "'}]);");
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());
  WaitForOnPersonalDataChanged();

  // Check that the usage of the profile was recorded.
  const autofill::AutofillProfile* updated_contact =
      GetDataManager()->address_data_manager().GetProfileByGUID(
          contact_address2.guid());
  EXPECT_EQ(4U, updated_contact->use_count());
  EXPECT_EQ(kSomeLaterDate, updated_contact->use_date());
}

using PaymentRequestSameShippingAndContactAddressUseStatsTest =
    PaymentRequestBrowserTestBase;

// Tests that use stats for an address that was used both as a shipping and
// contact address in a Payment Request are properly updated upon completion.
// Flaky. https://crbug.com/1495539.
IN_PROC_BROWSER_TEST_F(PaymentRequestSameShippingAndContactAddressUseStatsTest,
                       DISABLED_RecordUse) {
  std::string payment_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &payment_method_name);

  NavigateTo("/payment_request_contact_details_and_free_shipping_test.html");
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kSomeDate);

  // Create two addresses, one with a higher frequency score so that it is
  // selected as the default shipping and contact address.
  autofill::AutofillProfile multi_address1 = autofill::test::GetFullProfile();
  AddAutofillProfile(multi_address1);
  autofill::AutofillProfile multi_address2 = autofill::test::GetFullProfile2();
  multi_address2.set_use_count(3);
  AddAutofillProfile(multi_address2);

  // Check that the initial use stats were set correctly.
  const autofill::AutofillProfile* initial_multi =
      GetDataManager()->address_data_manager().GetProfileByGUID(
          multi_address2.guid());
  EXPECT_EQ(3U, initial_multi->use_count());
  EXPECT_EQ(kSomeDate, initial_multi->use_date());

  // Complete the Payment Request.
  test_clock.SetNow(kSomeLaterDate);
  InvokePaymentRequestUIWithJs("buyWithMethods([{supportedMethods:'" +
                               payment_method_name + "'}]);");
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());
  WaitForOnPersonalDataChanged();

  // Check that the usage of the profile was only recorded once.
  const autofill::AutofillProfile* updated_multi =
      GetDataManager()->address_data_manager().GetProfileByGUID(
          multi_address2.guid());
  EXPECT_EQ(4U, updated_multi->use_count());
  EXPECT_EQ(kSomeLaterDate, updated_multi->use_date());
}

}  // namespace payments
