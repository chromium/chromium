// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/user_data_util.h"

#include "base/guid.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::SizeIs;

TEST(UserDataUtilTest, SortsCompleteContactsAlphabetically) {
  auto profile_a = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_a.get(), "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  auto profile_b = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_b.get(), "Berta", "", "West",
                                 "berta.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  auto profile_unicode = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_unicode.get(),
                                 "\xC3\x85"
                                 "dam",
                                 "", "West", "adam.west@gmail.com", "", "", "",
                                 "", "", "", "", "");

  // Specify profiles in reverse order to force sorting.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles;
  profiles.emplace_back(std::move(profile_unicode));
  profiles.emplace_back(std::move(profile_b));
  profiles.emplace_back(std::move(profile_a));

  CollectUserDataOptions options;
  options.request_payer_name = true;
  options.request_payer_email = true;

  std::vector<int> profile_indices =
      autofill_assistant::SortContactsByCompleteness(options, profiles);
  EXPECT_THAT(profile_indices, SizeIs(profiles.size()));
  EXPECT_THAT(profile_indices, ElementsAre(2, 1, 0));
}

TEST(UserDataUtilTest, SortsContactsByCompleteness) {
  auto profile_complete = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(
      profile_complete.get(), "Charlie", "", "West", "charlie.west@gmail.com",
      "", "Baker Street 221b", "", "London", "", "WC2N 5DU", "UK", "+44");

  auto profile_no_phone = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(
      profile_no_phone.get(), "Berta", "", "West", "berta.west@gmail.com", "",
      "Baker Street 221b", "", "London", "", "WC2N 5DU", "UK", "");

  auto profile_incomplete = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_incomplete.get(), "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  // Specify profiles in reverse order to force sorting.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles;
  profiles.emplace_back(std::move(profile_incomplete));
  profiles.emplace_back(std::move(profile_no_phone));
  profiles.emplace_back(std::move(profile_complete));

  CollectUserDataOptions options;
  options.request_payer_name = true;
  options.request_payer_email = true;
  options.request_payer_phone = true;
  options.request_shipping = true;

  std::vector<int> profile_indices =
      autofill_assistant::SortContactsByCompleteness(options, profiles);
  EXPECT_THAT(profile_indices, SizeIs(profiles.size()));
  EXPECT_THAT(profile_indices, ElementsAre(2, 1, 0));
}

TEST(UserDataUtilTest, GetDefaultContactSelectionForEmptyProfiles) {
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles;
  CollectUserDataOptions options;

  EXPECT_THAT(GetDefaultContactProfile(options, profiles), -1);
}

TEST(UserDataUtilTest, GetDefaultContactSelectionForCompleteProfiles) {
  auto profile_b = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_b.get(), "Berta", "", "West",
                                 "berta.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  auto profile_a = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_a.get(), "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  // Specify profiles in reverse order to force sorting.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles;
  profiles.emplace_back(std::move(profile_b));
  profiles.emplace_back(std::move(profile_a));

  CollectUserDataOptions options;
  options.request_payer_name = true;
  options.request_payer_email = true;

  EXPECT_THAT(GetDefaultContactProfile(options, profiles), 1);
}

TEST(UserDataUtilTest, GetDefaultSelectionForDefaultEmail) {
  auto profile_complete = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_complete.get(), "Berta", "", "West",
                                 "berta.west@gmail.com", "", "", "", "", "", "",
                                 "", "+41");

  auto profile_incomplete_with_default_email =
      std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_incomplete_with_default_email.get(),
                                 "", "", "", "adam.west@gmail.com", "", "", "",
                                 "", "", "", "", "");

  auto profile_complete_with_default_email =
      std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_complete_with_default_email.get(),
                                 "Adam", "", "West", "adam.west@gmail.com", "",
                                 "", "", "", "", "", "", "");

  // Specify profiles in reverse order to force sorting.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles;
  profiles.emplace_back(std::move(profile_complete));
  profiles.emplace_back(std::move(profile_incomplete_with_default_email));
  profiles.emplace_back(std::move(profile_complete_with_default_email));

  CollectUserDataOptions options;
  options.request_payer_name = true;
  options.request_payer_email = true;
  options.request_payer_phone = true;
  options.default_email = "adam.west@gmail.com";

  EXPECT_THAT(GetDefaultContactProfile(options, profiles), 2);
}

