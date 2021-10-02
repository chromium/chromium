// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/user_data_util.h"

#include <string>

#include "base/guid.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/mock_website_login_manager.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/value_util.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace autofill_assistant {
namespace user_data {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::SizeIs;

RequiredDataPiece MakeRequiredDataPiece(autofill::ServerFieldType field) {
  RequiredDataPiece required_data_piece;
  required_data_piece.set_error_message(
      base::NumberToString(static_cast<int>(field)));
  required_data_piece.mutable_condition()->set_key(static_cast<int>(field));
  required_data_piece.mutable_condition()->mutable_not_empty();
  return required_data_piece;
}

TEST(UserDataUtilTest, KeepsOrderForIdenticalContacts) {
  base::Time current = base::Time::Now();

  auto profile_first = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_first.get(), "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");
  profile_first->set_use_date(current);

  auto profile_second = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_second.get(), "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");
  profile_second->set_use_date(current);

  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles;
  profiles.emplace_back(std::move(profile_first));
  profiles.emplace_back(std::move(profile_second));

  CollectUserDataOptions options;

  std::vector<int> profile_indices =
      SortContactsByCompleteness(options, profiles);
  EXPECT_THAT(profile_indices, SizeIs(profiles.size()));
  EXPECT_THAT(profile_indices, ElementsAre(0, 1));
}

TEST(UserDataUtilTest, SortsCompleteContactsByUseDate) {
  base::Time current = base::Time::Now();

  auto profile_old = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_old.get(), "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");
  profile_old->set_use_date(current - base::Days(2));

  auto profile_new = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_new.get(), "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");
  profile_new->set_use_date(current);

  // Specify profiles in reverse order to force sorting.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles;
  profiles.emplace_back(std::move(profile_old));
  profiles.emplace_back(std::move(profile_new));

  CollectUserDataOptions options;
  options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_FULL));
  options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::EMAIL_ADDRESS));

  std::vector<int> profile_indices =
      SortContactsByCompleteness(options, profiles);
  EXPECT_THAT(profile_indices, SizeIs(profiles.size()));
  EXPECT_THAT(profile_indices, ElementsAre(1, 0));
}

TEST(UserDataUtilTest, SortsContactsByCompleteness) {
  auto profile_complete = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_complete.get(), "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Baker Street 221b",
                                 "", "London", "", "WC2N 5DU", "UK", "+44");

  auto profile_no_phone = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_no_phone.get(), "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Baker Street 221b",
                                 "", "London", "", "WC2N 5DU", "UK",
                                 /* phone_number= */ "");

  auto profile_incomplete = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_incomplete.get(), "Adam", "", "West",
                                 /* email= */ "", "", "", "", "", "", "", "",
                                 /* phone_number= */ "");

  // Specify profiles in reverse order to force sorting.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles;
  profiles.emplace_back(std::move(profile_incomplete));
  profiles.emplace_back(std::move(profile_no_phone));
  profiles.emplace_back(std::move(profile_complete));

  CollectUserDataOptions options;
  options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_FULL));
  options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::EMAIL_ADDRESS));
  options.required_contact_data_pieces.push_back(MakeRequiredDataPiece(
      autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER));

  std::vector<int> profile_indices =
      SortContactsByCompleteness(options, profiles);
  EXPECT_THAT(profile_indices, SizeIs(profiles.size()));
  EXPECT_THAT(profile_indices, ElementsAre(2, 1, 0));
}

TEST(UserDataUtilTest, GetDefaultContactSelectionForEmptyProfiles) {
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles;
  CollectUserDataOptions options;

  EXPECT_THAT(GetDefaultContactProfile(options, profiles), -1);
}

TEST(UserDataUtilTest, GetDefaultContactSelectionForCompleteProfiles) {
  base::Time current = base::Time::Now();

  auto profile_old = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_old.get(), "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");
  profile_old->set_use_date(current - base::Days(2));

  auto profile_new = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_new.get(), "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");
  profile_new->set_use_date(current);

  // Specify profiles in reverse order to force sorting.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles;
  profiles.emplace_back(std::move(profile_old));
  profiles.emplace_back(std::move(profile_new));

  CollectUserDataOptions options;
  options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_FULL));
  options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::EMAIL_ADDRESS));

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
  options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_FULL));
  options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::EMAIL_ADDRESS));
  options.required_contact_data_pieces.push_back(MakeRequiredDataPiece(
      autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER));
  options.default_email = "adam.west@gmail.com";

  EXPECT_THAT(GetDefaultContactProfile(options, profiles), 2);
}

TEST(UserDataUtilTest, SortsCompleteAddressesByUseDate) {
  base::Time current = base::Time::Now();

  auto profile_old = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_old.get(), "Adam", "", "West", "", "",
                                 "Brandschenkestrasse 110", "", "Zurich", "",
                                 "8002", "CH", "");
  profile_old->set_use_date(current - base::Days(2));

  auto profile_new = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_new.get(), "Adam", "", "West", "", "",
                                 "Brandschenkestrasse 110", "", "Zurich", "",
                                 "8002", "CH", "");
  profile_new->set_use_date(current);

  // Specify profiles in reverse order to force sorting.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles;
  profiles.emplace_back(std::move(profile_old));
  profiles.emplace_back(std::move(profile_new));

  CollectUserDataOptions options;

  std::vector<int> profile_indices =
      SortShippingAddressesByCompleteness(options, profiles);
  EXPECT_THAT(profile_indices, SizeIs(profiles.size()));
  EXPECT_THAT(profile_indices, ElementsAre(1, 0));
}

