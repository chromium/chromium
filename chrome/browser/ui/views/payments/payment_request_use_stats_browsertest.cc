// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/test/browser_test_utils.h"

namespace payments {

namespace {

const base::Time kSomeDate = base::Time::FromDoubleT(1484505871);
const base::Time kSomeLaterDate = base::Time::FromDoubleT(1497552271);

}  // namespace

class PaymentRequestAutofillInstrumentUseStatsTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestAutofillInstrumentUseStatsTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestAutofillInstrumentUseStatsTest);
};

// Tests that use stats for the autofill payment instrument used in a Payment
// Request are properly updated upon completion.
IN_PROC_BROWSER_TEST_F(PaymentRequestAutofillInstrumentUseStatsTest,
                       RecordUse) {
  NavigateTo("/payment_request_no_shipping_test.html");
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kSomeDate);

  // Setup a credit card with an associated billing address.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  // Check that the initial use stats were set correctly.
  autofill::CreditCard* initial_card =
      GetDataManager()->GetCreditCardByGUID(card.guid());
  EXPECT_EQ(1U, initial_card->use_count());
  EXPECT_EQ(kSomeDate, initial_card->use_date());

  // Complete the Payment Request.
  test_clock.SetNow(kSomeLaterDate);
  InvokePaymentRequestUI();
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"));
  WaitForOnPersonalDataChanged();

  // Check that the usage of the card was recorded.
  autofill::CreditCard* updated_card =
      GetDataManager()->GetCreditCardByGUID(card.guid());
  EXPECT_EQ(2U, updated_card->use_count());
  EXPECT_EQ(kSomeLaterDate, updated_card->use_date());
}

class PaymentRequestShippingAddressUseStatsTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestShippingAddressUseStatsTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestShippingAddressUseStatsTest);
};

// Tests that use stats for the shipping address used in a Payment Request are
// properly updated upon completion.
IN_PROC_BROWSER_TEST_F(PaymentRequestShippingAddressUseStatsTest, RecordUse) {
  NavigateTo("/payment_request_free_shipping_test.html");
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kSomeDate);

  // Create a billing address and a card that uses it.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  // Create a shipping address with a higher frecency score, so that it is
  // selected as the default shipping address.
  autofill::AutofillProfile shipping_address =
      autofill::test::GetFullProfile2();
  shipping_address.set_use_count(3);
  AddAutofillProfile(shipping_address);

  // Check that the initial use stats were set correctly.
  autofill::AutofillProfile* initial_shipping =
      GetDataManager()->GetProfileByGUID(shipping_address.guid());
  EXPECT_EQ(3U, initial_shipping->use_count());
  EXPECT_EQ(kSomeDate, initial_shipping->use_date());

  // Complete the Payment Request.
  test_clock.SetNow(kSomeLaterDate);
  InvokePaymentRequestUI();
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"));
  WaitForOnPersonalDataChanged();

  // Check that the usage of the profile was recorded.
  autofill::AutofillProfile* updated_shipping =
      GetDataManager()->GetProfileByGUID(shipping_address.guid());
  EXPECT_EQ(4U, updated_shipping->use_count());
  EXPECT_EQ(kSomeLaterDate, updated_shipping->use_date());
}

class PaymentRequestContactAddressUseStatsTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestContactAddressUseStatsTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestContactAddressUseStatsTest);
};

// Tests that use stats for the contact address used in a Payment Request are
// properly updated upon completion.
IN_PROC_BROWSER_TEST_F(PaymentRequestContactAddressUseStatsTest, RecordUse) {
  NavigateTo("/payment_request_name_test.html");
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kSomeDate);

  // Setup a credit card with an associated billing address.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  // Create a contact address with a higher frecency score, so that it is
  // selected as the default contact address.
  autofill::AutofillProfile contact_address = autofill::test::GetFullProfile2();
  contact_address.set_use_count(3);
  AddAutofillProfile(contact_address);

  // Check that the initial use stats were set correctly.
  autofill::AutofillProfile* initial_contact =
      GetDataManager()->GetProfileByGUID(contact_address.guid());
  EXPECT_EQ(3U, initial_contact->use_count());
  EXPECT_EQ(kSomeDate, initial_contact->use_date());

  // Complete the Payment Request.
  test_clock.SetNow(kSomeLaterDate);
  InvokePaymentRequestUI();
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"));
  WaitForOnPersonalDataChanged();

  // Check that the usage of the profile was recorded.
  autofill::AutofillProfile* updated_contact =
      GetDataManager()->GetProfileByGUID(contact_address.guid());
  EXPECT_EQ(4U, updated_contact->use_count());
  EXPECT_EQ(kSomeLaterDate, updated_contact->use_date());
}

class PaymentRequestSameShippingAndContactAddressUseStatsTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestSameShippingAndContactAddressUseStatsTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(
      PaymentRequestSameShippingAndContactAddressUseStatsTest);
};

// Tests that use stats for an address that was used both as a shipping and
// contact address in a Payment Request are properly updated upon completion.
IN_PROC_BROWSER_TEST_F(PaymentRequestSameShippingAndContactAddressUseStatsTest,
                       RecordUse) {
  NavigateTo("/payment_request_contact_details_and_free_shipping_test.html");
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kSomeDate);

  // Setup a credit card with an associated billing address.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  // Create an address with a higher frecency score, so that it is selected as
  // the default shipping and contact address.
  autofill::AutofillProfile multi_address = autofill::test::GetFullProfile2();
  multi_address.set_use_count(3);
  AddAutofillProfile(multi_address);

  // Check that the initial use stats were set correctly.
  autofill::AutofillProfile* initial_multi =
      GetDataManager()->GetProfileByGUID(multi_address.guid());
  EXPECT_EQ(3U, initial_multi->use_count());
  EXPECT_EQ(kSomeDate, initial_multi->use_date());

  // Complete the Payment Request.
  test_clock.SetNow(kSomeLaterDate);
  InvokePaymentRequestUI();
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"));
  WaitForOnPersonalDataChanged();

  // Check that the usage of the profile was only recorded once.
  autofill::AutofillProfile* updated_multi =
      GetDataManager()->GetProfileByGUID(multi_address.guid());
  EXPECT_EQ(4U, updated_multi->use_count());
  EXPECT_EQ(kSomeLaterDate, updated_multi->use_date());
}

}  // namespace payments