TEST(UserDataUtilTest, SortsCompleteAddressesAlphabetically) {
  auto profile_b = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_b.get(), "Berta", "", "West", "", "",
                                 "Brandschenkestrasse 110", "", "Zurich", "",
                                 "8002", "CH", "");

  auto profile_a = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_a.get(), "Adam", "", "West", "", "",
                                 "Brandschenkestrasse 110", "", "Zurich", "",
                                 "8002", "CH", "");

  // Specify profiles in reverse order to force sorting.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles;
  profiles.emplace_back(std::move(profile_b));
  profiles.emplace_back(std::move(profile_a));

  CollectUserDataOptions options;

  std::vector<int> profile_indices =
      autofill_assistant::SortAddressesByCompleteness(options, profiles);
  EXPECT_THAT(profile_indices, SizeIs(profiles.size()));
  EXPECT_THAT(profile_indices, ElementsAre(1, 0));
}

TEST(UserDataUtilTest, SortsAddressesByCompleteness) {
  // Adding email address and phone number to demonstrate that they are not
  // checked for completeness.
  auto profile_no_street = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_no_street.get(), "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "Zurich",
                                 "", "8002", "CH", "+41");

  auto profile_complete = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_complete.get(), "Berta", "", "West",
                                 "", "", "Brandschenkestrasse 110", "",
                                 "Zurich", "", "8002", "UK", "");

  // Specify profiles in reverse order to force sorting.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles;
  profiles.emplace_back(std::move(profile_no_street));
  profiles.emplace_back(std::move(profile_complete));

  CollectUserDataOptions options;

  std::vector<int> profile_indices =
      autofill_assistant::SortAddressesByCompleteness(options, profiles);
  EXPECT_THAT(profile_indices, SizeIs(profiles.size()));
  EXPECT_THAT(profile_indices, ElementsAre(1, 0));
}

TEST(UserDataUtilTest, GetDefaultAddressSelectionForEmptyProfiles) {
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles;
  CollectUserDataOptions options;

  EXPECT_THAT(GetDefaultAddressProfile(options, profiles), -1);
}

TEST(UserDataUtilTest, GetDefaultAddressSelectionForCompleteProfiles) {
  // Adding email address and phone number to demonstrate that they are not
  // checked for completeness.
  auto profile_with_irrelevant_details =
      std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_with_irrelevant_details.get(), "Berta",
                                 "berta.west@gmail.com", "West", "", "",
                                 "Brandschenkestrasse 110", "", "Zurich", "",
                                 "8002", "CH", "+41");

  auto profile_complete = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_complete.get(), "Adam", "", "West", "",
                                 "", "Brandschenkestrasse 110", "", "Zurich",
                                 "", "8002", "CH", "");

  // Specify profiles in reverse order to force sorting.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles;
  profiles.emplace_back(std::move(profile_with_irrelevant_details));
  profiles.emplace_back(std::move(profile_complete));

  CollectUserDataOptions options;

  EXPECT_THAT(GetDefaultAddressProfile(options, profiles), 1);
}