TEST(UserDataUtilTest, SortsAddressesByEditorCompleteness) {
  // Adding email address and phone number to demonstrate that they are not
  // checked for completeness.
  auto profile_no_street = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_no_street.get(), "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "Zurich",
                                 "", "8002", "CH", "+41");

  auto profile_complete = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_complete.get(), "Adam", "", "West", "",
                                 "", "Brandschenkestrasse 110", "", "Zurich",
                                 "", "8002", "CH", "");

  // Specify profiles in reverse order to force sorting.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles;
  profiles.emplace_back(std::move(profile_no_street));
  profiles.emplace_back(std::move(profile_complete));

  CollectUserDataOptions options;

  std::vector<int> profile_indices =
      SortShippingAddressesByCompleteness(options, profiles);
  EXPECT_THAT(profile_indices, SizeIs(profiles.size()));
  EXPECT_THAT(profile_indices, ElementsAre(1, 0));
}

TEST(UserDataUtilTest, SortsAddressesByAssistantCompleteness) {
  auto profile_no_email = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_no_email.get(), "Adam", "", "West", "",
                                 "", "Brandschenkestrasse 110", "", "Zurich",
                                 "", "8002", "CH", "");

  auto profile_complete = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(
      profile_complete.get(), "Adam", "", "West", "adam.west@gmail.com", "",
      "Brandschenkestrasse 110", "", "Zurich", "", "8002", "CH", "");

  // Specify profiles in reverse order to force sorting.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles;
  profiles.emplace_back(std::move(profile_no_email));
  profiles.emplace_back(std::move(profile_complete));

  CollectUserDataOptions options;
  options.required_shipping_address_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::EMAIL_ADDRESS));

  std::vector<int> profile_indices =
      SortShippingAddressesByCompleteness(options, profiles);
  EXPECT_THAT(profile_indices, SizeIs(profiles.size()));
  EXPECT_THAT(profile_indices, ElementsAre(1, 0));
}

TEST(UserDataUtilTest, GetDefaultAddressSelectionForEmptyProfiles) {
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles;
  CollectUserDataOptions options;

  EXPECT_THAT(GetDefaultShippingAddressProfile(options, profiles), -1);
}

TEST(UserDataUtilTest, GetDefaultAddressSelectionForCompleteProfiles) {
  base::Time current = base::Time::Now();

  // Adding email address and phone number to demonstrate that they are not
  // checked for completeness.
  auto profile_with_irrelevant_details =
      std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_with_irrelevant_details.get(), "Adam",
                                 "adam.west@gmail.com", "West", "", "",
                                 "Brandschenkestrasse 110", "", "Zurich", "",
                                 "8002", "CH", "+41");
  profile_with_irrelevant_details->set_use_date(current - base::Days(2));

  auto profile_complete = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_complete.get(), "Adam", "", "West", "",
                                 "", "Brandschenkestrasse 110", "", "Zurich",
                                 "", "8002", "CH", "");
  profile_complete->set_use_date(current);

  // Specify profiles in reverse order to force sorting.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles;
  profiles.emplace_back(std::move(profile_with_irrelevant_details));
  profiles.emplace_back(std::move(profile_complete));

  CollectUserDataOptions options;

  EXPECT_THAT(GetDefaultShippingAddressProfile(options, profiles), 1);
}

TEST(UserDataUtilTest, SortsCreditCardsByCompleteness) {
  auto complete_card = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(complete_card.get(), "Adam West",
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
  options.required_credit_card_data_pieces.push_back(MakeRequiredDataPiece(
      autofill::ServerFieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR));
  options.required_credit_card_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::CREDIT_CARD_EXP_MONTH));

  std::vector<int> sorted_indices =
      SortPaymentInstrumentsByCompleteness(options, payment_instruments);
  EXPECT_THAT(sorted_indices, SizeIs(payment_instruments.size()));
  EXPECT_THAT(sorted_indices, ElementsAre(1, 0));
}

