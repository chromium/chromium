// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/determine_possible_field_types.h"

#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/types/zip.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_i18n_api.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/valuables_data_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::autofill::test::CreateTestFormField;
using ::autofill::test::CreateTestSelectField;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::Not;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

// Fakes that a `form` has been seen (without its field value) and parsed and
// then values have been entered. Returns the resulting FormStructure.
std::unique_ptr<FormStructure> ConstructFormStructureFromFormData(
    const FormData& form) {
  auto cached_form_structure =
      std::make_unique<FormStructure>(test::WithoutValues(form));
  cached_form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr);

  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->RetrieveFromCache(
      *cached_form_structure,
      FormStructure::RetrieveFromCacheReason::kFormImport);
  return form_structure;
}

void CheckThatOnlyFieldByIndexHasThisPossibleType(
    const FormStructure& form_structure,
    size_t field_index,
    FieldType type,
    FieldPropertiesMask mask) {
  EXPECT_TRUE(field_index < form_structure.field_count());

  for (size_t i = 0; i < form_structure.field_count(); i++) {
    if (i == field_index) {
      EXPECT_THAT(form_structure.field(i)->possible_types(), ElementsAre(type));
      EXPECT_EQ(mask, form_structure.field(i)->properties_mask());
    } else {
      EXPECT_THAT(form_structure.field(i)->possible_types(),
                  Not(Contains(type)));
    }
  }
}

void CheckThatNoFieldHasThisPossibleType(const FormStructure& form_structure,
                                         FieldType type) {
  for (size_t i = 0; i < form_structure.field_count(); i++) {
    EXPECT_THAT(form_structure.field(i)->possible_types(), Not(Contains(type)));
  }
}

struct TestAddressFillData {
  TestAddressFillData(const char* first,
                      const char* middle,
                      const char* last,
                      const char* address1,
                      const char* address2,
                      const char* city,
                      const char* state,
                      const char* postal_code,
                      const char* country,
                      const char* country_short,
                      const char* phone,
                      const char* email,
                      const char* company)
      : first(first),
        middle(middle),
        last(last),
        address1(address1),
        address2(address2),
        city(city),
        state(state),
        postal_code(postal_code),
        country(country),
        country_short(country_short),
        phone(phone),
        email(email),
        company(company) {}

  const char* first;
  const char* middle;
  const char* last;
  const char* address1;
  const char* address2;
  const char* city;
  const char* state;
  const char* postal_code;
  const char* country;
  const char* country_short;
  const char* phone;
  const char* email;
  const char* company;
};

TestAddressFillData GetElvisAddressFillData() {
  return {"Elvis",        "Aaron",   "Presley",    "3734 Elvis Presley Blvd.",
          "Apt. 10",      "Memphis", "Tennessee",  "38116",
          "South Africa", "ZA",      "2345678901", "theking@gmail.com",
          "RCA"};
}

AutofillProfile FillDataToAutofillProfile(const TestAddressFillData& data) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, data.first, data.middle, data.last, data.email,
                       data.company, data.address1, data.address2, data.city,
                       data.state, data.postal_code, data.country_short,
                       data.phone);
  return profile;
}

// Creates a GUID for testing. For example,
// MakeGuid(123) = "00000000-0000-0000-0000-000000000123";
std::string MakeGuid(size_t last_digit) {
  return base::StringPrintf("00000000-0000-0000-0000-%012zu", last_digit);
}

struct ProfileMatchingTypesTestCase {
  const char* input_value;   // The value to input in the field.
  FieldTypeSet field_types;  // The expected field types to be determined.
};

class ProfileMatchingTypesTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<ProfileMatchingTypesTestCase> {
 public:
  ProfileMatchingTypesTest() {
    features_.InitWithFeatures(
        {features::kAutofillUseFRAddressModel,
         features::kAutofillUseNLAddressModel,
         features::kAutofillUseNegativePatternForAllAttributes,
         features::kAutofillSupportLastNamePrefix},
        {});
  }

