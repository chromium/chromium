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
#include "components/autofill_assistant/browser/test_util.h"
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

RequiredDataPiece MakeRequiredDataPiece(autofill::ServerFieldType field) {
  RequiredDataPiece required_data_piece;
  required_data_piece.set_error_message(
      base::NumberToString(static_cast<int>(field)));
  required_data_piece.mutable_condition()->set_key(static_cast<int>(field));
  required_data_piece.mutable_condition()->mutable_not_empty();
  return required_data_piece;
}

TEST(UserDataUtilTest, ConditionEvaluation) {
  autofill::AutofillProfile profile;
  autofill::test::SetProfileInfo(&profile, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Baker Street 221b",
                                 "", "London", "", "WC2N 5DU", "UK", "+44");

  CollectUserDataOptions options;
  options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_FULL));
  RequiredDataPiece email_data_piece;
  email_data_piece.mutable_condition()->set_key(
      static_cast<int>(autofill::ServerFieldType::EMAIL_ADDRESS));
  email_data_piece.mutable_condition()
      ->mutable_regexp()
      ->mutable_text_filter()
      ->set_re2("^.*@.*$");
  options.required_contact_data_pieces.push_back(email_data_piece);
  RequiredDataPiece middle_name_data_piece;
  middle_name_data_piece.mutable_condition()->set_key(
      static_cast<int>(autofill::ServerFieldType::NAME_MIDDLE));
  middle_name_data_piece.mutable_condition()
      ->mutable_regexp()
      ->mutable_text_filter()
      ->set_re2("^$");
  options.required_contact_data_pieces.push_back(middle_name_data_piece);

  EXPECT_THAT(GetContactValidationErrors(&profile, options), IsEmpty());
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

  std::vector<std::unique_ptr<Contact>> contacts;
  contacts.emplace_back(std::make_unique<Contact>(std::move(profile_first)));
  contacts.emplace_back(std::make_unique<Contact>(std::move(profile_second)));

  CollectUserDataOptions options;

  std::vector<int> sorted_indices =
      SortContactsByCompleteness(options, contacts);
  EXPECT_THAT(sorted_indices, ElementsAre(0, 1));
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

  // Specify contacts in reverse order to force sorting.
  std::vector<std::unique_ptr<Contact>> contacts;
  contacts.emplace_back(std::make_unique<Contact>(std::move(profile_old)));
  contacts.emplace_back(std::make_unique<Contact>(std::move(profile_new)));

  CollectUserDataOptions options;
  options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_FULL));
  options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::EMAIL_ADDRESS));

  std::vector<int> sorted_indices =
      SortContactsByCompleteness(options, contacts);
  EXPECT_THAT(sorted_indices, ElementsAre(1, 0));
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

  // Specify contacts in reverse order to force sorting.
  std::vector<std::unique_ptr<Contact>> contacts;
  contacts.emplace_back(
      std::make_unique<Contact>(std::move(profile_incomplete)));
  contacts.emplace_back(std::make_unique<Contact>(std::move(profile_no_phone)));
  contacts.emplace_back(std::make_unique<Contact>(std::move(profile_complete)));

  CollectUserDataOptions options;
  options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_FULL));
  options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::EMAIL_ADDRESS));
  options.required_contact_data_pieces.push_back(MakeRequiredDataPiece(
      autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER));

  std::vector<int> sorted_indices =
      SortContactsByCompleteness(options, contacts);
  EXPECT_THAT(sorted_indices, ElementsAre(2, 1, 0));
}

TEST(UserDataUtilTest, GetDefaultContactSelectionForEmptyList) {
  std::vector<std::unique_ptr<Contact>> contacts;
  CollectUserDataOptions options;

  EXPECT_THAT(GetDefaultContact(options, contacts), -1);
}