TEST(UserDataUtilTest, SortsEquallyValidCardsByCardUseDate) {
  base::Time current = base::Time::Now();

  auto old_card = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(old_card.get(), "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "");
  old_card->set_use_date(current - base::Days(2));
  auto old_instrument =
      std::make_unique<PaymentInstrument>(std::move(old_card), nullptr);

  auto new_card = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(new_card.get(), "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "");
  new_card->set_use_date(current);
  auto new_instrument =
      std::make_unique<PaymentInstrument>(std::move(new_card), nullptr);

  // Specify payment instruments in reverse order to force sorting.
  std::vector<std::unique_ptr<PaymentInstrument>> payment_instruments;
  payment_instruments.emplace_back(std::move(old_instrument));
  payment_instruments.emplace_back(std::move(new_instrument));

  CollectUserDataOptions options;

  std::vector<int> sorted_indices =
      SortPaymentInstrumentsByCompleteness(options, payment_instruments);
  EXPECT_THAT(sorted_indices, SizeIs(payment_instruments.size()));
  EXPECT_THAT(sorted_indices, ElementsAre(1, 0));
}

TEST(UserDataUtilTest, SortsEquallyCompleteCardsByExpirationValidity) {
  auto invalid_card = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(invalid_card.get(), "Adam West",
                                    "4111111111111111", "1", "2000",
                                    /* billing_address_id= */ "");
  auto invalid_instrument =
      std::make_unique<PaymentInstrument>(std::move(invalid_card), nullptr);

  auto valid_card = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(valid_card.get(), "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "");
  auto valid_instrument =
      std::make_unique<PaymentInstrument>(std::move(valid_card), nullptr);

  // Specify payment instruments in reverse order to force sorting.
  std::vector<std::unique_ptr<PaymentInstrument>> payment_instruments;
  payment_instruments.emplace_back(std::move(invalid_instrument));
  payment_instruments.emplace_back(std::move(valid_instrument));

  CollectUserDataOptions options;

  std::vector<int> sorted_indices =
      SortPaymentInstrumentsByCompleteness(options, payment_instruments);
  EXPECT_THAT(sorted_indices, SizeIs(payment_instruments.size()));
  EXPECT_THAT(sorted_indices, ElementsAre(1, 0));
}

TEST(UserDataUtilTest, SortsEquallyCompleteCardsByNumberValidity) {
  auto invalid_card = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(invalid_card.get(), "Adam West", "41111",
                                    "1", "2050",
                                    /* billing_address_id= */ "");
  auto invalid_instrument =
      std::make_unique<PaymentInstrument>(std::move(invalid_card), nullptr);

  auto valid_card = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(valid_card.get(), "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "");
  auto valid_instrument =
      std::make_unique<PaymentInstrument>(std::move(valid_card), nullptr);

  // Specify payment instruments in reverse order to force sorting.
  std::vector<std::unique_ptr<PaymentInstrument>> payment_instruments;
  payment_instruments.emplace_back(std::move(invalid_instrument));
  payment_instruments.emplace_back(std::move(valid_instrument));

  CollectUserDataOptions options;

  std::vector<int> sorted_indices =
      SortPaymentInstrumentsByCompleteness(options, payment_instruments);
  EXPECT_THAT(sorted_indices, SizeIs(payment_instruments.size()));
  EXPECT_THAT(sorted_indices, ElementsAre(1, 0));
}

TEST(UserDataUtilTest, SortsCreditCardsByAddressCompleteness) {
  auto card_with_complete_address = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(card_with_complete_address.get(),
                                    "Adam West", "4111111111111111", "1",
                                    "2050",
                                    /* billing_address_id= */ "address-1");
  auto billing_address_with_zip = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(
      billing_address_with_zip.get(), "Adam", "", "West", "adam.west@gmail.com",
      "", "Baker Street 221b", "", "London", "", "WC2N 5DU", "UK", "+44");
  auto instrument_with_complete_address =
      std::make_unique<PaymentInstrument>(std::move(card_with_complete_address),
                                          std::move(billing_address_with_zip));

  auto card_with_incomplete_address = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(card_with_incomplete_address.get(),
                                    "Adam West", "4111111111111111", "1",
                                    "2050",
                                    /* billing_address_id= */ "address-1");
  auto billing_address_without_zip =
      std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(billing_address_without_zip.get(), "Adam", "",
                                 "West", "adam.west@gmail.com", "",
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
  options.required_billing_address_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::ADDRESS_HOME_ZIP));

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
  base::Time current = base::Time::Now();

  auto old_card = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(old_card.get(), "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "");
  old_card->set_use_date(current - base::Days(2));
  auto old_instrument =
      std::make_unique<PaymentInstrument>(std::move(old_card), nullptr);

  auto new_card = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(new_card.get(), "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "");
  new_card->set_use_date(current);
  auto new_instrument =
      std::make_unique<PaymentInstrument>(std::move(new_card), nullptr);

  // Specify payment instruments in reverse order to force sorting.
  std::vector<std::unique_ptr<PaymentInstrument>> payment_instruments;
  payment_instruments.emplace_back(std::move(old_instrument));
  payment_instruments.emplace_back(std::move(new_instrument));

  CollectUserDataOptions options;

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
  EXPECT_THAT(GetContactValidationErrors(nullptr, not_required_options),
              IsEmpty());
}

TEST(UserDataUtilTest, ContactCompletenessRequireName) {
  autofill::AutofillProfile contact;
  CollectUserDataOptions require_name_options;
  require_name_options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_FIRST));
  require_name_options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_LAST));

  EXPECT_THAT(GetContactValidationErrors(nullptr, require_name_options),
              ElementsAre("3", "5"));
  autofill::test::SetProfileInfo(&contact, /* first_name= */ "",
                                 /* middle_name= */ "",
                                 /* last_name= */ "", "adam.west@gmail.com", "",
                                 "", "", "", "", "", "", "+41");
  EXPECT_THAT(GetContactValidationErrors(&contact, require_name_options),
              ElementsAre("3", "5"));
  autofill::test::SetProfileInfo(&contact, "John", /* middle_name= */ "",
                                 /* last_name= */ "", "", "", "", "", "", "",
                                 "", "", "");
  EXPECT_THAT(GetContactValidationErrors(&contact, require_name_options),
              ElementsAre("5"));
  autofill::test::SetProfileInfo(&contact, "John", /* middle_name= */ "", "Doe",
                                 "", "", "", "", "", "", "", "", "");
  EXPECT_THAT(GetContactValidationErrors(&contact, require_name_options),
              IsEmpty());
}

TEST(UserDataUtilTest, ContactCompletenessRequireEmail) {
  autofill::AutofillProfile contact;
  CollectUserDataOptions require_email_options;
  require_email_options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::EMAIL_ADDRESS));

  EXPECT_THAT(GetContactValidationErrors(nullptr, require_email_options),
              ElementsAre("9"));
  autofill::test::SetProfileInfo(&contact, "John", "", "Doe",
                                 /* email= */ "", "", "", "", "", "", "", "",
                                 "+41");
  EXPECT_THAT(GetContactValidationErrors(&contact, require_email_options),
              ElementsAre("9"));
  autofill::test::SetProfileInfo(&contact, "John", "", "Doe",
                                 "john.doe@gmail.com", "", "", "", "", "", "",
                                 "", "+41");
  EXPECT_THAT(GetContactValidationErrors(&contact, require_email_options),
              IsEmpty());
}