 protected:
  base::test::ScopedFeatureList features_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

const ProfileMatchingTypesTestCase kProfileMatchingTypesTestCases[] = {
    // Profile fields matches.
    {"Elvis", {NAME_FIRST}},
    {"Aaron", {NAME_MIDDLE}},
    {"A", {NAME_MIDDLE_INITIAL}},
    {"Presley", {NAME_LAST, NAME_LAST_SECOND, NAME_LAST_CORE}},
    {"Elvis Aaron Presley", {NAME_FULL}},
    {"theking@gmail.com", {EMAIL_ADDRESS}},
    {"RCA", {COMPANY_NAME}},
    {"3734 Elvis Presley Blvd.",
     {ADDRESS_HOME_LINE1, ADDRESS_HOME_STREET_LOCATION}},
    {"3734", {ADDRESS_HOME_HOUSE_NUMBER}},
    {"Elvis Presley Blvd.", {ADDRESS_HOME_STREET_NAME}},
    {"Apt. 10", {ADDRESS_HOME_LINE2, ADDRESS_HOME_SUBPREMISE}},
    {"Memphis", {ADDRESS_HOME_CITY}},
    {"Tennessee", {ADDRESS_HOME_STATE}},
    {"38116", {ADDRESS_HOME_ZIP}},
    {"ZA", {ADDRESS_HOME_COUNTRY}},
    {"South Africa", {ADDRESS_HOME_COUNTRY}},
    {"12345678901", {PHONE_HOME_WHOLE_NUMBER}},
    {"+1 (234) 567-8901", {PHONE_HOME_WHOLE_NUMBER}},
    {"(234)567-8901",
     {PHONE_HOME_CITY_AND_NUMBER,
      PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX}},
    {"2345678901",
     {PHONE_HOME_CITY_AND_NUMBER,
      PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX}},
    {"1", {PHONE_HOME_COUNTRY_CODE}},
    {"234", {PHONE_HOME_CITY_CODE, PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX}},
    {"5678901", {PHONE_HOME_NUMBER}},
    {"567", {PHONE_HOME_NUMBER_PREFIX}},
    {"8901", {PHONE_HOME_NUMBER_SUFFIX}},

    // Test a European profile.
    {"Paris", {ADDRESS_HOME_CITY}},
    {"Île de France", {ADDRESS_HOME_STATE}},    // Exact match
    {"Ile de France", {ADDRESS_HOME_STATE}},    // Missing accent.
    {"-Ile-de-France-", {ADDRESS_HOME_STATE}},  // Extra punctuation.
    {"île dÉ FrÃÑÇË", {ADDRESS_HOME_STATE}},  // Other accents & case mismatch.
    {"75008", {ADDRESS_HOME_ZIP}},
    {"FR", {ADDRESS_HOME_COUNTRY}},
    {"France", {ADDRESS_HOME_COUNTRY}},
    {"33249197070", {PHONE_HOME_WHOLE_NUMBER}},
    {"+33 2 49 19 70 70", {PHONE_HOME_WHOLE_NUMBER}},
    {"02 49 19 70 70", {PHONE_HOME_CITY_AND_NUMBER}},
    {"0249197070", {PHONE_HOME_CITY_AND_NUMBER}},
    {"33", {PHONE_HOME_COUNTRY_CODE}},
    {"2", {PHONE_HOME_CITY_CODE}},

    // Credit card fields matches.
    {"John Doe", {CREDIT_CARD_NAME_FULL}},
    {"John", {CREDIT_CARD_NAME_FIRST}},
    {"Doe", {CREDIT_CARD_NAME_LAST}},
    {"4234-5678-9012-3456", {CREDIT_CARD_NUMBER}},
    {"04", {CREDIT_CARD_EXP_MONTH}},
    {"April", {CREDIT_CARD_EXP_MONTH}},
    {"2999", {CREDIT_CARD_EXP_4_DIGIT_YEAR}},
    {"99", {CREDIT_CARD_EXP_2_DIGIT_YEAR}},
    {"04/2999", {CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR}},

    // Make sure whitespace and invalid characters are handled properly.
    {"", {EMPTY_TYPE}},
    {" ", {EMPTY_TYPE}},
    {"***", {UNKNOWN_TYPE}},
    {" Elvis", {NAME_FIRST}},
    {"Elvis ", {NAME_FIRST}},

    // Make sure fields that differ by case match.
    {"elvis ", {NAME_FIRST}},
    {"SoUTh AfRiCa", {ADDRESS_HOME_COUNTRY}},

    // Make sure fields that differ by punctuation match.
    {"3734 Elvis Presley Blvd",
     {ADDRESS_HOME_LINE1, ADDRESS_HOME_STREET_LOCATION}},
    {"3734, Elvis    Presley Blvd.",
     {ADDRESS_HOME_LINE1, ADDRESS_HOME_STREET_LOCATION}},

    // Make sure that a state's full name and abbreviation match.
    {"TN", {ADDRESS_HOME_STATE}},     // Saved as "Tennessee" in profile.
    {"Texas", {ADDRESS_HOME_STATE}},  // Saved as "TX" in profile.

    // Special phone number case. A profile with no country code should
    // only match PHONE_HOME_CITY_AND_NUMBER (And the trunk prefix equivalent).
    {"5142821292",
     {PHONE_HOME_CITY_AND_NUMBER,
      PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX}},

    // Make sure unsupported variants do not match.
    {"Elvis Aaron", {UNKNOWN_TYPE}},
    {"Mr. Presley", {UNKNOWN_TYPE}},
    {"3734 Elvis Presley", {UNKNOWN_TYPE}},
    {"38116-1023", {UNKNOWN_TYPE}},
    {"5", {UNKNOWN_TYPE}},
    {"56", {UNKNOWN_TYPE}},
    {"901", {UNKNOWN_TYPE}},

    // Make sure that last name prefix and last name core is handled correctly.
    {"Vincent Wilhelm van Gogh", {NAME_FULL}},
    {"Vincent", {NAME_FIRST}},
    {"Wilhelm", {NAME_MIDDLE}},
    {"van Gogh", {NAME_LAST}},
    {"van", {NAME_LAST_PREFIX}},
    {"Gogh", {NAME_LAST_CORE, NAME_LAST_SECOND}},
};

// Tests that DeterminePossibleFieldTypesForUpload finds accurate possible
// types.
TEST_P(ProfileMatchingTypesTest, DeterminePossibleFieldTypesForUpload) {
  // Unpack the test parameters
  const auto& test_case = GetParam();

  SCOPED_TRACE(base::StringPrintf(
      "Test: input_value='%s', field_type=%s, structured_names=%s ",
      test_case.input_value,
      FieldTypeToString(*test_case.field_types.begin()).c_str(), "true"));

  // Take the field types depending on the state of the structured names
  // feature.
  const FieldTypeSet& expected_possible_types = test_case.field_types;

  // Set up the test profiles.
  std::vector<AutofillProfile> profiles(
      4, AutofillProfile(i18n_model_definition::kLegacyHierarchyCountryCode));

  TestAddressFillData profile_info_data = GetElvisAddressFillData();
  profile_info_data.phone = "+1 (234) 567-8901";
  profiles[0] = FillDataToAutofillProfile(profile_info_data);

  profiles[0].set_guid(MakeGuid(1));

  test::SetProfileInfo(&profiles[1], "Charles", "", "Holley", "buddy@gmail.com",
                       "Decca", "123 Apple St.", "unit 6", "Lubbock", "TX",
                       "79401", "US", "5142821292");
  profiles[1].set_guid(MakeGuid(2));

  test::SetProfileInfo(&profiles[2], "Charles", "", "Baudelaire",
                       "lesfleursdumal@gmail.com", "", "108 Rue Saint-Lazare",
                       "Apt. 11", "Paris", "Île de France", "75008", "FR",
                       "+33 2 49 19 70 70");
  profiles[2].set_guid(MakeGuid(1));

  test::SetProfileInfo(&profiles[3], "Vincent", "Wilhelm", "van Gogh", "NL");
  profiles[3].set_guid(MakeGuid(4));

  // Set up the test credit cards.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", "4234-5678-9012-3456", "04",
                          "2999", "1");
  credit_card.set_guid(MakeGuid(3));
  credit_cards.push_back(credit_card);

  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  test_api(form).Append(CreateTestFormField("", "1", test_case.input_value,
                                            FormControlType::kInputText));

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);

  DeterminePossibleFieldTypesForUpload(
      profiles, credit_cards, std::vector<EntityInstance>(),
      std::vector<LoyaltyCard>(),
      /*fields_that_match_state=*/{},
      /*last_unlocked_credit_card_cvc=*/u"", "en-us", *form_structure);

  ASSERT_EQ(1U, form_structure->field_count());

  FieldTypeSet possible_types = form_structure->field(0)->possible_types();
  EXPECT_EQ(possible_types, expected_possible_types);
}