TEST(UserDataUtilTest, GetDefaultContactSelectionForCompleteContacts) {
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

  // Specify contacts in reverse order to force sorting.
  std::vector<std::unique_ptr<Contact>> contacts;
  contacts.emplace_back(std::make_unique<Contact>(std::move(profile_old)));
  contacts.emplace_back(std::make_unique<Contact>(std::move(profile_new)));

  CollectUserDataOptions options;
  options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_FULL));
  options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::EMAIL_ADDRESS));

  EXPECT_THAT(GetDefaultContact(options, contacts), 1);
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

  // Specify contacts in reverse order to force sorting.
  std::vector<std::unique_ptr<Contact>> contacts;
  contacts.emplace_back(std::make_unique<Contact>(std::move(profile_complete)));
  contacts.emplace_back(std::make_unique<Contact>(
      std::move(profile_incomplete_with_default_email)));
  contacts.emplace_back(std::make_unique<Contact>(
      std::move(profile_complete_with_default_email)));

  CollectUserDataOptions options;
  options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::NAME_FULL));
  options.required_contact_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::EMAIL_ADDRESS));
  options.required_contact_data_pieces.push_back(MakeRequiredDataPiece(
      autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER));
  options.default_email = "adam.west@gmail.com";

  EXPECT_THAT(GetDefaultContact(options, contacts), 2);
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

  // Specify addresses in reverse order to force sorting.
  std::vector<std::unique_ptr<Address>> addresses;
  addresses.emplace_back(std::make_unique<Address>(std::move(profile_old)));
  addresses.emplace_back(std::make_unique<Address>(std::move(profile_new)));

  CollectUserDataOptions options;

  std::vector<int> sorted_indices =
      SortShippingAddressesByCompleteness(options, addresses);
  EXPECT_THAT(sorted_indices, ElementsAre(1, 0));
}

TEST(UserDataUtilTest, SortsPhoneNumbers) {
  auto profile_complete = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_complete.get(), "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "+1 23 456 789 01");

  auto profile_incomplete = std::make_unique<autofill::AutofillProfile>();
  profile_incomplete->SetRawInfo(
      autofill::ServerFieldType::PHONE_HOME_COUNTRY_CODE, u"1");

  // Specify contacts in reverse order to force sorting.
  std::vector<std::unique_ptr<PhoneNumber>> phone_numbers;
  phone_numbers.emplace_back(
      std::make_unique<PhoneNumber>(std::move(profile_incomplete)));
  phone_numbers.emplace_back(
      std::make_unique<PhoneNumber>(std::move(profile_complete)));

  CollectUserDataOptions options;
  options.required_phone_number_data_pieces.push_back(MakeRequiredDataPiece(
      autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER));
  options.required_phone_number_data_pieces.push_back(MakeRequiredDataPiece(
      autofill::ServerFieldType::PHONE_HOME_COUNTRY_CODE));

  std::vector<int> sorted_indices =
      SortPhoneNumbersByCompleteness(options, phone_numbers);
  EXPECT_THAT(sorted_indices, ElementsAre(1, 0));
}

TEST(UserDataUtilTest, GetDefaultPhoneNumberSelectionForEmptyList) {
  std::vector<std::unique_ptr<PhoneNumber>> phone_numbers;
  CollectUserDataOptions options;

  EXPECT_THAT(GetDefaultPhoneNumber(options, phone_numbers), -1);
}

TEST(UserDataUtilTest, GetDefaultPhoneNumberSelection) {
  auto profile_complete = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_complete.get(), "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "+1 23 456 789 01");

  auto profile_incomplete = std::make_unique<autofill::AutofillProfile>();
  profile_incomplete->SetRawInfo(
      autofill::ServerFieldType::PHONE_HOME_COUNTRY_CODE, u"1");

  // Specify contacts in reverse order to force sorting.
  std::vector<std::unique_ptr<PhoneNumber>> phone_numbers;
  phone_numbers.emplace_back(
      std::make_unique<PhoneNumber>(std::move(profile_incomplete)));
  phone_numbers.emplace_back(
      std::make_unique<PhoneNumber>(std::move(profile_complete)));

  CollectUserDataOptions options;
  options.required_phone_number_data_pieces.push_back(MakeRequiredDataPiece(
      autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER));
  options.required_phone_number_data_pieces.push_back(MakeRequiredDataPiece(
      autofill::ServerFieldType::PHONE_HOME_COUNTRY_CODE));

  EXPECT_THAT(GetDefaultPhoneNumber(options, phone_numbers), 1);
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

  // Specify addresses in reverse order to force sorting.
  std::vector<std::unique_ptr<Address>> addresses;
  addresses.emplace_back(
      std::make_unique<Address>(std::move(profile_no_street)));
  addresses.emplace_back(
      std::make_unique<Address>(std::move(profile_complete)));

  CollectUserDataOptions options;

  std::vector<int> sorted_indices =
      SortShippingAddressesByCompleteness(options, addresses);
  EXPECT_THAT(sorted_indices, ElementsAre(1, 0));
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

  // Specify addresses in reverse order to force sorting.
  std::vector<std::unique_ptr<Address>> addresses;
  addresses.emplace_back(
      std::make_unique<Address>(std::move(profile_no_email)));
  addresses.emplace_back(
      std::make_unique<Address>(std::move(profile_complete)));

  CollectUserDataOptions options;
  options.required_shipping_address_data_pieces.push_back(
      MakeRequiredDataPiece(autofill::ServerFieldType::EMAIL_ADDRESS));

  std::vector<int> sorted_indices =
      SortShippingAddressesByCompleteness(options, addresses);
  EXPECT_THAT(sorted_indices, ElementsAre(1, 0));
}