TEST(UserDataUtilTest, ContactCompletenessRequirePhone) {
  autofill::AutofillProfile contact;
  CollectUserDataOptions require_phone_options;
  require_phone_options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(
          autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER));
  require_phone_options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::PHONE_HOME_NUMBER));
  require_phone_options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(
          autofill::ServerFieldType::PHONE_HOME_COUNTRY_CODE));

  EXPECT_THAT(GetContactValidationErrors(nullptr, require_phone_options),
              ElementsAre("14", "10", "12"));
  autofill::test::SetProfileInfo(&contact, "John", "", "Doe",
                                 "john.doe@gmail.com", "", "", "", "", "", "",
                                 "",
                                 /* phone= */ "");
  EXPECT_THAT(GetContactValidationErrors(&contact, require_phone_options),
              ElementsAre("14", "10", "12"));
  autofill::test::SetProfileInfo(&contact, "", "", "", "", "", "", "", "", "",
                                 "", "", "079 123 45 67");
  EXPECT_THAT(GetContactValidationErrors(&contact, require_phone_options),
              ElementsAre("12"));
  autofill::test::SetProfileInfo(&contact, "", "", "", "", "", "", "", "", "",
                                 "", "", "+41 79 123 45 67");
  EXPECT_THAT(GetContactValidationErrors(&contact, require_phone_options),
              IsEmpty());
}

TEST(UserDataUtilTest, CompleteShippingAddressNotRequired) {
  CollectUserDataOptions not_required_options;
  not_required_options.request_shipping = false;

  EXPECT_THAT(GetShippingAddressValidationErrors(nullptr, not_required_options),
              IsEmpty());
}

TEST(UserDataUtilTest, CompleteShippingAddressForAssistant) {
  autofill::AutofillProfile address;
  CollectUserDataOptions require_shipping_options;
  require_shipping_options.request_shipping = true;
  require_shipping_options.required_shipping_address_data_pieces.push_back(
      MakeRequiredDataPiece(
          autofill::ServerFieldType::ADDRESS_HOME_STREET_ADDRESS));
  require_shipping_options.required_shipping_address_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::ADDRESS_HOME_ZIP));
  require_shipping_options.required_shipping_address_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::ADDRESS_HOME_COUNTRY));

  EXPECT_THAT(
      GetShippingAddressValidationErrors(nullptr, require_shipping_options),
      ElementsAre("77", "35", "36"));
  autofill::test::SetProfileInfo(&address, "John", "", "Doe",
                                 "john.doe@gmail.com", "", /* address1= */ "",
                                 /* address2= */ "", /* city= */ "",
                                 /* state=  */ "", /* zip_code=  */ "",
                                 /* country= */ "", /* phone= */ "");
  EXPECT_THAT(
      GetShippingAddressValidationErrors(&address, require_shipping_options),
      ElementsAre("77", "35", "36"));
  autofill::test::SetProfileInfo(&address, "John", "", "Doe",
                                 /* email= */ "", "", "Brandschenkestrasse 110",
                                 "", "Zurich", "Zurich", /* zip_code= */ "",
                                 "CH",
                                 /* phone= */ "");
  EXPECT_THAT(
      GetShippingAddressValidationErrors(&address, require_shipping_options),
      ElementsAre("35"));
  autofill::test::SetProfileInfo(&address, "John", "", "Doe",
                                 /* email= */ "", "", "Brandschenkestrasse 110",
                                 "", "Zurich", "Zurich", "8002", "CH",
                                 /* phone= */ "");
  EXPECT_THAT(
      GetShippingAddressValidationErrors(&address, require_shipping_options),
      IsEmpty());
}

TEST(UserDataUtilTest, CompleteShippingAddressForEditor) {
  autofill::AutofillProfile address;
  CollectUserDataOptions require_shipping_options;
  require_shipping_options.request_shipping = true;

  EXPECT_THAT(
      GetShippingAddressValidationErrors(nullptr, require_shipping_options),
      ElementsAre(_));
  autofill::test::SetProfileInfo(&address, "John", "", "Doe",
                                 /* email= */ "", "", "Brandschenkestrasse 110",
                                 "", "Zurich", "Zurich", /* zip_code= */ "",
                                 "CH",
                                 /* phone= */ "");
  EXPECT_THAT(
      GetShippingAddressValidationErrors(&address, require_shipping_options),
      ElementsAre(_));
  autofill::test::SetProfileInfo(&address, "John", "", "Doe",
                                 /* email= */ "", "", "Brandschenkestrasse 110",
                                 "", "Zurich", "Zurich", "8002", "CH",
                                 /* phone= */ "");
  EXPECT_THAT(
      GetShippingAddressValidationErrors(&address, require_shipping_options),
      IsEmpty());
}

TEST(UserDataUtilTest, CompleteCreditCardNotRequired) {
  CollectUserDataOptions not_required_options;
  not_required_options.request_payment_method = false;

  EXPECT_THAT(GetPaymentInstrumentValidationErrors(nullptr, nullptr,
                                                   not_required_options),
              IsEmpty());
}