INSTANTIATE_TEST_SUITE_P(DeterminePossibleFieldTypesForUploadTest,
                         ProfileMatchingTypesTest,
                         testing::ValuesIn(kProfileMatchingTypesTestCases));

class DeterminePossibleFieldTypesForUploadTest : public ::testing::Test {
 public:
  DeterminePossibleFieldTypesForUploadTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kAutofillAiWithDataSchema,
         features::kAutofillEnableLoyaltyCardsFilling},
        {});
  }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// If a server-side credit card is unmasked by entering the CVC, the
// BrowserAutofillManager reuses the CVC value to identify a potentially
// existing CVC form field to cast a |CREDIT_CARD_VERIFICATION_CODE|-type vote.
TEST_F(DeterminePossibleFieldTypesForUploadTest, CrowdsourceCVCFieldByValue) {
  std::vector<AutofillProfile> profiles;
  std::vector<CreditCard> credit_cards;

  constexpr char kCvc[] = "1234";
  constexpr char16_t kCvc16[] = u"1234";
  constexpr char kFourDigitButNotCvc[] = "6676";
  constexpr char kCreditCardNumber[] = "4234-5678-9012-3456";

  FormData form;
  form.set_fields(
      {CreateTestFormField("number", "number", kCreditCardNumber,
                           FormControlType::kInputText),
       // This field would not be detected as CVC heuristically if the CVC value
       // wouldn't be known.
       CreateTestFormField("not_cvc", "not_cvc", kFourDigitButNotCvc,
                           FormControlType::kInputText),
       // This field has the CVC value used to unlock the card and should be
       // detected as the CVC field.
       CreateTestFormField("c_v_c", "c_v_c", kCvc,
                           FormControlType::kInputText)});

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  form_structure->field(0)->set_possible_types({CREDIT_CARD_NUMBER});

  DeterminePossibleFieldTypesForUpload(
      profiles, credit_cards, std::vector<EntityInstance>(),
      std::vector<LoyaltyCard>(),
      /*fields_that_match_state=*/{},
      /*last_unlocked_credit_card_cvc=*/kCvc16, "en-us", *form_structure);

  CheckThatOnlyFieldByIndexHasThisPossibleType(
      *form_structure, 2, CREDIT_CARD_VERIFICATION_CODE,
      FieldPropertiesFlags::kKnownValue);
}

// Expiration year field was detected by the server. The other field with a
// 4-digit value should be detected as CVC.
TEST_F(DeterminePossibleFieldTypesForUploadTest,
       CrowdsourceCVCFieldAfterInvalidExpDateByHeuristics) {
  constexpr char credit_card_number[] = "4234-5678-9012-3456";
  constexpr char actual_credit_card_exp_year[] = "2030";
  constexpr char user_entered_credit_card_exp_year[] = "2031";
  constexpr char cvc[] = "1234";

  FormData form;
  form.set_fields({CreateTestFormField("number", "number", credit_card_number,
                                       FormControlType::kInputText),
                   // Expiration date, but is not the expiration date of the
                   // used credit card.
                   CreateTestFormField("exp_year", "exp_year",
                                       user_entered_credit_card_exp_year,
                                       FormControlType::kInputText),
                   // Must be CVC since expiration date was already identified.
                   CreateTestFormField("cvc_number", "cvc_number", cvc,
                                       FormControlType::kInputText)});

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);

  // Set the field types.
  form_structure->field(0)->set_possible_types({CREDIT_CARD_NUMBER});
  form_structure->field(1)->set_possible_types({CREDIT_CARD_EXP_4_DIGIT_YEAR});
  form_structure->field(2)->set_possible_types({UNKNOWN_TYPE});

  // Set up the test credit cards.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", credit_card_number, "04",
                          actual_credit_card_exp_year, "1");
  credit_card.set_guid(MakeGuid(3));
  credit_cards.push_back(credit_card);

  // Set up the test profiles.
  std::vector<AutofillProfile> profiles;

  DeterminePossibleFieldTypesForUpload(
      profiles, credit_cards, std::vector<EntityInstance>(),
      std::vector<LoyaltyCard>(),
      /*fields_that_match_state=*/{},
      /*last_unlocked_credit_card_cvc=*/std::u16string(), "en-us",
      *form_structure);

  CheckThatOnlyFieldByIndexHasThisPossibleType(*form_structure, 2,
                                               CREDIT_CARD_VERIFICATION_CODE,
                                               FieldPropertiesFlags::kNoFlags);
}