TEST(UserDataUtilTest, SortsCreditCardsByCompleteness) {
  auto complete_card = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(complete_card.get(), "Berta West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "");
  auto complete_instrument =
      std::make_unique<PaymentInstrument>(std::move(complete_card), nullptr);

  auto incomplete_card = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(incomplete_card.get(), "Adam West",
                                    "4111111111111111", "", "",
                                    /* billing_address_id= */ "");
  auto incomplete_instrument =
      std::make_unique<PaymentInstrument>(std::move(incomplete_card), nullptr);

  // Specify payment instruments in reverse order to force sorting.
  std::vector<std::unique_ptr<PaymentInstrument>> payment_instruments;
  payment_instruments.emplace_back(std::move(incomplete_instrument));
  payment_instruments.emplace_back(std::move(complete_instrument));

  CollectUserDataOptions options;

  std::vector<int> sorted_indices =
      SortPaymentInstrumentsByCompleteness(options, payment_instruments);
  EXPECT_THAT(sorted_indices, SizeIs(payment_instruments.size()));
  EXPECT_THAT(sorted_indices, ElementsAre(1, 0));
}

TEST(UserDataUtilTest, SortsCompleteCardsByName) {
  auto a_card = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(a_card.get(), "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "");
  auto a_instrument =
      std::make_unique<PaymentInstrument>(std::move(a_card), nullptr);

  auto b_card = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(b_card.get(), "Berta West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "");
  auto b_instrument =
      std::make_unique<PaymentInstrument>(std::move(b_card), nullptr);

  // Specify payment instruments in reverse order to force sorting.
  std::vector<std::unique_ptr<PaymentInstrument>> payment_instruments;
  payment_instruments.emplace_back(std::move(b_instrument));
  payment_instruments.emplace_back(std::move(a_instrument));

  CollectUserDataOptions options;

  std::vector<int> sorted_indices =
      SortPaymentInstrumentsByCompleteness(options, payment_instruments);
  EXPECT_THAT(sorted_indices, SizeIs(payment_instruments.size()));
  EXPECT_THAT(sorted_indices, ElementsAre(1, 0));
}

TEST(UserDataUtilTest, SortsCreditCardsByAddressCompleteness) {
  auto card_with_complete_address = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(card_with_complete_address.get(),
                                    "Charlie West", "4111111111111111", "1",
                                    "2050",
                                    /* billing_address_id= */ "address-1");
  auto billing_address_with_zip = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(billing_address_with_zip.get(), "Charlie", "",
                                 "West", "charlie.west@gmail.com", "",
                                 "Baker Street 221b", "", "London", "",
                                 "WC2N 5DU", "UK", "+44");
  auto instrument_with_complete_address =
      std::make_unique<PaymentInstrument>(std::move(card_with_complete_address),
                                          std::move(billing_address_with_zip));

  auto card_with_incomplete_address = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(card_with_incomplete_address.get(),
                                    "Berta West", "4111111111111111", "1",
                                    "2050",
                                    /* billing_address_id= */ "address-1");
  auto billing_address_without_zip =
      std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(billing_address_without_zip.get(), "Berta", "",
                                 "West", "berta.west@gmail.com", "",
                                 "Baker Street 221b", "", "London", "", "",
                                 "UK", "+44");
  auto instrument_with_incomplete_address = std::make_unique<PaymentInstrument>(
      std::move(card_with_incomplete_address),
      std::move(billing_address_without_zip));

  auto card_without_address = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(card_without_address.get(), "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "");
  auto instrument_without_address = std::make_unique<PaymentInstrument>(
      std::move(card_without_address), nullptr);

  // Specify payment instruments in reverse order to force sorting.
  std::vector<std::unique_ptr<PaymentInstrument>> payment_instruments;
  payment_instruments.emplace_back(std::move(instrument_without_address));
  payment_instruments.emplace_back(
      std::move(instrument_with_incomplete_address));
  payment_instruments.emplace_back(std::move(instrument_with_complete_address));

  CollectUserDataOptions options;
  options.require_billing_postal_code = true;

  std::vector<int> sorted_indices =
      SortPaymentInstrumentsByCompleteness(options, payment_instruments);
  EXPECT_THAT(sorted_indices, SizeIs(payment_instruments.size()));
  EXPECT_THAT(sorted_indices, ElementsAre(2, 1, 0));
}

TEST(UserDataUtilTest, GetDefaultSelectionForEmptyPaymentInstruments) {
  std::vector<std::unique_ptr<PaymentInstrument>> payment_instruments;
  CollectUserDataOptions options;

  EXPECT_THAT(GetDefaultPaymentInstrument(options, payment_instruments), -1);
}

TEST(UserDataUtilTest, GetDefaultSelectionForCompletePaymentInstruments) {
  auto b_card = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(b_card.get(), "Berta West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "");
  auto b_instrument =
      std::make_unique<PaymentInstrument>(std::move(b_card), nullptr);

  auto a_card = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(a_card.get(), "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "");
  auto a_instrument =
      std::make_unique<PaymentInstrument>(std::move(a_card), nullptr);

  // Specify payment instruments in reverse order to force sorting.
  std::vector<std::unique_ptr<PaymentInstrument>> payment_instruments;
  payment_instruments.emplace_back(std::move(b_instrument));
  payment_instruments.emplace_back(std::move(a_instrument));

  CollectUserDataOptions options;
  options.request_payer_name = true;
  options.request_payer_email = true;

  EXPECT_THAT(GetDefaultPaymentInstrument(options, payment_instruments), 1);
}

TEST(UserDataUtilTest, CompareContactDetailsMatch) {
  autofill::AutofillProfile profile_a;
  autofill::test::SetProfileInfo(&profile_a, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "+41");

  autofill::AutofillProfile profile_b;
  autofill::test::SetProfileInfo(&profile_b, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "+41");

  CollectUserDataOptions options;
  options.request_payer_name = true;
  options.request_payer_email = true;
  options.request_payer_phone = true;

  EXPECT_TRUE(CompareContactDetails(options, &profile_a, &profile_b));
}

TEST(UserDataUtilTest, CompareContactDetailsMismatchForNoChecks) {
  autofill::AutofillProfile profile_a;
  autofill::test::SetProfileInfo(&profile_a, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "+41");

  autofill::AutofillProfile profile_b;
  autofill::test::SetProfileInfo(&profile_b, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "+41");

  CollectUserDataOptions options;

  EXPECT_FALSE(CompareContactDetails(options, &profile_a, &profile_b));
}

TEST(UserDataUtilTest, CompareContactDetailsMismatches) {
  autofill::AutofillProfile profile_truth;
  autofill::test::SetProfileInfo(&profile_truth, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "+41");

  autofill::AutofillProfile profile_mismatching_name;
  autofill::test::SetProfileInfo(&profile_mismatching_name, "Berta", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "+41");

  autofill::AutofillProfile profile_mismatching_email;
  autofill::test::SetProfileInfo(&profile_mismatching_email, "Adam", "", "West",
                                 "berta.west@gmail.com", "", "", "", "", "", "",
                                 "", "+41");

  autofill::AutofillProfile profile_mismatching_phone;
  autofill::test::SetProfileInfo(&profile_mismatching_name, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "+44");

  CollectUserDataOptions options;
  options.request_payer_name = true;
  options.request_payer_email = true;
  options.request_payer_phone = true;

  EXPECT_FALSE(CompareContactDetails(options, &profile_truth,
                                     &profile_mismatching_name));
  EXPECT_FALSE(CompareContactDetails(options, &profile_truth,
                                     &profile_mismatching_email));
  EXPECT_FALSE(CompareContactDetails(options, &profile_truth,
                                     &profile_mismatching_phone));
}

TEST(UserDataUtilTest, CompareContactDetailsMatchesForUnqueriedFields) {
  autofill::AutofillProfile profile_truth;
  autofill::test::SetProfileInfo(&profile_truth, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "+41");

  autofill::AutofillProfile profile_mismatching_name;
  autofill::test::SetProfileInfo(&profile_mismatching_name, "Berta", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "+41");

  autofill::AutofillProfile profile_mismatching_email;
  autofill::test::SetProfileInfo(&profile_mismatching_email, "Adam", "", "West",
                                 "berta.west@gmail.com", "", "", "", "", "", "",
                                 "", "+41");

  autofill::AutofillProfile profile_mismatching_phone;
  autofill::test::SetProfileInfo(&profile_mismatching_phone, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "+44");

  CollectUserDataOptions options_no_check_name;
  options_no_check_name.request_payer_email = true;
  options_no_check_name.request_payer_phone = true;

  CollectUserDataOptions options_no_check_email;
  options_no_check_email.request_payer_name = true;
  options_no_check_email.request_payer_phone = true;

  CollectUserDataOptions options_no_check_phone;
  options_no_check_phone.request_payer_name = true;
  options_no_check_phone.request_payer_email = true;

  EXPECT_TRUE(CompareContactDetails(options_no_check_name, &profile_truth,
                                    &profile_mismatching_name));
  EXPECT_TRUE(CompareContactDetails(options_no_check_email, &profile_truth,
                                    &profile_mismatching_email));
  EXPECT_TRUE(CompareContactDetails(options_no_check_phone, &profile_truth,
                                    &profile_mismatching_phone));
}

TEST(UserDataUtilTest, ContactCompletenessNotRequired) {
  CollectUserDataOptions not_required_options;
  not_required_options.request_payer_name = false;
  not_required_options.request_payer_email = false;
  not_required_options.request_payer_phone = false;

  EXPECT_TRUE(IsCompleteContact(nullptr, not_required_options));
}

TEST(UserDataUtilTest, ContactCompletenessRequireName) {
  autofill::AutofillProfile contact;
  CollectUserDataOptions require_name_options;
  require_name_options.request_payer_name = true;

  EXPECT_FALSE(IsCompleteContact(nullptr, require_name_options));
  autofill::test::SetProfileInfo(&contact, /* first_name= */ "",
                                 /* middle_name= */ "",
                                 /* last_name= */ "", "adam.west@gmail.com", "",
                                 "", "", "", "", "", "", "+41");
  EXPECT_FALSE(IsCompleteContact(&contact, require_name_options));
  autofill::test::SetProfileInfo(&contact, "John", /* middle_name= */ "", "Doe",
                                 "", "", "", "", "", "", "", "", "");
  EXPECT_TRUE(IsCompleteContact(&contact, require_name_options));
}

TEST(UserDataUtilTest, ContactCompletenessRequireEmail) {
  autofill::AutofillProfile contact;
  CollectUserDataOptions require_email_options;
  require_email_options.request_payer_email = true;

  EXPECT_FALSE(IsCompleteContact(nullptr, require_email_options));
  autofill::test::SetProfileInfo(&contact, "John", "", "Doe",
                                 /* email= */ "", "", "", "", "", "", "", "",
                                 "+41");
  EXPECT_FALSE(IsCompleteContact(&contact, require_email_options));
  autofill::test::SetProfileInfo(&contact, "John", "", "Doe",
                                 "john.doe@gmail.com", "", "", "", "", "", "",
                                 "", "+41");
  EXPECT_TRUE(IsCompleteContact(&contact, require_email_options));
}

TEST(UserDataUtilTest, ContactCompletenessRequirePhone) {
  autofill::AutofillProfile contact;
  CollectUserDataOptions require_phone_options;
  require_phone_options.request_payer_phone = true;

  EXPECT_FALSE(IsCompleteContact(nullptr, require_phone_options));
  autofill::test::SetProfileInfo(&contact, "John", "", "Doe",
                                 "john.doe@gmail.com", "", "", "", "", "", "",
                                 "",
                                 /* phone= */ "");
  EXPECT_FALSE(IsCompleteContact(&contact, require_phone_options));
  autofill::test::SetProfileInfo(&contact, "", "", "", "", "", "", "", "", "",
                                 "", "", "+41");
  EXPECT_TRUE(IsCompleteContact(&contact, require_phone_options));
}

TEST(UserDataUtilTest, CompleteShippingAddressNotRequired) {
  CollectUserDataOptions not_required_options;
  not_required_options.request_shipping = false;

  EXPECT_TRUE(IsCompleteShippingAddress(nullptr, not_required_options));
}

TEST(UserDataUtilTest, CompleteShippingAddressRequired) {
  autofill::AutofillProfile address;
  CollectUserDataOptions require_shipping_options;
  require_shipping_options.request_shipping = true;

  autofill::test::SetProfileInfo(&address, "John", "", "Doe",
                                 "john.doe@gmail.com", "", /* address1= */ "",
                                 /* address2= */ "", /* city= */ "",
                                 /* state=  */ "", /* zip_code=  */ "",
                                 /* country= */ "", "+41");
  EXPECT_FALSE(IsCompleteShippingAddress(&address, require_shipping_options));
  autofill::test::SetProfileInfo(&address, "John", "", "Doe",
                                 /* email= */ "", "", "Brandschenkestrasse 110",
                                 "", "Zurich", "Zurich", "8002", "CH",
                                 /* phone= */ "");
  EXPECT_TRUE(IsCompleteShippingAddress(&address, require_shipping_options));
}

TEST(UserDataUtilTest, CompleteCreditCardNotRequired) {
  CollectUserDataOptions not_required_options;
  not_required_options.request_payment_method = false;

  EXPECT_TRUE(IsCompleteCreditCard(nullptr, nullptr, not_required_options));
}

TEST(UserDataUtilTest, CompleteCreditCardZipNotRequired) {
  CollectUserDataOptions payment_options;
  payment_options.request_payment_method = true;
  payment_options.require_billing_postal_code = false;

  autofill::AutofillProfile address;
  autofill::CreditCard card;
  autofill::test::SetCreditCardInfo(&card, "Adam West", "4111111111111111", "1",
                                    "2050",
                                    /* billing_address_id= */ "id");

  EXPECT_FALSE(IsCompleteCreditCard(nullptr, nullptr, payment_options));
  EXPECT_FALSE(IsCompleteCreditCard(&card, nullptr, payment_options));
  EXPECT_FALSE(IsCompleteCreditCard(&card, &address, payment_options));
  // UK addresses do not require a zip code, they are complete without it.
  autofill::test::SetProfileInfo(&address, "John", "", "Doe",
                                 /* email= */ "", "", "Baker Street 221b", "",
                                 "London", /* state= */ "",
                                 /* zipcode= */ "", "UK", /* phone= */ "");
  EXPECT_TRUE(IsCompleteCreditCard(&card, &address, payment_options));
  // CH addresses require a zip code to be complete. This check outranks the
  // |require_billing_postal_code| flag.
  autofill::test::SetProfileInfo(&address, "John", "", "Doe",
                                 /* email= */ "", "", "Brandschenkestrasse 110",
                                 "", "Zurich", "Zurich",
                                 /* zipcode= */ "", "CH", /* phone= */ "");
  EXPECT_FALSE(IsCompleteCreditCard(&card, &address, payment_options));
}

TEST(UserDataUtilTest, CompleteCreditCardZipRequired) {
  CollectUserDataOptions payment_options;
  payment_options.request_payment_method = true;
  payment_options.require_billing_postal_code = true;

  autofill::AutofillProfile address;
  autofill::CreditCard card;
  autofill::test::SetCreditCardInfo(&card, "Adam West", "4111111111111111", "1",
                                    "2050",
                                    /* billing_address_id= */ "id");

  EXPECT_FALSE(IsCompleteCreditCard(nullptr, nullptr, payment_options));
  EXPECT_FALSE(IsCompleteCreditCard(&card, nullptr, payment_options));
  EXPECT_FALSE(IsCompleteCreditCard(&card, &address, payment_options));
  autofill::test::SetProfileInfo(&address, "John", "", "Doe",
                                 /* email= */ "", "", "Baker Street 221b", "",
                                 "London", /* state= */ "",
                                 /* zipcode= */ "", "UK", /* phone= */ "");
  EXPECT_FALSE(IsCompleteCreditCard(&card, &address, payment_options));
  autofill::test::SetProfileInfo(&address, "John", "", "Doe",
                                 /* email= */ "", "", "Baker Street 221b", "",
                                 "London", /* state=  */ "", "WC2N 5DU", "UK",
                                 /* phone= */ "");
  EXPECT_TRUE(IsCompleteCreditCard(&card, &address, payment_options));
}

TEST(UserDataUtilTest, CompleteExpiredCreditCard) {
  CollectUserDataOptions payment_options;
  payment_options.request_payment_method = true;

  autofill::AutofillProfile address;
  autofill::test::SetProfileInfo(
      &address, "John", "", "Doe", "john.doe@gmail.com", "",
      "Brandschenkestrasse 110", "", "Zurich", "Zurich", "8002", "CH", "+41");
  autofill::CreditCard card;

  autofill::test::SetCreditCardInfo(&card, "Adam West", "4111111111111111", "1",
                                    "2000",
                                    /* billing_address_id= */ "id");
  EXPECT_FALSE(IsCompleteCreditCard(&card, &address, payment_options));
  autofill::test::SetCreditCardInfo(&card, "Adam West", "4111111111111111", "1",
                                    "2050",
                                    /* billing_address_id= */ "id");
  EXPECT_TRUE(IsCompleteCreditCard(&card, &address, payment_options));
}

TEST(UserDataUtilTest, CompleteCreditCardWithBadNetwork) {
  autofill::AutofillProfile address;
  autofill::test::SetProfileInfo(
      &address, "John", "", "Doe", "john.doe@gmail.com", "",
      "Brandschenkestrasse 110", "", "Zurich", "Zurich", "8002", "CH", "+41");
  autofill::CreditCard card;
  autofill::test::SetCreditCardInfo(&card, "Adam West", "4111111111111111", "1",
                                    "2050",
                                    /* billing_address_id= */ "id");

  CollectUserDataOptions payment_options_mastercard;
  payment_options_mastercard.request_payment_method = true;
  payment_options_mastercard.supported_basic_card_networks.emplace_back(
      "mastercard");
  EXPECT_FALSE(
      IsCompleteCreditCard(&card, &address, payment_options_mastercard));

  CollectUserDataOptions payment_options_visa;
  payment_options_visa.request_payment_method = true;
  payment_options_visa.supported_basic_card_networks.emplace_back("visa");
  EXPECT_TRUE(IsCompleteCreditCard(&card, &address, payment_options_visa));
}

TEST(UserDataUtilTest, RequestEmptyAutofillValue) {
  UserData user_data;
  AutofillValue autofill_value;
  std::string result;

  EXPECT_EQ(GetFormattedAutofillValue(autofill_value, &user_data, &result)
                .proto_status(),
            INVALID_ACTION);
  EXPECT_EQ(result, "");
}

TEST(UserDataUtilTest, RequestDataFromUnknownProfile) {
  UserData user_data;
  AutofillValue autofill_value;
  autofill_value.mutable_profile()->set_identifier("none");
  autofill_value.set_value_expression("value");
  std::string result;

  EXPECT_EQ(GetFormattedAutofillValue(autofill_value, &user_data, &result)
                .proto_status(),
            PRECONDITION_FAILED);
  EXPECT_EQ(result, "");
}

TEST(UserDataUtilTest, RequestUnknownDataFromKnownProfile) {
  UserData user_data;
  autofill::AutofillProfile contact(base::GenerateGUID(),
                                    autofill::test::kEmptyOrigin);
  // Middle name is expected to be empty.
  autofill::test::SetProfileInfo(&contact, "John", /* middle name */ "", "Doe",
                                 "", "", "", "", "", "", "", "", "");
  user_data.selected_addresses_["contact"] =
      std::make_unique<autofill::AutofillProfile>(contact);

  AutofillValue autofill_value;
  autofill_value.mutable_profile()->set_identifier("contact");
  autofill_value.set_value_expression(
      base::StrCat({"${",
                    base::NumberToString(static_cast<int>(
                        autofill::ServerFieldType::NAME_MIDDLE)),
                    "}"}));

  std::string result;

  EXPECT_EQ(GetFormattedAutofillValue(autofill_value, &user_data, &result)
                .proto_status(),
            AUTOFILL_INFO_NOT_AVAILABLE);
  EXPECT_EQ(result, "");
}

TEST(UserDataUtilTest, RequestKnownDataFromKnownProfile) {
  UserData user_data;
  autofill::AutofillProfile contact(base::GenerateGUID(),
                                    autofill::test::kEmptyOrigin);
  autofill::test::SetProfileInfo(&contact, "John", /* middle name */ "", "Doe",
                                 "", "", "", "", "", "", "", "", "");
  user_data.selected_addresses_["contact"] =
      std::make_unique<autofill::AutofillProfile>(contact);

  AutofillValue autofill_value;
  autofill_value.mutable_profile()->set_identifier("contact");
  autofill_value.set_value_expression(
      base::StrCat({"${",
                    base::NumberToString(static_cast<int>(
                        autofill::ServerFieldType::NAME_FIRST)),
                    "}"}));

  std::string result;

  EXPECT_TRUE(
      GetFormattedAutofillValue(autofill_value, &user_data, &result).ok());
  EXPECT_EQ(result, "John");
}

TEST(UserDataUtilTest, EscapeDataFromProfile) {
  UserData user_data;
  autofill::AutofillProfile contact(base::GenerateGUID(),
                                    autofill::test::kEmptyOrigin);
  autofill::test::SetProfileInfo(&contact, "Jo.h*n", /* middle name */ "",
                                 "Doe", "", "", "", "", "", "", "", "", "");
  user_data.selected_addresses_["contact"] =
      std::make_unique<autofill::AutofillProfile>(contact);

  AutofillValueRegexp autofill_value;
  autofill_value.mutable_profile()->set_identifier("contact");
  autofill_value.mutable_value_expression()->set_re2(
      base::StrCat({"^${",
                    base::NumberToString(static_cast<int>(
                        autofill::ServerFieldType::NAME_FIRST)),
                    "}$"}));

  std::string result;

  EXPECT_TRUE(
      GetFormattedAutofillValue(autofill_value, &user_data, &result).ok());
  EXPECT_EQ(result, "^Jo\\.h\\*n$");
}

}  // namespace
}  // namespace autofill_assistant