TEST(UserDataUtilTest, CompleteCreditCardAddressValidation) {
  CollectUserDataOptions payment_options;
  payment_options.request_payment_method = true;

  autofill::AutofillProfile address;
  autofill::CreditCard card;
  autofill::test::SetCreditCardInfo(&card, "Adam West", "4111111111111111", "1",
                                    "2050",
                                    /* billing_address_id= */ "id");

  EXPECT_THAT(
      GetPaymentInstrumentValidationErrors(nullptr, nullptr, payment_options),
      ElementsAre(_));
  EXPECT_THAT(
      GetPaymentInstrumentValidationErrors(&card, nullptr, payment_options),
      ElementsAre(_));
  EXPECT_THAT(
      GetPaymentInstrumentValidationErrors(&card, &address, payment_options),
      ElementsAre(_));
  // CH addresses require a zip code to be complete. This check outranks the
  // our validation.
  autofill::test::SetProfileInfo(&address, "John", "", "Doe",
                                 /* email= */ "", "", "Brandschenkestrasse 110",
                                 "", "Zurich", "Zurich",
                                 /* zipcode= */ "", "CH", /* phone= */ "");
  EXPECT_THAT(
      GetPaymentInstrumentValidationErrors(&card, &address, payment_options),
      ElementsAre(_));
  // UK addresses do not require a zip code, they are complete without it.
  autofill::test::SetProfileInfo(&address, "John", "", "Doe",
                                 /* email= */ "", "", "Baker Street 221b", "",
                                 "London", /* state= */ "",
                                 /* zipcode= */ "", "UK", /* phone= */ "");
  EXPECT_THAT(
      GetPaymentInstrumentValidationErrors(&card, &address, payment_options),
      IsEmpty());
  payment_options.required_billing_address_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::ADDRESS_HOME_ZIP));
  EXPECT_THAT(
      GetPaymentInstrumentValidationErrors(&card, &address, payment_options),
      ElementsAre("35"));
}

TEST(UserDataUtilTest, CompleteExpiredCreditCard) {
  CollectUserDataOptions payment_options;
  payment_options.request_payment_method = true;
  payment_options.credit_card_expired_text = "expired";

  autofill::AutofillProfile address;
  autofill::test::SetProfileInfo(
      &address, "John", "", "Doe", "john.doe@gmail.com", "",
      "Brandschenkestrasse 110", "", "Zurich", "Zurich", "8002", "CH", "+41");
  autofill::CreditCard card;

  autofill::test::SetCreditCardInfo(&card, "Adam West", "4111111111111111", "1",
                                    "2000",
                                    /* billing_address_id= */ "id");
  EXPECT_THAT(
      GetPaymentInstrumentValidationErrors(&card, &address, payment_options),
      ElementsAre("expired"));
  autofill::test::SetCreditCardInfo(&card, "Adam West", "4111111111111111", "1",
                                    "2050",
                                    /* billing_address_id= */ "id");
  EXPECT_THAT(
      GetPaymentInstrumentValidationErrors(&card, &address, payment_options),
      IsEmpty());
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

  RequiredDataPiece required_data_piece;
  required_data_piece.set_error_message("network");
  required_data_piece.mutable_condition()->set_key(
      static_cast<int>(AutofillFormatProto::CREDIT_CARD_NETWORK));
  required_data_piece.mutable_condition()
      ->mutable_regexp()
      ->mutable_text_filter()
      ->set_re2("^(mastercard)$");
  CollectUserDataOptions payment_options_mastercard;
  payment_options_mastercard.request_payment_method = true;
  payment_options_mastercard.required_credit_card_data_pieces.push_back(
      required_data_piece);
  payment_options_mastercard.supported_basic_card_networks.emplace_back(
      "mastercard");
  EXPECT_THAT(GetPaymentInstrumentValidationErrors(&card, &address,
                                                   payment_options_mastercard),
              ElementsAre("network"));

  required_data_piece.mutable_condition()
      ->mutable_regexp()
      ->mutable_text_filter()
      ->set_re2("^(mastercard|visa)$");
  CollectUserDataOptions payment_options_visa;
  payment_options_visa.request_payment_method = true;
  payment_options_visa.required_credit_card_data_pieces.push_back(
      required_data_piece);
  EXPECT_THAT(GetPaymentInstrumentValidationErrors(&card, &address,
                                                   payment_options_visa),
              IsEmpty());
}

TEST(UserDataUtilTest, CompleteCreditCardWithInvalidNumber) {
  CollectUserDataOptions payment_options;
  payment_options.request_payment_method = true;

  autofill::AutofillProfile address;
  autofill::test::SetProfileInfo(
      &address, "John", "", "Doe", "john.doe@gmail.com", "",
      "Brandschenkestrasse 110", "", "Zurich", "Zurich", "8002", "CH", "+41");
  autofill::CreditCard card;

  autofill::test::SetCreditCardInfo(&card, "Adam West", "4111", "1", "2050",
                                    /* billing_address_id= */ "id");
  EXPECT_THAT(
      GetPaymentInstrumentValidationErrors(&card, &address, payment_options),
      ElementsAre(_));
  autofill::test::SetCreditCardInfo(&card, "Adam West", "4111111111111111", "1",
                                    "2050",
                                    /* billing_address_id= */ "id");
  EXPECT_THAT(
      GetPaymentInstrumentValidationErrors(&card, &address, payment_options),
      IsEmpty());
}

class UserDataUtilTextValueTest : public testing::Test {
 public:
  UserDataUtilTextValueTest() {}

  void SetUp() override {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &browser_context_, nullptr);

    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));
    ON_CALL(mock_action_delegate_, GetWebsiteLoginManager)
        .WillByDefault(Return(&mock_website_login_manager_));
  }

  MOCK_METHOD2(OnResult, void(const ClientStatus&, const std::string&));

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  content::TestBrowserContext browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;
  MockActionDelegate mock_action_delegate_;
  UserData user_data_;
  UserModel user_model_;
  MockWebsiteLoginManager mock_website_login_manager_;
};

TEST_F(UserDataUtilTextValueTest, RequestEmptyAutofillValue) {
  AutofillValue autofill_value;

  std::string result;
  EXPECT_EQ(GetFormattedClientValue(autofill_value, user_data_, &result)
                .proto_status(),
            INVALID_ACTION);
  EXPECT_EQ(result, "");
}

