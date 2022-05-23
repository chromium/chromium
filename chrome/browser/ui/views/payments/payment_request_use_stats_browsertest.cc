// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"
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
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace payments {

namespace {

const base::Time kSomeDate = base::Time::FromDoubleT(1484505871);
const base::Time kSomeLaterDate = base::Time::FromDoubleT(1497552271);

}  // namespace

class PaymentRequestAutofillInstrumentUseStatsTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestAutofillInstrumentUseStatsTest(
      const PaymentRequestAutofillInstrumentUseStatsTest&) = delete;
  PaymentRequestAutofillInstrumentUseStatsTest& operator=(
      const PaymentRequestAutofillInstrumentUseStatsTest&) = delete;

 protected:
  PaymentRequestAutofillInstrumentUseStatsTest() {}
};

// Tests that use stats for the autofill payment instrument used in a Payment
// Request are properly updated upon completion.
// TODO(crbug.com/938763): Flaky on Linux and Win10, at least
IN_PROC_BROWSER_TEST_F(PaymentRequestAutofillInstrumentUseStatsTest,
                       DISABLED_RecordUse) {
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
  PayWithCreditCardAndWait(u"123");
  WaitForOnPersonalDataChanged();

  // Check that the usage of the card was recorded.
  autofill::CreditCard* updated_card =
      GetDataManager()->GetCreditCardByGUID(card.guid());
  EXPECT_EQ(2U, updated_card->use_count());
  EXPECT_EQ(kSomeLaterDate, updated_card->use_date());
}

class PaymentRequestShippingAddressUseStatsTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestShippingAddressUseStatsTest(
      const PaymentRequestShippingAddressUseStatsTest&) = delete;
  PaymentRequestShippingAddressUseStatsTest& operator=(
      const PaymentRequestShippingAddressUseStatsTest&) = delete;

 protected:
  PaymentRequestShippingAddressUseStatsTest() {
    feature_list_.InitAndEnableFeature(::features::kPaymentRequestBasicCard);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that use stats for the shipping address used in a Payment Request are
// properly updated upon completion.
// TODO(crbug.com/1327722): Test is flaky.
IN_PROC_BROWSER_TEST_F(PaymentRequestShippingAddressUseStatsTest,
                       DISABLED_RecordUse) {
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
  PayWithCreditCardAndWait(u"123");
  WaitForOnPersonalDataChanged();

  // Check that the usage of the profile was recorded.
  autofill::AutofillProfile* updated_shipping =
      GetDataManager()->GetProfileByGUID(shipping_address.guid());
  EXPECT_EQ(4U, updated_shipping->use_count());
  EXPECT_EQ(kSomeLaterDate, updated_shipping->use_date());
}

// The tests in this class correspond to the tests of the same name in
// PaymentRequestShippingAddressUseStatsTest, but with basic-card disabled.
// Parameterized tests are not used because the test setup for both tests are
// too different.
class PaymentRequestShippingAddressUseStatsBasicCardDisabledTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestShippingAddressUseStatsBasicCardDisabledTest(
      const PaymentRequestShippingAddressUseStatsBasicCardDisabledTest&) =
      delete;
  PaymentRequestShippingAddressUseStatsBasicCardDisabledTest& operator=(
      const PaymentRequestShippingAddressUseStatsBasicCardDisabledTest&) =
      delete;

 protected:
  PaymentRequestShippingAddressUseStatsBasicCardDisabledTest() {
    feature_list_.InitAndDisableFeature(::features::kPaymentRequestBasicCard);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that use stats for the shipping address used in a Payment Request are
// properly updated upon completion.
IN_PROC_BROWSER_TEST_F(
    PaymentRequestShippingAddressUseStatsBasicCardDisabledTest,
    RecordUse) {
  std::string payment_method_name;
  InstallPaymentApp("a.com", "payment_request_success_responder.js",
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
  autofill::AutofillProfile* initial_shipping =
      GetDataManager()->GetProfileByGUID(shipping_address2.guid());
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
  autofill::AutofillProfile* updated_shipping =
      GetDataManager()->GetProfileByGUID(shipping_address2.guid());
  EXPECT_EQ(4U, updated_shipping->use_count());
  EXPECT_EQ(kSomeLaterDate, updated_shipping->use_date());
}

class PaymentRequestContactAddressUseStatsTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestContactAddressUseStatsTest(
      const PaymentRequestContactAddressUseStatsTest&) = delete;
  PaymentRequestContactAddressUseStatsTest& operator=(
      const PaymentRequestContactAddressUseStatsTest&) = delete;

 protected:
  PaymentRequestContactAddressUseStatsTest() {
    feature_list_.InitAndEnableFeature(::features::kPaymentRequestBasicCard);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that use stats for the contact address used in a Payment Request are
// properly updated upon completion.
// TODO(crbug.com/1328016): Flaky on Linux, investigate and re-enable.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_RecordUse DISABLED_RecordUse
#else
#define MAYBE_RecordUse RecordUse
#endif
IN_PROC_BROWSER_TEST_F(PaymentRequestContactAddressUseStatsTest,
                       MAYBE_RecordUse) {
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
  PayWithCreditCardAndWait(u"123");
  WaitForOnPersonalDataChanged();

  // Check that the usage of the profile was recorded.
  autofill::AutofillProfile* updated_contact =
      GetDataManager()->GetProfileByGUID(contact_address.guid());
  EXPECT_EQ(4U, updated_contact->use_count());
  EXPECT_EQ(kSomeLaterDate, updated_contact->use_date());
}

// The tests in this class correspond to the tests of the same name in
// PaymentRequestContactAddressUseStatsTest, but with basic-card disabled.
// Parameterized tests are not used because the test setup for both tests are
// too different.
class PaymentRequestContactAddressUseStatsBasicCardDisabledTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestContactAddressUseStatsBasicCardDisabledTest(
      const PaymentRequestContactAddressUseStatsBasicCardDisabledTest&) =
      delete;
  PaymentRequestContactAddressUseStatsBasicCardDisabledTest& operator=(
      const PaymentRequestContactAddressUseStatsBasicCardDisabledTest&) =
      delete;

 protected:
  PaymentRequestContactAddressUseStatsBasicCardDisabledTest() {
    feature_list_.InitAndDisableFeature(::features::kPaymentRequestBasicCard);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that use stats for the contact address used in a Payment Request are
// properly updated upon completion.
IN_PROC_BROWSER_TEST_F(
    PaymentRequestContactAddressUseStatsBasicCardDisabledTest,
    RecordUse) {
  std::string payment_method_name;
  InstallPaymentApp("a.com", "payment_request_success_responder.js",
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
  autofill::AutofillProfile* initial_contact =
      GetDataManager()->GetProfileByGUID(contact_address2.guid());
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
  autofill::AutofillProfile* updated_contact =
      GetDataManager()->GetProfileByGUID(contact_address2.guid());
  EXPECT_EQ(4U, updated_contact->use_count());
  EXPECT_EQ(kSomeLaterDate, updated_contact->use_date());
}

class PaymentRequestSameShippingAndContactAddressUseStatsTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestSameShippingAndContactAddressUseStatsTest(
      const PaymentRequestSameShippingAndContactAddressUseStatsTest&) = delete;
  PaymentRequestSameShippingAndContactAddressUseStatsTest& operator=(
      const PaymentRequestSameShippingAndContactAddressUseStatsTest&) = delete;

 protected:
  PaymentRequestSameShippingAndContactAddressUseStatsTest() {
    feature_list_.InitAndEnableFeature(::features::kPaymentRequestBasicCard);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that use stats for an address that was used both as a shipping and
// contact address in a Payment Request are properly updated upon completion.
// TODO(crbug.com/1328016): Flaky on Linux, investigate and re-enable.
// MAYBE_RecordUse defined above (PaymentRequestContactAddressUseStatsTest
// fixture).
IN_PROC_BROWSER_TEST_F(PaymentRequestSameShippingAndContactAddressUseStatsTest,
                       MAYBE_RecordUse) {
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
  PayWithCreditCardAndWait(u"123");
  WaitForOnPersonalDataChanged();

  // Check that the usage of the profile was only recorded once.
  autofill::AutofillProfile* updated_multi =
      GetDataManager()->GetProfileByGUID(multi_address.guid());
  EXPECT_EQ(4U, updated_multi->use_count());
  EXPECT_EQ(kSomeLaterDate, updated_multi->use_date());
}

// The tests in this class correspond to the tests of the same name in
// PaymentRequestSameShippingAndContactAddressUseStatsTest, but with basic-card
// disabled. Parameterized tests are not used because the test setup for both
// tests are too different.
class PaymentRequestSameShippingAndContactAddressUseStatsBasicCardDisabledTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestSameShippingAndContactAddressUseStatsBasicCardDisabledTest(
      const PaymentRequestSameShippingAndContactAddressUseStatsBasicCardDisabledTest&) =
      delete;
  PaymentRequestSameShippingAndContactAddressUseStatsBasicCardDisabledTest&
  operator=(
      const PaymentRequestSameShippingAndContactAddressUseStatsBasicCardDisabledTest&) =
      delete;

 protected:
  PaymentRequestSameShippingAndContactAddressUseStatsBasicCardDisabledTest() {
    feature_list_.InitAndDisableFeature(::features::kPaymentRequestBasicCard);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that use stats for an address that was used both as a shipping and
// contact address in a Payment Request are properly updated upon completion.
IN_PROC_BROWSER_TEST_F(
    PaymentRequestSameShippingAndContactAddressUseStatsBasicCardDisabledTest,
    RecordUse) {
  std::string payment_method_name;
  InstallPaymentApp("a.com", "payment_request_success_responder.js",
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
  autofill::AutofillProfile* initial_multi =
      GetDataManager()->GetProfileByGUID(multi_address2.guid());
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
  autofill::AutofillProfile* updated_multi =
      GetDataManager()->GetProfileByGUID(multi_address2.guid());
  EXPECT_EQ(4U, updated_multi->use_count());
  EXPECT_EQ(kSomeLaterDate, updated_multi->use_date());
}

}  // namespace payments