// Tests if the CVC field is heuristically detected if it appears after the
// expiration year field as it was predicted by the server.
// The value in the CVC field would be a valid expiration year value.
TEST_F(DeterminePossibleFieldTypesForUploadTest,
       CrowdsourceCVCFieldAfterExpDateByHeuristics) {
  constexpr char credit_card_number[] = "4234-5678-9012-3456";
  constexpr char actual_credit_card_exp_year[] = "2030";
  constexpr char cvc[] = "1234";

  FormData form;
  form.set_fields(
      {CreateTestFormField("number", "number", credit_card_number,
                           FormControlType::kInputText),
       // Expiration date, that is the expiration date of the used credit card.
       CreateTestFormField("date_or_cvc1", "date_or_cvc1",
                           actual_credit_card_exp_year,
                           FormControlType::kInputText),
       // Must be CVC since expiration date was already identified.
       CreateTestFormField("date_or_cvc2", "date_or_cvc2", cvc,
                           FormControlType::kInputText)});

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);

  // Set the field types.
  form_structure->field(0)->set_possible_types({CREDIT_CARD_NUMBER});
  form_structure->field(1)->set_possible_types({CREDIT_CARD_EXP_4_DIGIT_YEAR});
  form_structure->field(2)->set_possible_types({UNKNOWN_TYPE});

  // Set up the test credit cards.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", credit_card_number, "04",
                          actual_credit_card_exp_year, "1");
  credit_card.set_guid(MakeGuid(3));
  credit_cards.push_back(credit_card);

  // Set up the test profiles.
  std::vector<AutofillProfile> profiles;

  DeterminePossibleFieldTypesForUpload(
      profiles, credit_cards, std::vector<EntityInstance>(),
      std::vector<LoyaltyCard>(),
      /*fields_that_match_state=*/{},
      /*last_unlocked_credit_card_cvc=*/std::u16string(), "en-us",
      *form_structure);

  CheckThatOnlyFieldByIndexHasThisPossibleType(*form_structure, 2,
                                               CREDIT_CARD_VERIFICATION_CODE,
                                               FieldPropertiesFlags::kNoFlags);
}

// Tests if the CVC field is heuristically detected if it contains a value which
// is not a valid expiration year.
TEST_F(DeterminePossibleFieldTypesForUploadTest,
       CrowdsourceCVCFieldBeforeExpDateByHeuristics) {
  constexpr char credit_card_number[] = "4234-5678-9012-3456";
  constexpr char actual_credit_card_exp_year[] = "2030";
  constexpr char user_entered_credit_card_exp_year[] = "2031";

  FormData form;
  form.set_fields({CreateTestFormField("number", "number", credit_card_number,
                                       FormControlType::kInputText),
                   // Must be CVC since it is an implausible expiration date.
                   CreateTestFormField("date_or_cvc2", "date_or_cvc2", "2130",
                                       FormControlType::kInputText),
                   // A field which is filled with a plausible expiration date
                   // which is not the date of the credit card.
                   CreateTestFormField("date_or_cvc1", "date_or_cvc1",
                                       user_entered_credit_card_exp_year,
                                       FormControlType::kInputText)});

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);

  // Set the field types.
  form_structure->field(0)->set_possible_types({CREDIT_CARD_NUMBER});
  form_structure->field(1)->set_possible_types({UNKNOWN_TYPE});
  form_structure->field(2)->set_possible_types({UNKNOWN_TYPE});

  // Set up the test credit cards.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", credit_card_number, "04",
                          actual_credit_card_exp_year, "1");
  credit_card.set_guid(MakeGuid(3));
  credit_cards.push_back(credit_card);

  // Set up the test profiles.
  std::vector<AutofillProfile> profiles;

  DeterminePossibleFieldTypesForUpload(
      profiles, credit_cards, std::vector<EntityInstance>(),
      std::vector<LoyaltyCard>(),
      /*fields_that_match_state=*/{},
      /*last_unlocked_credit_card_cvc=*/std::u16string(), "en-us",
      *form_structure);

  CheckThatOnlyFieldByIndexHasThisPossibleType(*form_structure, 1,
                                               CREDIT_CARD_VERIFICATION_CODE,
                                               FieldPropertiesFlags::kNoFlags);
}