TEST_F(UserDataUtilTextValueTest, ValueExpressionResultIsEmpty) {
  AutofillValue client_value;
  client_value.mutable_value_expression()->add_chunk()->set_text("");

  std::string result;
  EXPECT_EQ(
      GetFormattedClientValue(client_value, user_data_, &result).proto_status(),
      EMPTY_VALUE_EXPRESSION_RESULT);
  EXPECT_EQ(result, "");
}

TEST_F(UserDataUtilTextValueTest, RequestDataFromUnknownProfile) {
  AutofillValue autofill_value;
  autofill_value.mutable_profile()->set_identifier("none");
  autofill_value.mutable_value_expression()->add_chunk()->set_text("text");

  std::string result;
  EXPECT_EQ(GetFormattedClientValue(autofill_value, user_data_, &result)
                .proto_status(),
            PRECONDITION_FAILED);
  EXPECT_EQ(result, "");
}

TEST_F(UserDataUtilTextValueTest, RequestUnknownDataFromKnownProfile) {
  autofill::AutofillProfile contact(base::GenerateGUID(),
                                    autofill::test::kEmptyOrigin);
  // Middle name is expected to be empty.
  autofill::test::SetProfileInfo(&contact, "John", /* middle name */ "", "Doe",
                                 "", "", "", "", "", "", "", "", "");
  user_model_.SetSelectedAutofillProfile(
      "contact", std::make_unique<autofill::AutofillProfile>(contact),
      &user_data_);

  AutofillValue autofill_value;
  autofill_value.mutable_profile()->set_identifier("contact");
  autofill_value.mutable_value_expression()->add_chunk()->set_key(
      static_cast<int>(autofill::ServerFieldType::NAME_MIDDLE));

  std::string result;
  EXPECT_EQ(GetFormattedClientValue(autofill_value, user_data_, &result)
                .proto_status(),
            AUTOFILL_INFO_NOT_AVAILABLE);
  EXPECT_EQ(result, "");
}

TEST_F(UserDataUtilTextValueTest, RequestKnownDataFromKnownProfile) {
  autofill::AutofillProfile contact(base::GenerateGUID(),
                                    autofill::test::kEmptyOrigin);
  autofill::test::SetProfileInfo(&contact, "John", /* middle name */ "", "Doe",
                                 "", "", "", "", "", "", "", "", "");
  user_model_.SetSelectedAutofillProfile(
      "contact", std::make_unique<autofill::AutofillProfile>(contact),
      &user_data_);

  AutofillValue autofill_value;
  autofill_value.mutable_profile()->set_identifier("contact");
  autofill_value.mutable_value_expression()->add_chunk()->set_key(
      static_cast<int>(autofill::ServerFieldType::NAME_FIRST));

  std::string result;
  EXPECT_TRUE(
      GetFormattedClientValue(autofill_value, user_data_, &result).ok());
  EXPECT_EQ(result, "John");
}

TEST_F(UserDataUtilTextValueTest, EscapeDataFromProfile) {
  autofill::AutofillProfile contact(base::GenerateGUID(),
                                    autofill::test::kEmptyOrigin);
  autofill::test::SetProfileInfo(&contact, "Jo.h*n", /* middle name */ "",
                                 "Doe", "", "", "", "", "", "", "", "", "");
  user_model_.SetSelectedAutofillProfile(
      "contact", std::make_unique<autofill::AutofillProfile>(contact),
      &user_data_);

  AutofillValueRegexp autofill_value;
  autofill_value.mutable_profile()->set_identifier("contact");
  *autofill_value.mutable_value_expression_re2()->mutable_value_expression() =
      test_util::ValueExpressionBuilder()
          .addChunk("^")
          .addChunk(autofill::ServerFieldType::NAME_FIRST)
          .addChunk("$")
          .toProto();

  std::string result;
  EXPECT_TRUE(
      GetFormattedClientValue(autofill_value, user_data_, &result).ok());
  EXPECT_EQ(result, "^Jo\\.h\\*n$");
}

TEST_F(UserDataUtilTextValueTest, RequestLocalizedProfileData) {
  autofill::AutofillProfile contact(base::GenerateGUID(),
                                    autofill::test::kEmptyOrigin);
  autofill::test::SetProfileInfo(&contact, "John", /* middle name */ "", "Doe",
                                 "", "", "", "", "", "", "", "CH", "");
  user_model_.SetSelectedAutofillProfile(
      "contact", std::make_unique<autofill::AutofillProfile>(contact),
      &user_data_);

  AutofillValue autofill_value;
  autofill_value.mutable_profile()->set_identifier("contact");
  autofill_value.mutable_value_expression()->add_chunk()->set_key(
      static_cast<int>(autofill::ServerFieldType::ADDRESS_HOME_COUNTRY));

  std::string result_default;
  EXPECT_TRUE(
      GetFormattedClientValue(autofill_value, user_data_, &result_default)
          .ok());
  EXPECT_EQ(result_default, "Switzerland");

  autofill_value.set_locale("de-CH");
  std::string result_localized;
  EXPECT_TRUE(
      GetFormattedClientValue(autofill_value, user_data_, &result_localized)
          .ok());
  EXPECT_EQ(result_localized, "Schweiz");
}

TEST_F(UserDataUtilTextValueTest, RequestDataFromUnknownCreditCard) {
  AutofillValue autofill_value;
  autofill_value.mutable_value_expression()->add_chunk()->set_key(
      static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL));

  std::string result;
  EXPECT_EQ(GetFormattedClientValue(autofill_value, user_data_, &result)
                .proto_status(),
            AUTOFILL_INFO_NOT_AVAILABLE);
  EXPECT_EQ(result, "");
}