TEST(UserDataUtilTest, GetDefaultAddressSelectionForEmptyList) {
  std::vector<std::unique_ptr<Address>> addresses;
  CollectUserDataOptions options;

  EXPECT_THAT(GetDefaultShippingAddress(options, addresses), -1);
}

TEST(UserDataUtilTest, GetDefaultAddressSelectionForCompleteAddresses) {
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

  // Specify addresses in reverse order to force sorting.
  std::vector<std::unique_ptr<Address>> addresses;
  addresses.emplace_back(
      std::make_unique<Address>(std::move(profile_with_irrelevant_details)));
  addresses.emplace_back(
      std::make_unique<Address>(std::move(profile_complete)));

  CollectUserDataOptions options;

  EXPECT_THAT(GetDefaultShippingAddress(options, addresses), 1);
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

TEST(UserDataUtilTest, CompletePhoneNumberNotRequired) {
  CollectUserDataOptions not_required_options;
  not_required_options.request_phone_number_separately = false;

  EXPECT_THAT(GetPhoneNumberValidationErrors(nullptr, not_required_options),
              IsEmpty());
}

TEST(UserDataUtilTest, CompletePhoneNumber) {
  autofill::AutofillProfile phone_number;
  CollectUserDataOptions options;
  options.required_phone_number_data_pieces.push_back(MakeRequiredDataPiece(
      autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER));

  EXPECT_THAT(GetPhoneNumberValidationErrors(nullptr, options),
              ElementsAre("14"));
  autofill::test::SetProfileInfo(&phone_number, /* first_name= */ "",
                                 /* middle_name= */ "",
                                 /* last_name= */ "", "", "", "", "", "", "",
                                 "", "", "+41");
  EXPECT_THAT(GetPhoneNumberValidationErrors(&phone_number, options),
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

TEST(UserDataUtilTest, GetNewSelectionState) {
  EXPECT_EQ(Metrics::UserDataSelectionState::NO_CHANGE,
            GetNewSelectionState(Metrics::UserDataSelectionState::NO_CHANGE,
                                 NO_NOTIFICATION));
  EXPECT_EQ(Metrics::UserDataSelectionState::SELECTED_DIFFERENT_ENTRY,
            GetNewSelectionState(Metrics::UserDataSelectionState::NO_CHANGE,
                                 SELECTION_CHANGED));
  EXPECT_EQ(Metrics::UserDataSelectionState::NEW_ENTRY,
            GetNewSelectionState(Metrics::UserDataSelectionState::NO_CHANGE,
                                 ENTRY_CREATED));

  EXPECT_EQ(Metrics::UserDataSelectionState::EDIT_PRESELECTED,
            GetNewSelectionState(Metrics::UserDataSelectionState::NO_CHANGE,
                                 ENTRY_EDITED));
  EXPECT_EQ(Metrics::UserDataSelectionState::NEW_ENTRY,
            GetNewSelectionState(Metrics::UserDataSelectionState::NEW_ENTRY,
                                 ENTRY_EDITED));
  EXPECT_EQ(
      Metrics::UserDataSelectionState::SELECTED_DIFFERENT_AND_MODIFIED_ENTRY,
      GetNewSelectionState(
          Metrics::UserDataSelectionState::SELECTED_DIFFERENT_ENTRY,
          ENTRY_EDITED));
  EXPECT_EQ(
      Metrics::UserDataSelectionState::SELECTED_DIFFERENT_AND_MODIFIED_ENTRY,
      GetNewSelectionState(Metrics::UserDataSelectionState::
                               SELECTED_DIFFERENT_AND_MODIFIED_ENTRY,
                           SELECTION_CHANGED));
}

class UserDataUtilTextValueTest : public testing::Test {
 public:
  UserDataUtilTextValueTest() {}

  void SetUp() override {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &browser_context_, nullptr);

    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));
    ON_CALL(mock_action_delegate_, GetUserModel)
        .WillByDefault(Return(&user_model_));
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

  ElementFinderResult element;
  element.SetRenderFrameHost(web_contents_->GetMainFrame());

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

  ElementFinderResult element;
  element.SetRenderFrameHost(web_contents_->GetMainFrame());

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

  ElementFinderResult element;
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://www.example.com"));
  element.SetRenderFrameHost(web_contents_->GetMainFrame());

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

TEST_F(UserDataUtilTextValueTest, ClientMemoryKeyFromUserData) {
  user_data_.SetAdditionalValue("key", SimpleValue(std::string("Hello World")));

  std::string result;
  EXPECT_TRUE(
      GetClientMemoryStringValue("key", &user_data_, &user_model_, &result)
          .ok());
  EXPECT_EQ(result, "Hello World");
}

TEST_F(UserDataUtilTextValueTest, ClientMemoryKeyFromUserModel) {
  user_model_.SetValue("key", SimpleValue(std::string("Hello World")));

  std::string result;
  EXPECT_TRUE(
      GetClientMemoryStringValue("key", &user_data_, &user_model_, &result)
          .ok());
  EXPECT_EQ(result, "Hello World");
}

TEST_F(UserDataUtilTextValueTest, ClientMemoryValueDifferentInDataAndModel) {
  user_data_.SetAdditionalValue(
      "key", SimpleValue(std::string("Hello from UserData")));
  user_model_.SetValue("key", SimpleValue(std::string("Hello from UserModel")));

  std::string result;
  EXPECT_EQ(PRECONDITION_FAILED, GetClientMemoryStringValue(
                                     "key", &user_data_, &user_model_, &result)
                                     .proto_status());
}

TEST_F(UserDataUtilTextValueTest, ClientMemoryValueDuplicateInDataAndModel) {
  user_data_.SetAdditionalValue("key", SimpleValue(std::string("Hello World")));
  user_model_.SetValue("key", SimpleValue(std::string("Hello World")));

  std::string result;
  EXPECT_TRUE(
      GetClientMemoryStringValue("key", &user_data_, &user_model_, &result)
          .ok());
  EXPECT_EQ(result, "Hello World");
}

TEST_F(UserDataUtilTextValueTest, EmptyClientMemoryKey) {
  std::string result;
  EXPECT_EQ(INVALID_ACTION,
            GetClientMemoryStringValue(std::string(), &user_data_, &user_model_,
                                       &result)
                .proto_status());
}

TEST_F(UserDataUtilTextValueTest, NonExistingClientMemoryKey) {
  std::string result;
  EXPECT_EQ(PRECONDITION_FAILED, GetClientMemoryStringValue(
                                     "key", &user_data_, &user_model_, &result)
                                     .proto_status());
}

TEST_F(UserDataUtilTextValueTest, TextValueText) {
  TextValue text_value;
  text_value.set_text("text");

  EXPECT_CALL(*this, OnResult(EqualsStatus(OkClientStatus()), "text"));

  ResolveTextValue(text_value, ElementFinderResult(), &mock_action_delegate_,
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

  ResolveTextValue(text_value, ElementFinderResult(), &mock_action_delegate_,
                   base::BindOnce(&UserDataUtilTextValueTest::OnResult,
                                  base::Unretained(this)));
}

TEST_F(UserDataUtilTextValueTest, TextValuePasswordManagerValue) {
  user_data_.selected_login_ = absl::make_optional<WebsiteLoginManager::Login>(
      GURL("https://www.example.com"), "username");

  ElementFinderResult element;
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://www.example.com"));
  element.SetRenderFrameHost(web_contents_->GetMainFrame());

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

  ResolveTextValue(text_value, ElementFinderResult(), &mock_action_delegate_,
                   base::BindOnce(&UserDataUtilTextValueTest::OnResult,
                                  base::Unretained(this)));
}

TEST_F(UserDataUtilTextValueTest, GetAddressFieldBitArray) {
  EXPECT_EQ(0, GetFieldBitArrayForAddress(nullptr));

  autofill::AutofillProfile empty_profile;
  EXPECT_EQ(0, GetFieldBitArrayForAddress(&empty_profile));

  autofill::AutofillProfile name_only;
  autofill::test::SetProfileInfo(&name_only, "Adam", "", "West", "", "", "", "",
                                 "", "", "", "", "");
  EXPECT_EQ(Metrics::AutofillAssistantProfileFields::NAME_FIRST |
                Metrics::AutofillAssistantProfileFields::NAME_LAST |
                Metrics::AutofillAssistantProfileFields::NAME_FULL,
            GetFieldBitArrayForAddress(&name_only));

  autofill::AutofillProfile full_profile;
  autofill::test::SetProfileInfo(&full_profile, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Baker Street 221b",
                                 "", "Chicago", "Illinois", "10000", "US",
                                 "+1 23 456 789 01");

  EXPECT_EQ(
      Metrics::AutofillAssistantProfileFields::NAME_FIRST |
          Metrics::AutofillAssistantProfileFields::NAME_LAST |
          Metrics::AutofillAssistantProfileFields::NAME_FULL |
          Metrics::AutofillAssistantProfileFields::EMAIL_ADDRESS |
          Metrics::AutofillAssistantProfileFields::ADDRESS_HOME_LINE1 |
          Metrics::AutofillAssistantProfileFields::ADDRESS_HOME_CITY |
          Metrics::AutofillAssistantProfileFields::ADDRESS_HOME_STATE |
          Metrics::AutofillAssistantProfileFields::ADDRESS_HOME_ZIP |
          Metrics::AutofillAssistantProfileFields::ADDRESS_HOME_COUNTRY |
          Metrics::AutofillAssistantProfileFields::PHONE_HOME_NUMBER |
          Metrics::AutofillAssistantProfileFields::PHONE_HOME_COUNTRY_CODE |
          Metrics::AutofillAssistantProfileFields::PHONE_HOME_WHOLE_NUMBER,
      GetFieldBitArrayForAddress(&full_profile));

  autofill::AutofillProfile contact_profile;
  autofill::test::SetProfileInfo(&contact_profile, "Adam", "", "West", "", "",
                                 "", "", "", "", "", "", "");
  autofill::AutofillProfile number_profile;
  autofill::test::SetProfileInfo(&number_profile, "", "", "", "", "", "", "",
                                 "", "", "", "", "+1 23 456 789 01");
  EXPECT_EQ(
      Metrics::AutofillAssistantProfileFields::NAME_FIRST |
          Metrics::AutofillAssistantProfileFields::NAME_LAST |
          Metrics::AutofillAssistantProfileFields::NAME_FULL |
          Metrics::AutofillAssistantProfileFields::PHONE_HOME_NUMBER |
          Metrics::AutofillAssistantProfileFields::PHONE_HOME_COUNTRY_CODE |
          Metrics::AutofillAssistantProfileFields::PHONE_HOME_WHOLE_NUMBER,
      GetFieldBitArrayForAddressAndPhoneNumber(&contact_profile,
                                               &number_profile));
}

TEST_F(UserDataUtilTextValueTest, GetCreditCardFieldBitArray) {
  EXPECT_EQ(0, GetFieldBitArrayForCreditCard(nullptr));

  autofill::CreditCard empty_card;
  EXPECT_EQ(0, GetFieldBitArrayForCreditCard(&empty_card));

  autofill::CreditCard name_only;
  autofill::test::SetCreditCardInfo(&name_only, "Adam West", "4111", "", "",
                                    /* billing_address_id= */ "");
  EXPECT_EQ(Metrics::AutofillAssistantCreditCardFields::CREDIT_CARD_NAME_FULL,
            GetFieldBitArrayForCreditCard(&name_only));

  autofill::CreditCard complete;
  autofill::test::SetCreditCardInfo(&complete, "Adam West", "4111111111111111",
                                    "1", "50",
                                    /* billing_address_id= */ "");
  EXPECT_EQ(
      Metrics::AutofillAssistantCreditCardFields::CREDIT_CARD_NAME_FULL |
          Metrics::AutofillAssistantCreditCardFields::CREDIT_CARD_EXP_MONTH |
          Metrics::AutofillAssistantCreditCardFields::
              CREDIT_CARD_EXP_2_DIGIT_YEAR |
          Metrics::AutofillAssistantCreditCardFields::
              CREDIT_CARD_EXP_4_DIGIT_YEAR |
          Metrics::AutofillAssistantCreditCardFields::VALID_NUMBER,
      GetFieldBitArrayForCreditCard(&complete));

  autofill::CreditCard masked;
  autofill::test::SetCreditCardInfo(&masked, "Adam West", "4111111111111111",
                                    "1", "50",
                                    /* billing_address_id= */ "");
  masked.set_record_type(autofill::CreditCard::MASKED_SERVER_CARD);
  EXPECT_EQ(
      Metrics::AutofillAssistantCreditCardFields::CREDIT_CARD_NAME_FULL |
          Metrics::AutofillAssistantCreditCardFields::CREDIT_CARD_EXP_MONTH |
          Metrics::AutofillAssistantCreditCardFields::
              CREDIT_CARD_EXP_2_DIGIT_YEAR |
          Metrics::AutofillAssistantCreditCardFields::
              CREDIT_CARD_EXP_4_DIGIT_YEAR |
          Metrics::AutofillAssistantCreditCardFields::MASKED |
          Metrics::AutofillAssistantCreditCardFields::VALID_NUMBER,
      GetFieldBitArrayForCreditCard(&masked));
}

TEST_F(UserDataUtilTextValueTest, ResolveSelectorUserData) {
  autofill::AutofillProfile contact(base::GenerateGUID(),
                                    autofill::test::kEmptyOrigin);
  autofill::test::SetProfileInfo(&contact, "Jo.h*n", /* middle name */ "",
                                 "Doe", "", "", "", "", "", "", "", "", "");
  user_model_.SetSelectedAutofillProfile(
      "contact", std::make_unique<autofill::AutofillProfile>(contact),
      &user_data_);

  SelectorProto selector;
  selector.add_filters()->set_css_selector("#test");

  auto* filter = selector.add_filters();
  auto* value = filter->mutable_property()->mutable_autofill_value_regexp();
  value->mutable_profile()->set_identifier("contact");
  auto* expression =
      value->mutable_value_expression_re2()->mutable_value_expression();
  expression->add_chunk()->set_text("My name is ");
  expression->add_chunk()->set_key(
      static_cast<int>(autofill::ServerFieldType::NAME_LAST));
  expression->add_chunk()->set_text(", ");
  expression->add_chunk()->set_key(
      static_cast<int>(autofill::ServerFieldType::NAME_FIRST));
  expression->add_chunk()->set_text(" ");
  expression->add_chunk()->set_key(
      static_cast<int>(autofill::ServerFieldType::NAME_LAST));

  selector.add_filters()->mutable_enter_frame();
  selector.add_filters()->mutable_nth_match()->set_index(0);
  SelectorProto copy = selector;

  ClientStatus status = ResolveSelectorUserData(&selector, &user_data_);

  ASSERT_TRUE(status.ok());
  ASSERT_EQ(selector.filters(1).property().text_filter().re2(),
            "My name is Doe, Jo\\.h\\*n Doe");
  ASSERT_EQ(selector.filters().size(), copy.filters().size());

  // Other filters should remain unchanged
  ASSERT_EQ(selector.filters(0), copy.filters(0));
  ASSERT_EQ(selector.filters(2), copy.filters(2));
  ASSERT_EQ(selector.filters(3), copy.filters(3));
}

TEST_F(UserDataUtilTextValueTest, ResolveSelectorUserDataError) {
  autofill::AutofillProfile contact(base::GenerateGUID(),
                                    autofill::test::kEmptyOrigin);
  autofill::test::SetProfileInfo(&contact, "Jo.h*n", /* middle name */ "",
                                 "Doe", "", "", "", "", "", "", "", "", "");
  user_model_.SetSelectedAutofillProfile(
      "contact", std::make_unique<autofill::AutofillProfile>(contact),
      &user_data_);

  SelectorProto selector;
  selector.add_filters()->set_css_selector("#test");

  auto* filter = selector.add_filters();
  auto* value = filter->mutable_property()->mutable_autofill_value_regexp();
  value->mutable_profile()->set_identifier("contact");
  auto* expression =
      value->mutable_value_expression_re2()->mutable_value_expression();
  expression->add_chunk()->set_key(
      static_cast<int>(autofill::ServerFieldType::NAME_MIDDLE));

  ClientStatus status = ResolveSelectorUserData(&selector, &user_data_);

  ASSERT_FALSE(status.ok());
}

}  // namespace
}  // namespace user_data
}  // namespace autofill_assistant