// Tests if no CVC field is heuristically detected due to the missing of a
// credit card number field.
TEST_F(DeterminePossibleFieldTypesForUploadTest,
       CrowdsourceNoCVCFieldDueToMissingCreditCardNumber) {
  constexpr char credit_card_number[] = "4234-5678-9012-3456";
  constexpr char actual_credit_card_exp_year[] = "2030";
  constexpr char user_entered_credit_card_exp_year[] = "2031";
  constexpr char cvc[] = "2031";

  FormData form;
  form.set_fields({CreateTestFormField("number", "number", credit_card_number,
                                       FormControlType::kInputText),
                   // Server predicted as expiration year.
                   CreateTestFormField("date_or_cvc1", "date_or_cvc1",
                                       user_entered_credit_card_exp_year,
                                       FormControlType::kInputText),
                   // Must be CVC since expiration date was already identified.
                   CreateTestFormField("date_or_cvc2", "date_or_cvc2", cvc,
                                       FormControlType::kInputText)});

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);

  // Set the field types.
  form_structure->field(0)->set_possible_types({UNKNOWN_TYPE});
  form_structure->field(1)->set_possible_types({CREDIT_CARD_EXP_4_DIGIT_YEAR});
  form_structure->field(2)->set_possible_types({UNKNOWN_TYPE});

  // Set up the test credit cards.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", credit_card_number, "04",
                          actual_credit_card_exp_year, "1");
  credit_card.set_guid(MakeGuid(3));
  credit_cards.push_back(credit_card);

  // Set up the test profiles.
  std::vector<AutofillProfile> profiles;

  DeterminePossibleFieldTypesForUpload(
      profiles, credit_cards, std::vector<EntityInstance>(),
      std::vector<LoyaltyCard>(),
      /*fields_that_match_state=*/{},
      /*last_unlocked_credit_card_cvc=*/std::u16string(), "en-us",
      *form_structure);
  CheckThatNoFieldHasThisPossibleType(*form_structure,
                                      CREDIT_CARD_VERIFICATION_CODE);
}

// Test if no CVC is found because the candidate has no valid CVC value.
TEST_F(DeterminePossibleFieldTypesForUploadTest,
       CrowdsourceNoCVCDueToInvalidCandidateValue) {
  constexpr char credit_card_number[] = "4234-5678-9012-3456";
  constexpr char credit_card_exp_year[] = "2030";
  constexpr char cvc[] = "12";

  FormData form;
  form.set_fields(
      {CreateTestFormField("number", "number", credit_card_number,
                           FormControlType::kInputText),
       // Server predicted as expiration year.
       CreateTestFormField("date_or_cvc1", "date_or_cvc1", credit_card_exp_year,
                           FormControlType::kInputText),
       // Must be CVC since expiration date was already identified.
       CreateTestFormField("date_or_cvc2", "date_or_cvc2", cvc,
                           FormControlType::kInputText)});

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);

  // Set the field types.
  form_structure->field(0)->set_possible_types(
      {CREDIT_CARD_NUMBER, UNKNOWN_TYPE});
  form_structure->field(1)->set_possible_types({CREDIT_CARD_EXP_4_DIGIT_YEAR});
  form_structure->field(2)->set_possible_types({UNKNOWN_TYPE});

  // Set up the test credit cards.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", credit_card_number, "04",
                          credit_card_exp_year, "1");
  credit_card.set_guid(MakeGuid(3));
  credit_cards.push_back(credit_card);

  // Set up the test profiles.
  std::vector<AutofillProfile> profiles;

  DeterminePossibleFieldTypesForUpload(
      profiles, credit_cards, std::vector<EntityInstance>(),
      std::vector<LoyaltyCard>(),
      /*fields_that_match_state=*/{},
      /*last_unlocked_credit_card_cvc=*/u"", "en-us", *form_structure);

  CheckThatNoFieldHasThisPossibleType(*form_structure,
                                      CREDIT_CARD_VERIFICATION_CODE);
}

// Tests if the loyalty card field detected.
TEST_F(DeterminePossibleFieldTypesForUploadTest, CrowdsourceLoyaltyCardField) {
  constexpr char loyalty_card_program[] = "test_program";
  constexpr char loyalty_card_number[] = "4234567890123456";

  FormData form;
  form.set_fields(
      {// Loyalty card program field.
       CreateTestFormField("program", "program", loyalty_card_program,
                           FormControlType::kInputText),
       // Loyalty card number field.
       CreateTestFormField("loyalty_number", "loyalty_number",
                           loyalty_card_number, FormControlType::kInputText)});

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);

  // Set the field types.
  form_structure->field(0)->set_possible_types({LOYALTY_MEMBERSHIP_PROGRAM});

  // Set up the test loyalty cards.
  std::vector<LoyaltyCard> loyalty_cards;
  LoyaltyCard loyalty_card = test::CreateLoyaltyCard();
  loyalty_card.set_loyalty_card_number(loyalty_card_number);
  loyalty_cards.push_back(loyalty_card);
  DeterminePossibleFieldTypesForUpload(
      std::vector<AutofillProfile>(), std::vector<CreditCard>(),
      std::vector<EntityInstance>(), loyalty_cards,
      /*fields_that_match_state=*/{},
      /*last_unlocked_credit_card_cvc=*/u"", "en-us", *form_structure);

  CheckThatOnlyFieldByIndexHasThisPossibleType(*form_structure, 1,
                                               LOYALTY_MEMBERSHIP_ID,
                                               FieldPropertiesFlags::kNoFlags);
}