TEST_F(UserDataUtilTextValueTest, RequestUnknownDataFromKnownCreditCard) {
  autofill::CreditCard credit_card(base::GenerateGUID(),
                                   autofill::test::kEmptyOrigin);
  autofill::test::SetCreditCardInfo(&credit_card, "John Doe",
                                    "4111 1111 1111 1111", "01", "2050", "");
  user_model_.SetSelectedCreditCard(
      std::make_unique<autofill::CreditCard>(credit_card), &user_data_);

  AutofillValue autofill_value;
  autofill_value.mutable_value_expression()->add_chunk()->set_key(
      static_cast<int>(AutofillFormatProto::CREDIT_CARD_VERIFICATION_CODE));

  std::string result;
  EXPECT_EQ(GetFormattedClientValue(autofill_value, user_data_, &result)
                .proto_status(),
            AUTOFILL_INFO_NOT_AVAILABLE);
  EXPECT_EQ(result, "");
}

TEST_F(UserDataUtilTextValueTest, RequestDataFromKnownCreditCard) {
  autofill::CreditCard credit_card(base::GenerateGUID(),
                                   autofill::test::kEmptyOrigin);
  autofill::test::SetCreditCardInfo(&credit_card, "John Doe",
                                    "4111 1111 1111 1111", "01", "2050", "");
  user_model_.SetSelectedCreditCard(
      std::make_unique<autofill::CreditCard>(credit_card), &user_data_);

  AutofillValue autofill_value;
  autofill_value.mutable_value_expression()->add_chunk()->set_key(
      static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL));

  std::string result;
  EXPECT_TRUE(
      GetFormattedClientValue(autofill_value, user_data_, &result).ok());
  EXPECT_EQ(result, "John Doe");
}

TEST_F(UserDataUtilTextValueTest, RequestUnknownMemoryKey) {
  AutofillValue client_value;
  client_value.mutable_value_expression()->add_chunk()->set_memory_key("_val0");

  std::string result;
  EXPECT_EQ(
      GetFormattedClientValue(client_value, user_data_, &result).proto_status(),
      CLIENT_MEMORY_KEY_NOT_AVAILABLE);
  EXPECT_EQ(result, "");
}

TEST_F(UserDataUtilTextValueTest, RequestKnownMemoryKey) {
  user_data_.SetAdditionalValue("key", SimpleValue(std::string("Hello...")));

  std::string result;

  AutofillValue client_value;
  client_value.mutable_value_expression()->add_chunk()->set_memory_key("key");
  EXPECT_TRUE(GetFormattedClientValue(client_value, user_data_, &result).ok());
  EXPECT_EQ(result, "Hello...");

  AutofillValueRegexp client_value_regexp;
  client_value_regexp.mutable_value_expression_re2()
      ->mutable_value_expression()
      ->add_chunk()
      ->set_memory_key("key");
  EXPECT_TRUE(
      GetFormattedClientValue(client_value_regexp, user_data_, &result).ok());
  EXPECT_EQ(result, "Hello\\.\\.\\.");
}

TEST_F(UserDataUtilTextValueTest, RequestEmptyKnownMemoryKey) {
  user_data_.SetAdditionalValue("key", SimpleValue(std::string()));

  AutofillValue client_value;
  client_value.mutable_value_expression()->add_chunk()->set_memory_key("key");

  std::string result;
  EXPECT_EQ(
      GetFormattedClientValue(client_value, user_data_, &result).proto_status(),
      EMPTY_VALUE_EXPRESSION_RESULT);
  EXPECT_EQ(result, "");
}

TEST_F(UserDataUtilTextValueTest,
       NoKeyCollisionBetweenAutofillAndClientMemory) {
  int expMonthKey =
      static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_EXP_MONTH);

  autofill::CreditCard credit_card(base::GenerateGUID(),
                                   autofill::test::kEmptyOrigin);
  autofill::test::SetCreditCardInfo(&credit_card, "John Doe",
                                    "4111 1111 1111 1111", "01", "2050", "");
  user_model_.SetSelectedCreditCard(
      std::make_unique<autofill::CreditCard>(credit_card), &user_data_);

  user_data_.SetAdditionalValue(base::NumberToString(expMonthKey),
                                SimpleValue(std::string("January")));

  AutofillValue client_value;
  client_value.mutable_value_expression()->add_chunk()->set_key(expMonthKey);
  client_value.mutable_value_expression()->add_chunk()->set_text(" ");
  client_value.mutable_value_expression()->add_chunk()->set_memory_key(
      base::NumberToString(expMonthKey));

  std::string result;
  EXPECT_TRUE(GetFormattedClientValue(client_value, user_data_, &result).ok());
  EXPECT_EQ(result, "01 January");
}

TEST_F(UserDataUtilTextValueTest, GetUsername) {
  user_data_.selected_login_ = absl::make_optional<WebsiteLoginManager::Login>(
      GURL("https://www.example.com"), "username");

  ElementFinder::Result element;
  element.container_frame_host = web_contents_->GetMainFrame();

  PasswordManagerValue password_manager_value;
  password_manager_value.set_credential_type(PasswordManagerValue::USERNAME);

  EXPECT_CALL(*this, OnResult(EqualsStatus(OkClientStatus()), "username"));

  GetPasswordManagerValue(password_manager_value, element, &user_data_,
                          &mock_website_login_manager_,
                          base::BindOnce(&UserDataUtilTextValueTest::OnResult,
                                         base::Unretained(this)));
}

