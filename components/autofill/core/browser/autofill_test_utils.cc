// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_test_utils.h"

#include <string>

#include "base/guid.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_store.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

using base::ASCIIToUTF16;

namespace autofill {
namespace test {

namespace {

const int kValidityStateBitfield = 1984;

std::string GetRandomCardNumber() {
  const size_t length = 16;
  std::string value;
  value.reserve(length);
  for (size_t i = 0; i < length; ++i)
    value.push_back(static_cast<char>(base::RandInt('0', '9')));
  return value;
}

}  // namespace

std::unique_ptr<PrefService> PrefServiceForTesting() {
  scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
      new user_prefs::PrefRegistrySyncable());
  return PrefServiceForTesting(registry.get());
}

std::unique_ptr<PrefService> PrefServiceForTesting(
    user_prefs::PrefRegistrySyncable* registry) {
  prefs::RegisterProfilePrefs(registry);

  PrefServiceFactory factory;
  factory.set_user_prefs(base::MakeRefCounted<TestingPrefStore>());
  return factory.Create(registry);
}

void CreateTestFormField(const char* label,
                         const char* name,
                         const char* value,
                         const char* type,
                         FormFieldData* field) {
  field->label = ASCIIToUTF16(label);
  field->name = ASCIIToUTF16(name);
  field->value = ASCIIToUTF16(value);
  field->form_control_type = type;
  field->is_focusable = true;
}

void CreateTestSelectField(const char* label,
                           const char* name,
                           const char* value,
                           const std::vector<const char*>& values,
                           const std::vector<const char*>& contents,
                           size_t select_size,
                           FormFieldData* field) {
  // Fill the base attributes.
  CreateTestFormField(label, name, value, "select-one", field);

  std::vector<base::string16> values16(select_size);
  for (size_t i = 0; i < select_size; ++i)
    values16[i] = base::UTF8ToUTF16(values[i]);

  std::vector<base::string16> contents16(select_size);
  for (size_t i = 0; i < select_size; ++i)
    contents16[i] = base::UTF8ToUTF16(contents[i]);

  field->option_values = values16;
  field->option_contents = contents16;
}

void CreateTestSelectField(const std::vector<const char*>& values,
                           FormFieldData* field) {
  CreateTestSelectField("", "", "", values, values, values.size(), field);
}

void CreateTestAddressFormData(FormData* form, const char* unique_id) {
  std::vector<ServerFieldTypeSet> types;
  CreateTestAddressFormData(form, &types, unique_id);
}

void CreateTestAddressFormData(FormData* form,
                               std::vector<ServerFieldTypeSet>* types,
                               const char* unique_id) {
  form->name =
      ASCIIToUTF16("MyForm") + ASCIIToUTF16(unique_id ? unique_id : "");
  form->button_titles = {
      std::make_pair(ASCIIToUTF16("Submit"),
                     mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form->url = GURL("http://myform.com/form.html");
  form->action = GURL("http://myform.com/submit.html");
  form->is_action_empty = true;
  form->main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  types->clear();
  form->submission_event =
      mojom::SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  ServerFieldTypeSet type_set;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(NAME_FIRST);
  types->push_back(type_set);
  test::CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(NAME_MIDDLE);
  types->push_back(type_set);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(NAME_LAST);
  types->push_back(type_set);
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(ADDRESS_HOME_LINE1);
  types->push_back(type_set);
  test::CreateTestFormField("Address Line 2", "addr2", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(ADDRESS_HOME_LINE2);
  types->push_back(type_set);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(ADDRESS_HOME_CITY);
  types->push_back(type_set);
  test::CreateTestFormField("State", "state", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(ADDRESS_HOME_STATE);
  types->push_back(type_set);
  test::CreateTestFormField("Postal Code", "zipcode", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(ADDRESS_HOME_ZIP);
  types->push_back(type_set);
  test::CreateTestFormField("Country", "country", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(ADDRESS_HOME_COUNTRY);
  types->push_back(type_set);
  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(PHONE_HOME_WHOLE_NUMBER);
  types->push_back(type_set);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(EMAIL_ADDRESS);
  types->push_back(type_set);
}

void CreateTestPersonalInformationFormData(FormData* form,
                                           const char* unique_id) {
  form->name =
      ASCIIToUTF16("MyForm") + ASCIIToUTF16(unique_id ? unique_id : "");
  form->url = GURL("http://myform.com/form.html");
  form->action = GURL("http://myform.com/submit.html");
  form->main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));

  FormFieldData field;
  ServerFieldTypeSet type_set;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form->fields.push_back(field);
}

void CreateTestCreditCardFormData(FormData* form,
                                  bool is_https,
                                  bool use_month_type,
                                  bool split_names,
                                  const char* unique_id) {
  form->name =
      ASCIIToUTF16("MyForm") + ASCIIToUTF16(unique_id ? unique_id : "");
  if (is_https) {
    form->url = GURL("https://myform.com/form.html");
    form->action = GURL("https://myform.com/submit.html");
    form->main_frame_origin =
        url::Origin::Create(GURL("https://myform_root.com/form.html"));
  } else {
    form->url = GURL("http://myform.com/form.html");
    form->action = GURL("http://myform.com/submit.html");
    form->main_frame_origin =
        url::Origin::Create(GURL("http://myform_root.com/form.html"));
  }

  FormFieldData field;
  if (split_names) {
    test::CreateTestFormField("First Name on Card", "firstnameoncard", "",
                              "text", &field);
    field.autocomplete_attribute = "cc-given-name";
    form->fields.push_back(field);
    test::CreateTestFormField("Last Name on Card", "lastnameoncard", "", "text",
                              &field);
    field.autocomplete_attribute = "cc-family-name";
    form->fields.push_back(field);
    field.autocomplete_attribute = "";
  } else {
    test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
    form->fields.push_back(field);
  }
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  form->fields.push_back(field);
  if (use_month_type) {
    test::CreateTestFormField("Expiration Date", "ccmonth", "", "month",
                              &field);
    form->fields.push_back(field);
  } else {
    test::CreateTestFormField("Expiration Date", "ccmonth", "", "text", &field);
    form->fields.push_back(field);
    test::CreateTestFormField("", "ccyear", "", "text", &field);
    form->fields.push_back(field);
  }
  test::CreateTestFormField("CVC", "cvc", "", "text", &field);
  form->fields.push_back(field);
}

inline void check_and_set(FormGroup* profile,
                          ServerFieldType type,
                          const char* value) {
  if (value)
    profile->SetRawInfo(type, base::UTF8ToUTF16(value));
}

AutofillProfile GetFullValidProfileForCanada() {
  AutofillProfile profile(base::GenerateGUID(), kEmptyOrigin);
  SetProfileInfo(&profile, "Alice", "", "Wonderland", "alice@wonderland.ca",
                 "Fiction", "666 Notre-Dame Ouest", "Apt 8", "Montreal", "QC",
                 "H3B 2T9", "CA", "15141112233");
  return profile;
}

AutofillProfile GetFullValidProfileForChina() {
  AutofillProfile profile(base::GenerateGUID(), kEmptyOrigin);
  SetProfileInfo(&profile, "John", "H.", "Doe", "johndoe@google.cn", "Google",
                 "100 Century Avenue", "", "赫章县", "毕节地区", "贵州省",
                 "200120", "CN", "+86-21-6133-7666");
  return profile;
}

AutofillProfile GetFullProfile() {
  AutofillProfile profile(base::GenerateGUID(), kEmptyOrigin);
  SetProfileInfo(&profile, "John", "H.", "Doe", "johndoe@hades.com",
                 "Underworld", "666 Erebus St.", "Apt 8", "Elysium", "CA",
                 "91111", "US", "16502111111");
  return profile;
}

AutofillProfile GetFullProfile2() {
  AutofillProfile profile(base::GenerateGUID(), kEmptyOrigin);
  SetProfileInfo(&profile, "Jane", "A.", "Smith", "jsmith@example.com", "ACME",
                 "123 Main Street", "Unit 1", "Greensdale", "MI", "48838", "US",
                 "13105557889");
  return profile;
}

AutofillProfile GetFullCanadianProfile() {
  AutofillProfile profile(base::GenerateGUID(), kEmptyOrigin);
  SetProfileInfo(&profile, "Wayne", "", "Gretzky", "wayne@hockey.com", "NHL",
                 "123 Hockey rd.", "Apt 8", "Moncton", "New Brunswick",
                 "E1A 0A6", "CA", "15068531212");
  return profile;
}

AutofillProfile GetIncompleteProfile1() {
  AutofillProfile profile(base::GenerateGUID(), kEmptyOrigin);
  SetProfileInfo(&profile, "John", "H.", "Doe", "jsmith@example.com", "ACME",
                 "123 Main Street", "Unit 1", "Greensdale", "MI", "48838", "US",
                 "");
  return profile;
}

AutofillProfile GetIncompleteProfile2() {
  AutofillProfile profile(base::GenerateGUID(), kEmptyOrigin);
  SetProfileInfo(&profile, "", "", "", "jsmith@example.com", "", "", "", "", "",
                 "", "", "");
  return profile;
}

AutofillProfile GetVerifiedProfile() {
  AutofillProfile profile(GetFullProfile());
  profile.set_origin(kSettingsOrigin);
  return profile;
}

AutofillProfile GetServerProfile() {
  AutofillProfile profile(AutofillProfile::SERVER_PROFILE, "id1");
  // Note: server profiles don't have email addresses and only have full names.
  SetProfileInfo(&profile, "", "", "", "", "Google, Inc.", "123 Fake St.",
                 "Apt. 42", "Mountain View", "California", "94043", "US",
                 "1.800.555.1234");

  profile.SetInfo(NAME_FULL, ASCIIToUTF16("John K. Doe"), "en");
  profile.SetRawInfo(ADDRESS_HOME_SORTING_CODE, ASCIIToUTF16("CEDEX"));
  profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY,
                     ASCIIToUTF16("Santa Clara"));

  profile.set_language_code("en");
  profile.SetClientValidityFromBitfieldValue(kValidityStateBitfield);
  profile.set_is_client_validity_states_updated(true);
  profile.set_use_count(7);
  profile.set_use_date(base::Time::FromTimeT(54321));

  profile.GenerateServerProfileIdentifier();

  return profile;
}

AutofillProfile GetServerProfile2() {
  AutofillProfile profile(AutofillProfile::SERVER_PROFILE, "id2");
  // Note: server profiles don't have email addresses.
  SetProfileInfo(&profile, "", "", "", "", "Main, Inc.", "4323 Wrong St.",
                 "Apt. 1032", "Sunnyvale", "California", "10011", "US",
                 "+1 514-123-1234");

  profile.SetInfo(NAME_FULL, ASCIIToUTF16("Jim S. Bristow"), "en");
  profile.SetRawInfo(ADDRESS_HOME_SORTING_CODE, ASCIIToUTF16("XEDEC"));
  profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY,
                     ASCIIToUTF16("Santa Monica"));

  profile.set_language_code("en");
  profile.SetClientValidityFromBitfieldValue(kValidityStateBitfield);
  profile.set_is_client_validity_states_updated(true);
  profile.set_use_count(14);
  profile.set_use_date(base::Time::FromTimeT(98765));

  profile.GenerateServerProfileIdentifier();

  return profile;
}

CreditCard GetCreditCard() {
  CreditCard credit_card(base::GenerateGUID(), kEmptyOrigin);
  SetCreditCardInfo(&credit_card, "Test User", "4111111111111111" /* Visa */,
                    "11", "2022", "1");
  return credit_card;
}

CreditCard GetCreditCard2() {
  CreditCard credit_card(base::GenerateGUID(), kEmptyOrigin);
  SetCreditCardInfo(&credit_card, "Someone Else", "378282246310005" /* AmEx */,
                    "07", "2022", "1");
  return credit_card;
}

CreditCard GetExpiredCreditCard() {
  CreditCard credit_card(base::GenerateGUID(), kEmptyOrigin);
  SetCreditCardInfo(&credit_card, "Test User", "4111111111111111" /* Visa */,
                    "11", "2002", "1");
  return credit_card;
}

CreditCard GetIncompleteCreditCard() {
  CreditCard credit_card(base::GenerateGUID(), kEmptyOrigin);
  SetCreditCardInfo(&credit_card, "", "4111111111111111" /* Visa */, "11",
                    "2022", "1");
  return credit_card;
}

CreditCard GetVerifiedCreditCard() {
  CreditCard credit_card(GetCreditCard());
  credit_card.set_origin(kSettingsOrigin);
  return credit_card;
}

CreditCard GetVerifiedCreditCard2() {
  CreditCard credit_card(GetCreditCard2());
  credit_card.set_origin(kSettingsOrigin);
  return credit_card;
}

CreditCard GetMaskedServerCard() {
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&credit_card, "Bonnie Parker",
                          "2109" /* Mastercard */, "12", "2020", "1");
  credit_card.SetNetworkForMaskedCard(kMasterCard);
  credit_card.set_card_type(CreditCard::CARD_TYPE_CREDIT);
  return credit_card;
}

CreditCard GetMaskedServerCardAmex() {
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "b456");
  test::SetCreditCardInfo(&credit_card, "Justin Thyme", "8431" /* Amex */, "9",
                          "2020", "1");
  credit_card.SetNetworkForMaskedCard(kAmericanExpressCard);
  credit_card.set_card_type(CreditCard::CARD_TYPE_PREPAID);
  return credit_card;
}

CreditCard GetFullServerCard() {
  CreditCard credit_card(CreditCard::FULL_SERVER_CARD, "c123");
  test::SetCreditCardInfo(&credit_card, "Full Carter",
                          "4111111111111111" /* Visa */, "12", "2020", "1");
  credit_card.set_card_type(CreditCard::CARD_TYPE_CREDIT);
  return credit_card;
}

CreditCard GetRandomCreditCard(CreditCard::RecordType record_type) {
  static const char* const kNetworks[] = {
      kAmericanExpressCard,
      kDinersCard,
      kDiscoverCard,
      kEloCard,
      kGenericCard,
      kJCBCard,
      kMasterCard,
      kMirCard,
      kUnionPay,
      kVisaCard,
  };
  constexpr size_t kNumNetworks = sizeof(kNetworks) / sizeof(kNetworks[0]);
  base::Time::Exploded now;
  AutofillClock::Now().LocalExplode(&now);

  CreditCard credit_card =
      (record_type == CreditCard::LOCAL_CARD)
          ? CreditCard(base::GenerateGUID(), kEmptyOrigin)
          : CreditCard(record_type, base::GenerateGUID().substr(24));
  test::SetCreditCardInfo(
      &credit_card, "Justin Thyme", GetRandomCardNumber().c_str(),
      base::StringPrintf("%d", base::RandInt(1, 12)).c_str(),
      base::StringPrintf("%d", now.year + base::RandInt(1, 4)).c_str(), "1");
  if (record_type == CreditCard::MASKED_SERVER_CARD) {
    credit_card.SetNetworkForMaskedCard(
        kNetworks[base::RandInt(0, kNumNetworks - 1)]);
  }

  return credit_card;
}

void SetProfileInfo(AutofillProfile* profile,
                    const char* first_name,
                    const char* middle_name,
                    const char* last_name,
                    const char* email,
                    const char* company,
                    const char* address1,
                    const char* address2,
                    const char* dependent_locality,
                    const char* city,
                    const char* state,
                    const char* zipcode,
                    const char* country,
                    const char* phone) {
  check_and_set(profile, NAME_FIRST, first_name);
  check_and_set(profile, NAME_MIDDLE, middle_name);
  check_and_set(profile, NAME_LAST, last_name);
  check_and_set(profile, EMAIL_ADDRESS, email);
  check_and_set(profile, COMPANY_NAME, company);
  check_and_set(profile, ADDRESS_HOME_LINE1, address1);
  check_and_set(profile, ADDRESS_HOME_LINE2, address2);
  check_and_set(profile, ADDRESS_HOME_DEPENDENT_LOCALITY, dependent_locality);
  check_and_set(profile, ADDRESS_HOME_CITY, city);
  check_and_set(profile, ADDRESS_HOME_STATE, state);
  check_and_set(profile, ADDRESS_HOME_ZIP, zipcode);
  check_and_set(profile, ADDRESS_HOME_COUNTRY, country);
  check_and_set(profile, PHONE_HOME_WHOLE_NUMBER, phone);
}

void SetProfileInfo(AutofillProfile* profile,
                    const char* first_name,
                    const char* middle_name,
                    const char* last_name,
                    const char* email,
                    const char* company,
                    const char* address1,
                    const char* address2,
                    const char* city,
                    const char* state,
                    const char* zipcode,
                    const char* country,
                    const char* phone) {
  check_and_set(profile, NAME_FIRST, first_name);
  check_and_set(profile, NAME_MIDDLE, middle_name);
  check_and_set(profile, NAME_LAST, last_name);
  check_and_set(profile, EMAIL_ADDRESS, email);
  check_and_set(profile, COMPANY_NAME, company);
  check_and_set(profile, ADDRESS_HOME_LINE1, address1);
  check_and_set(profile, ADDRESS_HOME_LINE2, address2);
  check_and_set(profile, ADDRESS_HOME_CITY, city);
  check_and_set(profile, ADDRESS_HOME_STATE, state);
  check_and_set(profile, ADDRESS_HOME_ZIP, zipcode);
  check_and_set(profile, ADDRESS_HOME_COUNTRY, country);
  check_and_set(profile, PHONE_HOME_WHOLE_NUMBER, phone);
}

void SetProfileInfoWithGuid(AutofillProfile* profile,
                            const char* guid,
                            const char* first_name,
                            const char* middle_name,
                            const char* last_name,
                            const char* email,
                            const char* company,
                            const char* address1,
                            const char* address2,
                            const char* city,
                            const char* state,
                            const char* zipcode,
                            const char* country,
                            const char* phone) {
  if (guid)
    profile->set_guid(guid);
  SetProfileInfo(profile, first_name, middle_name, last_name, email, company,
                 address1, address2, city, state, zipcode, country, phone);
}

void SetCreditCardInfo(CreditCard* credit_card,
                       const char* name_on_card,
                       const char* card_number,
                       const char* expiration_month,
                       const char* expiration_year,
                       const std::string& billing_address_id) {
  check_and_set(credit_card, CREDIT_CARD_NAME_FULL, name_on_card);
  check_and_set(credit_card, CREDIT_CARD_NUMBER, card_number);
  check_and_set(credit_card, CREDIT_CARD_EXP_MONTH, expiration_month);
  check_and_set(credit_card, CREDIT_CARD_EXP_4_DIGIT_YEAR, expiration_year);
  credit_card->set_billing_address_id(billing_address_id);
}

void DisableSystemServices(PrefService* prefs) {
  // Use a mock Keychain rather than the OS one to store credit card data.
  OSCryptMocker::SetUp();
}

void ReenableSystemServices() {
  OSCryptMocker::TearDown();
}

void SetServerCreditCards(AutofillTable* table,
                          const std::vector<CreditCard>& cards) {
  std::vector<CreditCard> as_masked_cards = cards;
  for (CreditCard& card : as_masked_cards) {
    card.set_record_type(CreditCard::MASKED_SERVER_CARD);
    card.SetNumber(card.LastFourDigits());
    card.SetNetworkForMaskedCard(card.network());
  }
  table->SetServerCreditCards(as_masked_cards);

  for (const CreditCard& card : cards) {
    if (card.record_type() != CreditCard::FULL_SERVER_CARD)
      continue;
    ASSERT_TRUE(table->UnmaskServerCreditCard(card, card.number()));
  }
}

void InitializePossibleTypesAndValidities(
    std::vector<ServerFieldTypeSet>& possible_field_types,
    std::vector<ServerFieldTypeValidityStatesMap>&
        possible_field_types_validities,
    const std::vector<ServerFieldType>& possible_types,
    const std::vector<AutofillDataModel::ValidityState>& validity_states) {
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types_validities.push_back(ServerFieldTypeValidityStatesMap());

  if (validity_states.empty()) {
    for (const auto& possible_type : possible_types) {
      possible_field_types.back().insert(possible_type);
      possible_field_types_validities.back()[possible_type].push_back(
          AutofillProfile::UNVALIDATED);
    }
    return;
  }

  ASSERT_FALSE(possible_types.empty());
  ASSERT_TRUE((possible_types.size() == validity_states.size()) ||
              (possible_types.size() == 1 && validity_states.size() > 1));

  ServerFieldType possible_type = possible_types[0];
  for (unsigned i = 0; i < validity_states.size(); ++i) {
    if (possible_types.size() == validity_states.size()) {
      possible_type = possible_types[i];
    }
    possible_field_types.back().insert(possible_type);
    possible_field_types_validities.back()[possible_type].push_back(
        validity_states[i]);
  }
}

void BasicFillUploadField(AutofillUploadContents::Field* field,
                          unsigned signature,
                          const char* name,
                          const char* control_type,
                          const char* autocomplete) {
  field->set_signature(signature);
  if (name)
    field->set_name(name);
  if (control_type)
    field->set_type(control_type);
  if (autocomplete)
    field->set_autocomplete(autocomplete);
}

void FillUploadField(AutofillUploadContents::Field* field,
                     unsigned signature,
                     const char* name,
                     const char* control_type,
                     const char* autocomplete,
                     unsigned autofill_type,
                     unsigned validity_state) {
  BasicFillUploadField(field, signature, name, control_type, autocomplete);

  field->add_autofill_type(autofill_type);

  auto* type_validities = field->add_autofill_type_validities();
  type_validities->set_type(autofill_type);
  type_validities->add_validity(validity_state);
}

void FillUploadField(AutofillUploadContents::Field* field,
                     unsigned signature,
                     const char* name,
                     const char* control_type,
                     const char* autocomplete,
                     const std::vector<unsigned>& autofill_types,
                     const std::vector<unsigned>& validity_states) {
  BasicFillUploadField(field, signature, name, control_type, autocomplete);

  for (unsigned i = 0; i < autofill_types.size(); ++i) {
    field->add_autofill_type(autofill_types[i]);

    auto* type_validities = field->add_autofill_type_validities();
    type_validities->set_type(autofill_types[i]);
    if (i < validity_states.size()) {
      type_validities->add_validity(validity_states[i]);
    } else {
      type_validities->add_validity(0);
    }
  }
}

void FillUploadField(AutofillUploadContents::Field* field,
                     unsigned signature,
                     const char* name,
                     const char* control_type,
                     const char* autocomplete,
                     unsigned autofill_type,
                     const std::vector<unsigned>& validity_states) {
  BasicFillUploadField(field, signature, name, control_type, autocomplete);

  field->add_autofill_type(autofill_type);
  auto* type_validities = field->add_autofill_type_validities();
  type_validities->set_type(autofill_type);
  for (unsigned i = 0; i < validity_states.size(); ++i)
    type_validities->add_validity(validity_states[i]);
}

void FillQueryField(AutofillQueryContents::Form::Field* field,
                    unsigned signature,
                    const char* name,
                    const char* control_type) {
  field->set_signature(signature);
  if (name)
    field->set_name(name);
  if (control_type)
    field->set_type(control_type);
}

void GenerateTestAutofillPopup(
    AutofillExternalDelegate* autofill_external_delegate) {
  int query_id = 1;
  FormData form;
  FormFieldData field;
  field.is_focusable = true;
  field.should_autocomplete = true;
  gfx::RectF bounds(100.f, 100.f);
  autofill_external_delegate->OnQuery(query_id, form, field, bounds);

  std::vector<Suggestion> suggestions;
  suggestions.push_back(Suggestion(base::ASCIIToUTF16("Test suggestion")));
  autofill_external_delegate->OnSuggestionsReturned(
      query_id, suggestions, /*autoselect_first_suggestion=*/false);
}

std::string ObfuscatedCardDigitsAsUTF8(const std::string& str) {
  return base::UTF16ToUTF8(
      internal::GetObfuscatedStringForCardDigits(base::ASCIIToUTF16(str)));
}

std::string NextMonth() {
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);
  return base::NumberToString(now.month % 12 + 1);
}
std::string LastYear() {
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);
  return base::NumberToString(now.year - 1);
}
std::string NextYear() {
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);
  return base::NumberToString(now.year + 1);
}
std::string TenYearsFromNow() {
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);
  return base::NumberToString(now.year + 10);
}

}  // namespace test
}  // namespace autofill