// Tests if the Autofill AI field types are crowdsourced.
TEST_F(DeterminePossibleFieldTypesForUploadTest, CrowdsourceAutofillAiTypes) {
  FormData form;
  form.set_fields({
      CreateTestFormField("first-name", "first-name", "Pippi",
                          FormControlType::kInputText),
      CreateTestFormField("last-name", "last-name", "Longstocking",
                          FormControlType::kInputText),
      CreateTestFormField("number", "number", "1234567",
                          FormControlType::kInputText),
      CreateTestFormField("expiry-date", "expiry-date", "30/08/2019",
                          FormControlType::kInputText),
      CreateTestFormField("wrong-country", "wrong-country", "Finland",
                          FormControlType::kInputText),
  });
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);

  EntityInstance entity = test::GetPassportEntityInstance({
      .name = u"Pippi Longstocking",
      .number = u"1234567",
      .country = u"Sweden",
      .expiry_date = u"2019-08-30",
      .issue_date = u"2010-09-01",
  });

  DeterminePossibleFieldTypesForUpload(
      std::vector<AutofillProfile>(), std::vector<CreditCard>(),
      base::span_from_ref(entity), std::vector<LoyaltyCard>(),
      /*fields_that_match_state=*/{},
      /*last_unlocked_credit_card_cvc=*/u"", "en-US", *form_structure);

  EXPECT_THAT(form_structure->fields()[0]->possible_types(),
              UnorderedElementsAre(PASSPORT_NAME_TAG, NAME_FIRST));
  EXPECT_THAT(
      form_structure->fields()[1]->possible_types(),
      UnorderedElementsAre(PASSPORT_NAME_TAG, NAME_LAST, NAME_LAST_SECOND));
  EXPECT_THAT(form_structure->fields()[2]->possible_types(),
              UnorderedElementsAre(PASSPORT_NUMBER));
  EXPECT_THAT(form_structure->fields()[3]->possible_types(),
              UnorderedElementsAre(PASSPORT_EXPIRATION_DATE));
  EXPECT_THAT(form_structure->fields()[4]->possible_types(),
              UnorderedElementsAre(UNKNOWN_TYPE));
}

// Test fixture for PreProcessStateMatchingTypes().
class PreProcessStateMatchingTypesTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    test::ClearAlternativeStateNameMapForTesting();
    test::PopulateAlternativeStateNameMapForTesting();
    test::SetProfileInfo(&profile_, "", "", "", "", "", "", "", "", "Bavaria",
                         "", "DE", "");
  }

  void TearDown() override { testing::Test::TearDown(); }

  TestAutofillClient& client() { return client_; }
  AutofillProfile& profile() { return profile_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient client_;
  AutofillProfile profile_{i18n_model_definition::kLegacyHierarchyCountryCode};
};

// Tests that we properly match typed values to stored state data.
TEST_F(PreProcessStateMatchingTypesTest, PreProcessStateMatchingTypes) {
  const char* const kValidMatches[] = {"by", "Bavaria", "Bayern",
                                       "BY", "B.Y",     "B-Y"};
  std::vector<const AutofillProfile*> profiles = {&profile()};

  for (const char* valid_match : kValidMatches) {
    SCOPED_TRACE(valid_match);
    FormData form;
    form.set_fields({CreateTestFormField("Name", "Name", /*value=*/"",
                                         FormControlType::kInputText),
                     CreateTestFormField("State", "state", /*value=*/"",
                                         FormControlType::kInputText)});

    FormStructure form_structure(form);
    ASSERT_EQ(form_structure.field_count(), 2U);
    form_structure.fields()[0]->set_value(u"Test");
    form_structure.fields()[1]->set_value(base::UTF8ToUTF16(valid_match));
    ASSERT_EQ(form_structure.fields()[0]->initial_value(), u"");
    ASSERT_EQ(form_structure.fields()[1]->initial_value(), u"");

    EXPECT_THAT(PreProcessStateMatchingTypes(profiles, form_structure,
                                             client().GetAppLocale()),
                ElementsAre(form_structure.field(1)->global_id()));
  }

  const char* const kInvalidMatches[] = {"Garbage", "BYA",   "BYA is a state",
                                         "Bava",    "Empty", ""};
  for (const char* invalid_match : kInvalidMatches) {
    SCOPED_TRACE(invalid_match);
    FormData form;
    form.set_fields({CreateTestFormField("Name", "Name", "Test",
                                         FormControlType::kInputText),
                     CreateTestFormField("State", "state", invalid_match,
                                         FormControlType::kInputText)});

    FormStructure form_structure(form);
    EXPECT_EQ(form_structure.field_count(), 2U);

    EXPECT_THAT(PreProcessStateMatchingTypes(profiles, form_structure,
                                             client().GetAppLocale()),
                IsEmpty());
  }

  test::PopulateAlternativeStateNameMapForTesting(
      "US", "California",
      {{.canonical_name = "California",
        .abbreviations = {"CA"},
        .alternative_names = {}}});

  test::SetProfileInfo(&profile(), "", "", "", "", "", "", "", "", "California",
                       "", "US", "");

  FormData form;
  form.set_fields({CreateTestFormField("Name", "Name", /*value=*/"",
                                       FormControlType::kInputText),
                   CreateTestFormField("State", "state", /*value=*/"",
                                       FormControlType::kInputText)});

  FormStructure form_structure(form);
  ASSERT_EQ(form_structure.field_count(), 2U);
  form_structure.fields()[0]->set_value(u"Test");
  form_structure.fields()[1]->set_value(u"CA");
  ASSERT_EQ(form_structure.fields()[0]->initial_value(), u"");
  ASSERT_EQ(form_structure.fields()[1]->initial_value(), u"");

  EXPECT_THAT(PreProcessStateMatchingTypes(profiles, form_structure,
                                           client().GetAppLocale()),
              ElementsAre(form_structure.field(1)->global_id()));
}