TEST_F(UserDataUtilTextValueTest, GetStoredPassword) {
  user_data_.selected_login_ = absl::make_optional<WebsiteLoginManager::Login>(
      GURL("https://www.example.com"), "username");

  ElementFinder::Result element;
  element.container_frame_host = web_contents_->GetMainFrame();

  PasswordManagerValue password_manager_value;
  password_manager_value.set_credential_type(PasswordManagerValue::PASSWORD);

  EXPECT_CALL(mock_website_login_manager_, GetPasswordForLogin(_, _))
      .WillOnce(RunOnceCallback<1>(true, "password"));
  EXPECT_CALL(*this, OnResult(EqualsStatus(OkClientStatus()), "password"));

  GetPasswordManagerValue(password_manager_value, element, &user_data_,
                          &mock_website_login_manager_,
                          base::BindOnce(&UserDataUtilTextValueTest::OnResult,
                                         base::Unretained(this)));
}

TEST_F(UserDataUtilTextValueTest, GetStoredPasswordFails) {
  user_data_.selected_login_ = absl::make_optional<WebsiteLoginManager::Login>(
      GURL("https://www.example.com"), "username");

  ElementFinder::Result element;
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://www.example.com"));
  element.container_frame_host = web_contents_->GetMainFrame();

  PasswordManagerValue password_manager_value;
  password_manager_value.set_credential_type(PasswordManagerValue::PASSWORD);

  EXPECT_CALL(mock_website_login_manager_, GetPasswordForLogin(_, _))
      .WillOnce(RunOnceCallback<1>(false, std::string()));
  EXPECT_CALL(*this,
              OnResult(EqualsStatus(ClientStatus(AUTOFILL_INFO_NOT_AVAILABLE)),
                       std::string()));

  GetPasswordManagerValue(password_manager_value, element, &user_data_,
                          &mock_website_login_manager_,
                          base::BindOnce(&UserDataUtilTextValueTest::OnResult,
                                         base::Unretained(this)));
}

TEST_F(UserDataUtilTextValueTest, ClientMemoryKey) {
  user_data_.SetAdditionalValue("key", SimpleValue(std::string("Hello World")));

  std::string result;
  EXPECT_TRUE(GetClientMemoryStringValue("key", &user_data_, &result).ok());
  EXPECT_EQ(result, "Hello World");
}

TEST_F(UserDataUtilTextValueTest, EmptyClientMemoryKey) {
  std::string result;
  EXPECT_EQ(INVALID_ACTION,
            GetClientMemoryStringValue(std::string(), &user_data_, &result)
                .proto_status());
}

TEST_F(UserDataUtilTextValueTest, NonExistingClientMemoryKey) {
  std::string result;
  EXPECT_EQ(
      PRECONDITION_FAILED,
      GetClientMemoryStringValue("key", &user_data_, &result).proto_status());
}

TEST_F(UserDataUtilTextValueTest, TextValueText) {
  TextValue text_value;
  text_value.set_text("text");

  EXPECT_CALL(*this, OnResult(EqualsStatus(OkClientStatus()), "text"));

  ResolveTextValue(text_value, ElementFinder::Result(), &mock_action_delegate_,
                   base::BindOnce(&UserDataUtilTextValueTest::OnResult,
                                  base::Unretained(this)));
}

TEST_F(UserDataUtilTextValueTest, TextValueAutofillValue) {
  autofill::AutofillProfile contact(base::GenerateGUID(),
                                    autofill::test::kEmptyOrigin);
  autofill::test::SetProfileInfo(&contact, "John", /* middle name */ "", "Doe",
                                 "", "", "", "", "", "", "", "", "");
  user_model_.SetSelectedAutofillProfile(
      "contact", std::make_unique<autofill::AutofillProfile>(contact),
      &user_data_);

  TextValue text_value;
  text_value.mutable_autofill_value()->mutable_profile()->set_identifier(
      "contact");
  text_value.mutable_autofill_value()
      ->mutable_value_expression()
      ->add_chunk()
      ->set_key(static_cast<int>(autofill::ServerFieldType::NAME_FIRST));

  EXPECT_CALL(*this, OnResult(EqualsStatus(OkClientStatus()), "John"));

  ResolveTextValue(text_value, ElementFinder::Result(), &mock_action_delegate_,
                   base::BindOnce(&UserDataUtilTextValueTest::OnResult,
                                  base::Unretained(this)));
}

TEST_F(UserDataUtilTextValueTest, TextValuePasswordManagerValue) {
  user_data_.selected_login_ = absl::make_optional<WebsiteLoginManager::Login>(
      GURL("https://www.example.com"), "username");

  ElementFinder::Result element;
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://www.example.com"));
  element.container_frame_host = web_contents_->GetMainFrame();

  TextValue text_value;
  text_value.mutable_password_manager_value()->set_credential_type(
      PasswordManagerValue::PASSWORD);

  EXPECT_CALL(mock_website_login_manager_, GetPasswordForLogin(_, _))
      .WillOnce(RunOnceCallback<1>(true, "password"));
  EXPECT_CALL(*this, OnResult(EqualsStatus(OkClientStatus()), "password"));

  ResolveTextValue(text_value, element, &mock_action_delegate_,
                   base::BindOnce(&UserDataUtilTextValueTest::OnResult,
                                  base::Unretained(this)));
}

TEST_F(UserDataUtilTextValueTest, TextValueClientMemoryKey) {
  user_data_.SetAdditionalValue("key", SimpleValue(std::string("Hello World")));

  TextValue text_value;
  text_value.set_client_memory_key("key");

  EXPECT_CALL(*this, OnResult(EqualsStatus(OkClientStatus()), "Hello World"));

  ResolveTextValue(text_value, ElementFinder::Result(), &mock_action_delegate_,
                   base::BindOnce(&UserDataUtilTextValueTest::OnResult,
                                  base::Unretained(this)));
}

}  // namespace
}  // namespace user_data
}  // namespace autofill_assistant