// Test fixture for DeterminePossibleFormatStringsForUpload().
class DeterminePossibleFormatStringsForUploadTest : public testing::Test {
 public:
  DeterminePossibleFormatStringsForUploadTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kAutofillAiVoteForFormatStringsFromSingleFields,
         features::kAutofillAiVoteForFormatStringsFromMultipleFields},
        {});
  }

  static std::unique_ptr<AutofillField> CreateInput(
      std::string_view value,
      FormControlType form_control_type = FormControlType::kInputText) {
    auto field = std::make_unique<AutofillField>(CreateTestFormField(
        /*label=*/"", /*name=*/"", /*value=*/value, form_control_type));
    field->set_is_user_edited(true);
    return field;
  }

  static std::unique_ptr<AutofillField> CreateSelect(
      std::string_view value,
      const std::vector<const char*>& values) {
    auto field = std::make_unique<AutofillField>(CreateTestSelectField(
        /*label=*/"", /*name=*/"", /*value=*/value, /*values=*/values,
        /*contents=*/values));
    field->set_is_user_edited(true);
    return field;
  }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that non-text <input> do not match any format string.
TEST_F(DeterminePossibleFormatStringsForUploadTest, InputNonText) {
  using enum FormControlType;
  EXPECT_THAT(DeterminePossibleFormatStringsForUpload({
                  CreateInput("2025-12-31", kInputDate),
                  CreateInput("2025-12", kInputMonth),
                  CreateInput("2025-12-31", kInputNumber),
                  CreateInput("2025-12-31", kInputPassword),
                  CreateInput("2025-12-31", kInputSearch),
                  CreateInput("2025-12-31", kInputUrl),
              }),
              IsEmpty());
}

struct DateSingleTextParam {
  std::string ToString() const {
    return base::StrCat({"value: ", value, "\n",  //
                         "formats: [", base::JoinString(format_strings, ", "),
                         "]"});
  }

  std::string_view value;
  std::vector<std::string_view> format_strings;
};

// Test fixture for a single <input type=text> whose value may be a complete
// date.
class DeterminePossibleFormatStringsForUploadTest_SingleTextInput
    : public DeterminePossibleFormatStringsForUploadTest,
      public testing::WithParamInterface<DateSingleTextParam> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    DeterminePossibleFormatStringsForUploadTest_SingleTextInput,
    testing::ValuesIn(std::vector<DateSingleTextParam>{
        {"2025-12-31", {"YYYY-MM-DD", "YYYY-M-D"}},
        {"31/12/2025", {"DD/MM/YYYY", "D/M/YYYY"}},
        {"31.12.2025", {"DD.MM.YYYY", "D.M.YYYY"}},
        {"31-01-2025", {"DD-MM-YYYY"}},
        {"31-31-2025", {}},
        {"31012025", {"DDMMYYYY"}},
        {"12/12/12",
         {"DD/MM/YY", "MM/DD/YY", "YY/MM/DD", "D/M/YY", "M/D/YY", "YY/M/D"}},
        {"31/12/12", {"DD/MM/YY", "YY/MM/DD", "D/M/YY", "YY/M/D"}},
        {"12/13/12", {"MM/DD/YY", "M/D/YY"}},
        {"12/12/32", {"DD/MM/YY", "MM/DD/YY", "D/M/YY", "M/D/YY"}},
        {"13/13/12", {}},
        {"foobar", {}},
    }));

// Tests that the values of <input type=text> match certain format strings.
TEST_P(DeterminePossibleFormatStringsForUploadTest_SingleTextInput,
       SingleTextInput) {
  SCOPED_TRACE(testing::Message() << "Values are:\n" << GetParam().ToString());
  std::unique_ptr<AutofillField> field = CreateInput(GetParam().value);
  if (GetParam().format_strings.empty()) {
    EXPECT_THAT(
        DeterminePossibleFormatStringsForUpload(base::span_from_ref(field)),
        IsEmpty());
  } else {
    EXPECT_THAT(
        DeterminePossibleFormatStringsForUpload(base::span_from_ref(field)),
        ElementsAre(Pair(field->global_id(),
                         UnorderedElementsAreArray(base::ToVector(
                             GetParam().format_strings, [](std::string_view s) {
                               return base::UTF8ToUTF16(s);
                             })))));
  }
}

struct DateMultipleTextParam {
  struct Field {
    std::string ToString() const {
      return base::StrCat({"- label: ", label, "\n",  //
                           "  value: ", value, "\n",  //
                           "  format strings: [",
                           base::JoinString(format_strings, ", "), "]"});
    }

    std::string_view label;
    std::string_view value;
    std::vector<std::string_view> format_strings;
  };

  std::string ToString() const {
    return base::JoinString(base::ToVector(fields, &Field::ToString), "\n");
  }

  std::vector<Field> fields;
};

// Test fixture for a sequences of <input type=text> whose combined values may
// be a complete date.
class DeterminePossibleFormatStringsForUploadTest_MultipleTextInput
    : public DeterminePossibleFormatStringsForUploadTest,
      public testing::WithParamInterface<DateMultipleTextParam> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    DeterminePossibleFormatStringsForUploadTest_MultipleTextInput,
    testing::ValuesIn(std::vector<DateMultipleTextParam>{
        {{{"Date", "2025", {"YYYY"}},
          {"Date", "12", {"MM", "M"}},
          {"Date", "31", {"DD", "D"}}}},
        {{{"Date", "31", {"DD", "D"}},
          {"/", "12", {"MM", "M"}},
          {"/", "2025", {"YYYY"}}}},
        {{{"Date", "31", {"DD", "D"}},
          {".", "12", {"MM", "M"}},
          {".", "2025", {"YYYY"}}}},
        {{{"Date", "31", {"DD", "D"}},
          {".", "12", {"MM", "M"}},
          {".", "2025", {"YYYY"}}}},
        {{{"Date", "31", {"DD"}},
          {"Date", "01", {"MM"}},
          {"Date", "2025", {"YYYY"}}}},
        {{{"Date", "31", {}}, {"Date", "31", {}}, {"Date", "2025", {}}}},
        {{{"Date", "12", {"DD", "D", "MM", "M", "YY"}},
          {"Date", "12", {"DD", "D", "MM", "M"}},
          {"Date", "12", {"DD", "D", "YY"}}}},
        {{{"Date", "31", {"DD", "D", "YY"}},
          {"Date", "12", {"MM", "M"}},
          {"Date", "12", {"DD", "D", "YY"}}}},
        {{{"Date", "12", {"MM", "M"}},
          {"Date", "13", {"DD", "D"}},
          {"Date", "12", {"YY"}}}},
        {{{"Date", "12", {"DD", "D", "MM", "M"}},
          {"Date", "12", {"DD", "D", "MM", "M"}},
          {"Date", "32", {"YY"}}}},
        {{{"Date", "13", {}}, {"Date", "13", {}}, {"Date", "12", {}}}},
        {{{"Date", "1", {}},
          {"Date", "2025", {"YYYY"}},
          {"Date", "12", {"MM", "M"}},
          {"Date", "31", {"DD", "D"}}}},
        {{{"Date", "12", {"DD", "D", "MM", "M", "YY"}},
          {"Date", "12", {"DD", "D", "MM", "M", "YY"}},
          {"Date", "12", {"DD", "D", "MM", "M", "YY"}},
          {"Date", "12", {"DD", "D", "YY"}}}},
        {{{"Date", "31", {}},
          {"Something else", "12", {}},
          {"Something completely different", "2025", {}}}},
        {{{"Date", "1", {}},
          {"Date", "1", {}},
          {"Date", "1", {}},
          {"Date", "1", {}}}},
        {{{"Date", "123", {}},
          {"Date", "123", {}},
          {"Date", "123", {}},
          {"Date", "123", {}}}},
        {{{"Date", "ash", {}},
          {"Date", "sho", {}},
          {"Date", "hte", {}},
          {"Date", "tne", {}},
          {"Date", "neo", {}}}}}));

// Tests that the combined values of sequences of <input type=text> match
// certain format strings. For example,
//   <input type=text value=31>
//   <input type=text value=12>
//   <input type=text value=2025>
// represents the date 31/12/2025 and the resulting format strings for the
// three fields should be DD od D, MM or M, and YYYY, respectively.
TEST_P(DeterminePossibleFormatStringsForUploadTest_MultipleTextInput,
       MultipleTextInput) {
  SCOPED_TRACE(testing::Message() << "Fields are:\n" << GetParam().ToString());
  std::vector<std::unique_ptr<AutofillField>> fields = base::ToVector(
      GetParam().fields, [](const DateMultipleTextParam::Field& field) {
        std::unique_ptr<AutofillField> f = CreateInput(field.value);
        f->set_label(base::UTF8ToUTF16(field.label));
        return f;
      });

  std::vector<Matcher<std::pair<FieldGlobalId, base::flat_set<std::u16string>>>>
      expectations;
  for (auto [form_field, autofill_field] :
       base::zip(GetParam().fields, fields)) {
    std::vector<std::u16string> format_strings = base::ToVector(
        form_field.format_strings, [](std::string_view format_string) {
          return base::UTF8ToUTF16(format_string);
        });
    if (!format_strings.empty()) {
      expectations.emplace_back(
          Pair(autofill_field->global_id(),
               UnorderedElementsAreArray(std::move(format_strings))));
    }
  }

  EXPECT_THAT(DeterminePossibleFormatStringsForUpload(fields),
              UnorderedElementsAreArray(expectations));
}

// Test fixture for DetermineAvailableFieldTypes().
class DetermineAvailableFieldTypesTest : public ::testing::Test {
 public:
  DetermineAvailableFieldTypesTest() {
    features_.InitWithFeatures(
        {features::kAutofillAiWithDataSchema,
         features::kAutofillEnableLoyaltyCardsFilling,
         features::kAutofillEnableEmailOrLoyaltyCardsFilling},
        {});
  }

 protected:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList features_;
};

// Tests that entities are included in the set of available field types.
TEST_F(DetermineAvailableFieldTypesTest, Entities) {
  EntityInstance entity = test::GetPassportEntityInstance();
  FieldTypeSet available_types = DetermineAvailableFieldTypes(
      /*profiles=*/{}, /*credit_cards=*/{}, /*entities=*/{entity},
      /*loyalty_cards=*/{},
      /*last_unlocked_credit_card_cvc=*/u"",
      /*app_locale=*/"en-US");
  EXPECT_THAT(available_types,
              UnorderedElementsAre(
                  NAME_FULL, NAME_FIRST, NAME_LAST, NAME_LAST_SECOND,
                  PASSPORT_NAME_TAG, PASSPORT_NUMBER, PASSPORT_EXPIRATION_DATE,
                  PASSPORT_ISSUE_DATE, PASSPORT_ISSUING_COUNTRY));
}

// Tests that loyalty cards are included in the set of available field types.
TEST_F(DetermineAvailableFieldTypesTest, LoyaltyCards) {
  LoyaltyCard card = test::CreateLoyaltyCard();
  FieldTypeSet available_types = DetermineAvailableFieldTypes(
      /*profiles=*/{}, /*credit_cards=*/{}, /*entities=*/{}, {card},
      /*last_unlocked_credit_card_cvc=*/u"",
      /*app_locale=*/"");
  EXPECT_TRUE(available_types.contains(LOYALTY_MEMBERSHIP_ID));
}

}  // namespace
}  // namespace autofill
