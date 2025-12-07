// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/determine_possible_field_types.h"

#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/types/zip.h"
#include "components/autofill/core/browser/autofill_field_test_api.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_i18n_api.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/determine_regex_types.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_test_utils.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/valuables_data_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

void PrintTo(const PossibleTypes& ps, std::ostream* os) {
  auto type_to_string = [](FormatString_Type t) {
    switch (t) {
      case FormatString_Type_AFFIX:
        return u"AFFIX";
      case FormatString_Type_DATE:
        return u"DATE";
      case FormatString_Type_FLIGHT_NUMBER:
        return u"FLIGHT_NUMBER";
      case FormatString_Type_ICU_DATE:
        return u"ICU_DATE";
    }
    NOTREACHED();
  };
  auto format_to_string =
      [&](const std::pair<FormatString_Type, std::u16string>& format) {
        return base::StrCat({u"{", type_to_string(format.first), u"\"",
                             format.second, u"\"", u"}"});
      };
  *os << "PossibleTypes{.types = {"
      << base::JoinString(base::ToVector(ps.types,
                                         [](FieldType t) {
                                           return FieldTypeToStringView(t);
                                         }),
                          ",")
      << "}, .formats = {"
      << base::JoinString(base::ToVector(ps.formats, format_to_string), u",")
      << "}}";
}

namespace {

using ::autofill::test::CreateTestFormField;
using ::autofill::test::CreateTestSelectField;
using ::one_time_tokens::OneTimeToken;
using ::one_time_tokens::OneTimeTokenType;
using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::Not;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

// Matcher for `PossibleTypes::types`.
template <typename... Ts>
  requires(std::convertible_to<Ts, FieldType> && ...)
Matcher<const PossibleTypes&> HasTypes(Ts&&... field_types) {
  return Field("PossibleTypes::types", &PossibleTypes::types,
               UnorderedElementsAre(field_types...));
}

// Matcher for `PossibleTypes::formats`.
template <typename... Ts>
  requires(std::convertible_to<Ts, const char*> && ...)
Matcher<const PossibleTypes&> HasNoFormats() {
  return Field("PossibleTypes::formats", &PossibleTypes::formats, IsEmpty());
}

// Matcher for `PossibleTypes::formats`.
template <typename... Ts>
  requires(std::convertible_to<Ts, const char*> && ...)
Matcher<const PossibleTypes&> HasFormats(FormatString_Type type,
                                         Ts&&... formats) {
  return Field("PossibleTypes::formats", &PossibleTypes::formats,
               UnorderedElementsAre(Pair(
                   type, base::UTF8ToUTF16(std::string_view(formats)))...));
}

template <typename... Ts>
Matcher<const PossibleTypes&> HasAffixFormats(Ts&&... formats) {
  return HasFormats(FormatString_Type_AFFIX, formats...);
}

template <typename... Ts>
Matcher<const PossibleTypes&> HasDateFormats(Ts&&... formats) {
  return HasFormats(FormatString_Type_DATE, formats...);
}

template <typename... Ts>
Matcher<const PossibleTypes&> HasFlightNumberFormats(Ts&&... formats) {
  return HasFormats(FormatString_Type_FLIGHT_NUMBER, formats...);
}

// Fakes that a `form` has been seen (without its field value) and parsed and
// then values have been entered. Returns the resulting FormStructure.
std::unique_ptr<FormStructure> ConstructFormStructureFromFormData(
    const FormData& form) {
  auto form_structure = std::make_unique<FormStructure>(form);
  const RegexPredictions regex_predictions =
      DetermineRegexTypes(GeoIpCountryCode(""), LanguageCode(""),
                          form_structure->ToFormData(), nullptr);
  regex_predictions.ApplyTo(form_structure->fields());
  form_structure->RationalizeAndAssignSections(GeoIpCountryCode(""),
                                               LanguageCode(""), nullptr);

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    test_api(*form_structure->field(i)).set_initial_value(u"");
  }
  return form_structure;
}

void CheckThatOnlyFieldByIndexHasThisPossibleType(
    base::span<const PossibleTypes> possible_types,
    size_t field_index,
    FieldType type) {
  EXPECT_LT(field_index, possible_types.size());
  for (size_t i = 0; i < possible_types.size(); i++) {
    if (i == field_index) {
      EXPECT_THAT(possible_types[i].types, ElementsAre(type)) << "i=" << i;
    } else {
      EXPECT_THAT(possible_types[i].types, Not(Contains(type))) << "i=" << i;
    }
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
        {features::kAutofillUseNegativePatternForAllAttributes,
         features::kAutofillSupportLastNamePrefix,
         features::kAutofillSupportSplitZipCode},
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
    {"+12345678901", {PHONE_HOME_WHOLE_NUMBER}},
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

    // Make sure that zip prefix and suffix are handled correctly.
    {"79401-4321", {ADDRESS_HOME_ZIP}},
    {"79401", {ADDRESS_HOME_ZIP}},
    {"4321", {ADDRESS_HOME_ZIP_SUFFIX}},
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
                       "79401-4321", "US", "5142821292");
  profiles[1].set_guid(MakeGuid(2));

  test::SetProfileInfo(&profiles[2], "Charles", "", "Baudelaire",
                       "lesfleursdumal@gmail.com", "", "108 Rue Saint-Lazare",
                       "Apt. 11", "Paris", "Île de France", "75008", "FR",
                       "+33 2 49 19 70 70");
  profiles[2].set_guid(MakeGuid(1));

  test::SetProfileInfo(&profiles[3], "Vincent", "Wilhelm", "van Gogh", "NL");
  profiles[3].set_guid(MakeGuid(4));

  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", "4234-5678-9012-3456", "04",
                          "2999", "1");
  credit_card.set_guid(MakeGuid(3));

  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  test_api(form).Append(CreateTestFormField("", "1", test_case.input_value,
                                            FormControlType::kInputText));

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);

  std::vector<PossibleTypes> possible_types =
      DeterminePossibleFieldTypesForUpload(
          profiles, {credit_card}, std::vector<EntityInstance>(),
          std::vector<LoyaltyCard>(),
          /*fields_that_match_state=*/{},
          /*last_unlocked_credit_card_cvc=*/u"", std::vector<OneTimeToken>(),
          "en-us", form_structure->fields());

  ASSERT_EQ(form_structure->field_count(), possible_types.size());
  EXPECT_THAT(possible_types[0].types,
              UnorderedElementsAreArray(expected_possible_types));
}

INSTANTIATE_TEST_SUITE_P(DeterminePossibleFieldTypesForUploadTest,
                         ProfileMatchingTypesTest,
                         testing::ValuesIn(kProfileMatchingTypesTestCases));

class DeterminePossibleFieldTypesForUploadTest : public ::testing::Test {
 public:
  DeterminePossibleFieldTypesForUploadTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kAutofillAiWithDataSchema,
         features::kAutofillAiVoteForFormatStringsForAffixes,
         features::kAutofillAiVoteForFormatStringsForFlightNumbers,
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

  std::vector<PossibleTypes> possible_types =
      DeterminePossibleFieldTypesForUpload(
          std::vector<AutofillProfile>(), std::vector<CreditCard>(),
          std::vector<EntityInstance>(), std::vector<LoyaltyCard>(),
          /*fields_that_match_state=*/{},
          /*last_unlocked_credit_card_cvc=*/kCvc16, std::vector<OneTimeToken>(),
          "en-us", form_structure->fields());

  CheckThatOnlyFieldByIndexHasThisPossibleType(possible_types, 2,
                                               CREDIT_CARD_VERIFICATION_CODE);
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

  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", credit_card_number, "04",
                          actual_credit_card_exp_year, "1");
  credit_card.set_guid(MakeGuid(3));

  std::vector<PossibleTypes> possible_types =
      DeterminePossibleFieldTypesForUpload(
          std::vector<AutofillProfile>(), {credit_card},
          std::vector<EntityInstance>(), std::vector<LoyaltyCard>(),
          /*fields_that_match_state=*/{},
          /*last_unlocked_credit_card_cvc=*/std::u16string(),
          std::vector<OneTimeToken>(), "en-us", form_structure->fields());

  CheckThatOnlyFieldByIndexHasThisPossibleType(possible_types, 2,
                                               CREDIT_CARD_VERIFICATION_CODE);
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

  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", credit_card_number, "04",
                          actual_credit_card_exp_year, "1");
  credit_card.set_guid(MakeGuid(3));

  std::vector<PossibleTypes> possible_types =
      DeterminePossibleFieldTypesForUpload(
          std::vector<AutofillProfile>(), {credit_card},
          std::vector<EntityInstance>(), std::vector<LoyaltyCard>(),
          /*fields_that_match_state=*/{},
          /*last_unlocked_credit_card_cvc=*/std::u16string(),
          std::vector<OneTimeToken>(), "en-us", form_structure->fields());

  CheckThatOnlyFieldByIndexHasThisPossibleType(possible_types, 2,
                                               CREDIT_CARD_VERIFICATION_CODE);
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

  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", credit_card_number, "04",
                          actual_credit_card_exp_year, "1");
  credit_card.set_guid(MakeGuid(3));

  std::vector<PossibleTypes> possible_types =
      DeterminePossibleFieldTypesForUpload(
          std::vector<AutofillProfile>(), {credit_card},
          std::vector<EntityInstance>(), std::vector<LoyaltyCard>(),
          /*fields_that_match_state=*/{},
          /*last_unlocked_credit_card_cvc=*/std::u16string(),
          std::vector<OneTimeToken>(), "en-us", form_structure->fields());

  CheckThatOnlyFieldByIndexHasThisPossibleType(possible_types, 1,
                                               CREDIT_CARD_VERIFICATION_CODE);
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

  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", credit_card_number, "04",
                          actual_credit_card_exp_year, "1");
  credit_card.set_guid(MakeGuid(3));

  std::vector<PossibleTypes> possible_types =
      DeterminePossibleFieldTypesForUpload(
          std::vector<AutofillProfile>(), {credit_card},
          std::vector<EntityInstance>(), std::vector<LoyaltyCard>(),
          /*fields_that_match_state=*/{},
          /*last_unlocked_credit_card_cvc=*/std::u16string(),
          std::vector<OneTimeToken>(), "en-us", form_structure->fields());
  EXPECT_THAT(possible_types,
              Each(Field(&PossibleTypes::types,
                         Not(Contains(CREDIT_CARD_VERIFICATION_CODE)))));
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

  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", credit_card_number, "04",
                          credit_card_exp_year, "1");
  credit_card.set_guid(MakeGuid(3));

  std::vector<PossibleTypes> possible_types =
      DeterminePossibleFieldTypesForUpload(
          std::vector<AutofillProfile>(), {credit_card},
          std::vector<EntityInstance>(), std::vector<LoyaltyCard>(),
          /*fields_that_match_state=*/{},
          /*last_unlocked_credit_card_cvc=*/u"", std::vector<OneTimeToken>(),
          "en-us", form_structure->fields());
  EXPECT_THAT(possible_types,
              Each(Field(&PossibleTypes::types,
                         Not(Contains(CREDIT_CARD_VERIFICATION_CODE)))));
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

  LoyaltyCard loyalty_card = test::CreateLoyaltyCard();
  loyalty_card.set_loyalty_card_number(loyalty_card_number);
  std::vector<PossibleTypes> possible_types =
      DeterminePossibleFieldTypesForUpload(
          std::vector<AutofillProfile>(), std::vector<CreditCard>(),
          std::vector<EntityInstance>(), {loyalty_card},
          /*fields_that_match_state=*/{},
          /*last_unlocked_credit_card_cvc=*/u"", std::vector<OneTimeToken>(),
          "en-us", form_structure->fields());

  CheckThatOnlyFieldByIndexHasThisPossibleType(possible_types, 1,
                                               LOYALTY_MEMBERSHIP_ID);
}

// Tests that a loyalty card number that is also a valid email address is not
// considered a loyalty card.
TEST_F(DeterminePossibleFieldTypesForUploadTest,
       CrowdsourceLoyaltyCardField_NotAnEmail) {
  constexpr char loyalty_card_number_as_email[] = "test@example.com";

  FormData form;
  form.set_fields({CreateTestFormField("loyalty_number", "loyalty_number",
                                       loyalty_card_number_as_email,
                                       FormControlType::kInputText)});

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);

  LoyaltyCard loyalty_card = test::CreateLoyaltyCard();
  loyalty_card.set_loyalty_card_number(loyalty_card_number_as_email);

  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "John", "", "Doe",
                       loyalty_card_number_as_email, "", "", "", "", "", "", "",
                       "");

  std::vector<PossibleTypes> possible_types =
      DeterminePossibleFieldTypesForUpload(
          {profile}, std::vector<CreditCard>(), std::vector<EntityInstance>(),
          {loyalty_card},
          /*fields_that_match_state=*/{},
          /*last_unlocked_credit_card_cvc=*/u"", std::vector<OneTimeToken>(),
          "en-us", form_structure->fields());

  // No loyalty card votes.
  EXPECT_THAT(possible_types[0].types, UnorderedElementsAre(EMAIL_ADDRESS));
}

// Tests if the OTP field is detected.
TEST_F(DeterminePossibleFieldTypesForUploadTest, CrowdsourceOtpField) {
  constexpr char kOtp[] = "123456";

  FormData form;
  form.set_fields({CreateTestFormField("first-name", "first-name", "Pippi",
                                       FormControlType::kInputText),
                   CreateTestFormField("last-name", "last-name", "Longstocking",
                                       FormControlType::kInputText),
                   CreateTestFormField("otp-field", "otp-field", kOtp,
                                       FormControlType::kInputText)});

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);

  std::vector<OneTimeToken> recent_otps = {
      OneTimeToken(OneTimeTokenType::kSmsOtp, kOtp, base::Time::Now())};
  std::vector<PossibleTypes> possible_types_otp =
      DeterminePossibleFieldTypesForUpload(
          std::vector<AutofillProfile>(), std::vector<CreditCard>(),
          std::vector<EntityInstance>(), std::vector<LoyaltyCard>(),
          /*fields_that_match_state=*/{},
          /*last_unlocked_credit_card_cvc=*/u"", recent_otps, "en-us",
          form_structure->fields());

  CheckThatOnlyFieldByIndexHasThisPossibleType(possible_types_otp, 2,
                                               ONE_TIME_CODE);
}

// Tests OTP field is not detected if there are no recently received OTPs.
TEST_F(DeterminePossibleFieldTypesForUploadTest,
       CrowdsourceNoOtpFieldDueToNoRecentOtp) {
  constexpr char kOtp[] = "123456";

  FormData form;
  form.set_fields({CreateTestFormField("first-name", "first-name", "Pippi",
                                       FormControlType::kInputText),
                   CreateTestFormField("last-name", "last-name", "Longstocking",
                                       FormControlType::kInputText),
                   CreateTestFormField("otp-field", "otp-field", kOtp,
                                       FormControlType::kInputText)});

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);

  std::vector<PossibleTypes> possible_types =
      DeterminePossibleFieldTypesForUpload(
          std::vector<AutofillProfile>(), std::vector<CreditCard>(),
          std::vector<EntityInstance>(), std::vector<LoyaltyCard>(),
          /*fields_that_match_state=*/{},
          /*last_unlocked_credit_card_cvc=*/u"", std::vector<OneTimeToken>(),
          "en-us", form_structure->fields());

  EXPECT_THAT(possible_types,
              Each(Field(&PossibleTypes::types, Not(Contains(ONE_TIME_CODE)))));
}

// Tests other fields are not detected as OTPs
TEST_F(DeterminePossibleFieldTypesForUploadTest,
       CrowdsourceNoOtpFieldDueToFormatMismatch) {
  constexpr char kCvc[] = "1234";
  constexpr char kCreditCardNumber[] = "4234-5678-9012-3456";

  FormData form;
  form.set_fields(
      {// Credit card number is not detected as OTP
       CreateTestFormField("number", "number", kCreditCardNumber,
                           FormControlType::kInputText),
       // CVC field is not detected as OTP
       CreateTestFormField("cvc", "cv_", kCvc, FormControlType::kInputText)});

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);

  std::vector<PossibleTypes> possible_types =
      DeterminePossibleFieldTypesForUpload(
          std::vector<AutofillProfile>(), std::vector<CreditCard>(),
          std::vector<EntityInstance>(), std::vector<LoyaltyCard>(),
          /*fields_that_match_state=*/{},
          /*last_unlocked_credit_card_cvc=*/u"", std::vector<OneTimeToken>(),
          "en-us", form_structure->fields());

  EXPECT_THAT(possible_types,
              Each(Field(&PossibleTypes::types, Not(Contains(ONE_TIME_CODE)))));
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
      CreateTestFormField("issue", "issue-day", "01",
                          FormControlType::kInputText),
      CreateTestFormField("issue", "issue-month", "09",
                          FormControlType::kInputText),
      CreateTestFormField("issue", "issue-year", "2010",
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

  EXPECT_THAT(
      DeterminePossibleFieldTypesForUpload(
          std::vector<AutofillProfile>(), std::vector<CreditCard>(),
          base::span_from_ref(entity), std::vector<LoyaltyCard>(),
          /*fields_that_match_state=*/{},
          /*last_unlocked_credit_card_cvc=*/u"", std::vector<OneTimeToken>(),
          "en-US", form_structure->fields()),
      ElementsAre(HasTypes(NAME_FIRST),                   //
                  HasTypes(NAME_LAST, NAME_LAST_SECOND),  //
                  HasTypes(PASSPORT_NUMBER),              //
                  HasTypes(PASSPORT_EXPIRATION_DATE),     //
                  HasTypes(PASSPORT_ISSUE_DATE),          //
                  HasTypes(PASSPORT_ISSUE_DATE),          //
                  HasTypes(PASSPORT_ISSUE_DATE),          //
                  HasTypes(UNKNOWN_TYPE)));
}

// Tests if format strings are crowdsourced for certain Autofill AI FieldTypes.
TEST_F(DeterminePossibleFieldTypesForUploadTest,
       CrowdsourceAutofillAiFormatStrings) {
  FormData form;
  form.set_fields({
      // Complete first/last name.
      CreateTestFormField("first-name", "first-name", "Pippi",
                          FormControlType::kInputText),
      CreateTestFormField("last-name", "last-name", "Longstocking",
                          FormControlType::kInputText),
      // Complete passport number.
      CreateTestFormField("number", "number", "0123456789",
                          FormControlType::kInputText),
      // Affixes of the passport number.
      CreateTestFormField("number", "number", "0123",
                          FormControlType::kInputText),
      CreateTestFormField("number", "number", "789",
                          FormControlType::kInputText),
      CreateTestFormField("number", "number", "23456789",
                          FormControlType::kInputText),
      // These two are too long.
      CreateTestFormField("number", "number", "012345678",
                          FormControlType::kInputText),
      CreateTestFormField("number", "number", "123456789",
                          FormControlType::kInputText),
      // Date format strings.
      CreateTestFormField("expiry-date", "expiry-date", "30/08/2019",
                          FormControlType::kInputText),
      CreateTestFormField("issue", "issue-day", "01",
                          FormControlType::kInputText),
      CreateTestFormField("issue", "issue-month", "09",
                          FormControlType::kInputText),
      CreateTestFormField("issue", "issue-year", "2010",
                          FormControlType::kInputText),
      // Flight number.
      CreateTestFormField("airline", "airline", "LH",
                          FormControlType::kInputText),
      CreateTestFormField("number", "number", "93",
                          FormControlType::kInputText),
      CreateTestFormField("flight number", "flight number", "LH93",
                          FormControlType::kInputText),
      // No format string.
      CreateTestFormField("wrong-country", "wrong-country", "Finland",
                          FormControlType::kInputText),
  });
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);

  const EntityInstance passport_entity = test::GetPassportEntityInstance({
      .name = u"Pippi Longstocking",
      .number = u"0123456789",
      .country = u"Sweden",
      .expiry_date = u"2019-08-30",
      .issue_date = u"2010-09-01",
  });

  const EntityInstance flight_entity =
      test::GetFlightReservationEntityInstance({.flight_number = u"LH93"});

  EXPECT_THAT(
      DeterminePossibleFieldTypesForUpload(
          std::vector<AutofillProfile>(), std::vector<CreditCard>(),
          {passport_entity, flight_entity}, std::vector<LoyaltyCard>(),
          /*fields_that_match_state=*/{},
          /*last_unlocked_credit_card_cvc=*/u"", std::vector<OneTimeToken>(),
          "en-US", form_structure->fields()),
      ElementsAre(
          AllOf(HasTypes(NAME_FIRST), HasNoFormats()),
          AllOf(HasTypes(NAME_LAST, NAME_LAST_SECOND), HasNoFormats()),
          AllOf(HasTypes(PASSPORT_NUMBER), HasAffixFormats("0")),
          AllOf(HasTypes(PASSPORT_NUMBER), HasAffixFormats("4")),
          AllOf(HasTypes(PASSPORT_NUMBER), HasAffixFormats("-3")),
          AllOf(HasTypes(PASSPORT_NUMBER), HasAffixFormats("-8")),
          AllOf(HasTypes(UNKNOWN_TYPE), HasNoFormats()),
          AllOf(HasTypes(UNKNOWN_TYPE), HasNoFormats()),
          AllOf(HasTypes(PASSPORT_EXPIRATION_DATE),
                HasDateFormats("DD/MM/YYYY")),
          AllOf(HasTypes(PASSPORT_ISSUE_DATE), HasDateFormats("DD", "MM")),
          AllOf(HasTypes(PASSPORT_ISSUE_DATE), HasDateFormats("DD", "MM")),
          AllOf(HasTypes(PASSPORT_ISSUE_DATE), HasDateFormats("YYYY")),
          AllOf(HasTypes(FLIGHT_RESERVATION_FLIGHT_NUMBER),
                HasFlightNumberFormats("A")),
          AllOf(HasTypes(FLIGHT_RESERVATION_FLIGHT_NUMBER),
                HasFlightNumberFormats("N")),
          AllOf(HasTypes(FLIGHT_RESERVATION_FLIGHT_NUMBER),
                HasFlightNumberFormats("F")),
          AllOf(HasTypes(UNKNOWN_TYPE), HasNoFormats())));
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

// Test fixture for FindDatesAndSetFormatStrings(). It's an interneal function
// of determine_possible_field_types.cc but it's sufficiently complex for a
// separate test.
class FindDatesAndSetFormatStringsTest : public testing::Test {
 public:
  struct DatesAndFormats {
    base::flat_set<data_util::Date> dates;
    std::set<std::pair<FormatString_Type, std::u16string>> formats;
  };

  // FindDatesAndSetFormatStrings() does two things:
  // - It stores the format strings in `PossibleTypes::formats`.
  // - It returns the found dates and pointers to the `PossibleTypes` of the
  //   fields that contribute to the date.
  //
  // That's ergonomic inside determine_possible_field_types.cc but not for
  // tests.
  //
  // We therefore merge the found dates and formats per field and return them in
  // a vector whose `i`th element corresponds to `fields[i]`.
  static std::vector<DatesAndFormats> FindDatesAndSetFormatStrings(
      base::span<const std::unique_ptr<AutofillField>> fields) {
    std::vector<PossibleTypes> possible_types;
    possible_types.resize(fields.size());
    base::flat_set<std::pair<data_util::Date, PossibleTypes*>> dates =
        FindDatesAndSetFormatStringsForTesting(fields, possible_types);

    std::vector<DatesAndFormats> dafs;
    dafs.resize(fields.size());
    for (auto [pt, daf] : base::zip(possible_types, dafs)) {
      daf.formats = pt.formats;
      for (const auto& p : dates) {
        if (&pt == p.second) {
          daf.dates.insert(p.first);
        }
      }
    }
    return dafs;
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

  static auto HasDatesAndFormatStrings(
      base::span<const data_util::Date> dates,
      base::span<const std::string_view> formats) {
    auto format_pairs = base::ToVector(formats, [](std::string_view s) {
      return Pair(FormatString_Type_DATE, base::UTF8ToUTF16(s));
    });
    return AllOf(
        Field(&DatesAndFormats::dates, UnorderedElementsAreArray(dates)),
        Field(&DatesAndFormats::formats,
              UnorderedElementsAreArray(format_pairs)));
  }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

// Tests that non-text <input> do not match any format string.
TEST_F(FindDatesAndSetFormatStringsTest, InputNonText) {
  using enum FormControlType;
  EXPECT_THAT(FindDatesAndSetFormatStrings({
                  CreateInput("2025-12-31", kInputDate),
                  CreateInput("2025-12", kInputMonth),
                  CreateInput("2025-12-31", kInputNumber),
                  CreateInput("2025-12-31", kInputPassword),
                  CreateInput("2025-12-31", kInputSearch),
                  CreateInput("2025-12-31", kInputUrl),
              }),
              Each(HasDatesAndFormatStrings({}, {})));
}

struct DateSingleTextParam {
  std::string ToString() const {
    std::vector<std::string> date_strings =
        base::ToVector(dates, [](data_util::Date date) {
          return base::UTF16ToUTF8(data_util::FormatDate(date, u"YYYY-MM-DD"));
        });
    return base::StrCat(
        {"value: ", value, "\n",                                    //
         "dates: [ ", base::JoinString(date_strings, ", "), "]\n",  //
         "formats: [", base::JoinString(format_strings, ", "), "]"});
  }

  // The field's fake value.
  std::string_view value;
  // The expected date associated with the field.
  std::vector<data_util::Date> dates;
  // The expected format string of the field.
  std::vector<std::string_view> format_strings;
};

// Test fixture for a single <input type=text> whose value may be a complete
// date.
class FindDatesAndSetFormatStringsTest_SingleTextInput
    : public FindDatesAndSetFormatStringsTest,
      public testing::WithParamInterface<DateSingleTextParam> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    FindDatesAndSetFormatStringsTest_SingleTextInput,
    testing::ValuesIn(std::vector<DateSingleTextParam>{
        {"2025-12-31", {{2025, 12, 31}}, {"YYYY-MM-DD", "YYYY-M-D"}},
        {"31/12/2025", {{2025, 12, 31}}, {"DD/MM/YYYY", "D/M/YYYY"}},
        {"31.12.2025", {{2025, 12, 31}}, {"DD.MM.YYYY", "D.M.YYYY"}},
        {"31-01-2025", {{2025, 01, 31}}, {"DD-MM-YYYY"}},
        {"31-31-2025", {{}}, {}},
        {"31012025", {{2025, 01, 31}}, {"DDMMYYYY"}},
        {"01/01/01", {{2001, 01, 01}}, {"DD/MM/YY", "MM/DD/YY", "YY/MM/DD"}},
        {"12/12/12",
         {{2012, 12, 12}},
         {"DD/MM/YY", "MM/DD/YY", "YY/MM/DD", "D/M/YY", "M/D/YY", "YY/M/D"}},
        {"31/12/12",
         {{2012, 12, 31}, {2031, 12, 12}},
         {"DD/MM/YY", "YY/MM/DD", "D/M/YY", "YY/M/D"}},
        {"12/13/12", {{2012, 12, 13}}, {"MM/DD/YY", "M/D/YY"}},
        {"12/12/32",
         {{2032, 12, 12}},
         {"DD/MM/YY", "MM/DD/YY", "D/M/YY", "M/D/YY"}},
        {"13/13/12", {}, {}},
        {"foobar", {}, {}},
    }));

// Tests that the values of <input type=text> match certain format strings.
TEST_P(FindDatesAndSetFormatStringsTest_SingleTextInput, SingleTextInput) {
  SCOPED_TRACE(testing::Message() << "Values are:\n" << GetParam().ToString());
  std::unique_ptr<AutofillField> field = CreateInput(GetParam().value);
  if (GetParam().format_strings.empty()) {
    EXPECT_THAT(FindDatesAndSetFormatStrings(base::span_from_ref(field)),
                ElementsAre(HasDatesAndFormatStrings({}, {})));
  } else {
    EXPECT_THAT(FindDatesAndSetFormatStrings(base::span_from_ref(field)),
                ElementsAre(HasDatesAndFormatStrings(
                    GetParam().dates, GetParam().format_strings)));
  }
}

struct DateMultipleTextParam {
  struct Field {
    std::string ToString() const {
      std::vector<std::string> date_strings =
          base::ToVector(dates, [](data_util::Date date) {
            return base::UTF16ToUTF8(
                data_util::FormatDate(date, u"YYYY-MM-DD"));
          });
      return base::StrCat(
          {"- label: ", label, "\n",                                   //
           "  value: ", value, "\n",                                   //
           "  dates: [", base::JoinString(date_strings, ", "), "]\n",  //
           "  format strings: [", base::JoinString(format_strings, ", "), "]"});
    }

    // The field's fake label.
    std::string_view label;
    // The field's fake value.
    std::string_view value;
    // The expected date associated with the field.
    std::vector<data_util::Date> dates;
    // The expected format string of the field.
    std::vector<std::string_view> format_strings;
  };

  std::string ToString() const {
    return base::JoinString(base::ToVector(fields, &Field::ToString), "\n");
  }

  std::vector<Field> fields;
};

// Test fixture for a sequences of <input type=text> whose combined values may
// be a complete date.
class FindDatesAndSetFormatStringsTest_MultipleTextInput
    : public FindDatesAndSetFormatStringsTest,
      public testing::WithParamInterface<DateMultipleTextParam> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    FindDatesAndSetFormatStringsTest_MultipleTextInput,
    testing::ValuesIn(std::vector<DateMultipleTextParam>{
        {{{"Date", "2025", {{2025, 12, 31}}, {"YYYY"}},
          {"Date", "12", {{2025, 12, 31}}, {"MM", "M"}},
          {"Date", "31", {{2025, 12, 31}}, {"DD", "D"}}}},
        {{{"Date", "31", {{2025, 12, 31}}, {"DD", "D"}},
          {"", "12", {{2025, 12, 31}}, {"MM", "M"}},
          {"", "2025", {{2025, 12, 31}}, {"YYYY"}}}},
        {{{"Date", "31", {{2025, 01, 31}}, {"DD"}},
          {"Date", "01", {{2025, 01, 31}}, {"MM"}},
          {"Date", "2025", {{2025, 01, 31}}, {"YYYY"}}}},
        {{{"Date", "31", {}, {}},
          {"Date", "31", {}, {}},
          {"Date", "2025", {}, {}}}},
        {{{"Date", "12", {{2012, 12, 12}}, {"DD", "D", "MM", "M", "YY"}},
          {"Date", "12", {{2012, 12, 12}}, {"DD", "D", "MM", "M"}},
          {"Date", "12", {{2012, 12, 12}}, {"DD", "D", "YY"}}}},
        {{{"Date", "31", {{2012, 12, 31}, {2031, 12, 12}}, {"DD", "D", "YY"}},
          {"Date", "12", {{2012, 12, 31}, {2031, 12, 12}}, {"MM", "M"}},
          {"Date", "12", {{2012, 12, 31}, {2031, 12, 12}}, {"DD", "D", "YY"}}}},
        {{{"Date", "12", {{2012, 12, 13}}, {"MM", "M"}},
          {"Date", "13", {{2012, 12, 13}}, {"DD", "D"}},
          {"Date", "12", {{2012, 12, 13}}, {"YY"}}}},
        {{{"Date", "12", {{2032, 12, 12}}, {"DD", "D", "MM", "M"}},
          {"Date", "12", {{2032, 12, 12}}, {"DD", "D", "MM", "M"}},
          {"Date", "32", {{2032, 12, 12}}, {"YY"}}}},
        {{{"Date", "13", {}, {}},
          {"Date", "13", {}, {}},
          {"Date", "12", {}, {}}}},
        {{{"Date", "1", {}, {}},
          {"Date", "2025", {{2025, 12, 31}}, {"YYYY"}},
          {"Date", "12", {{2025, 12, 31}}, {"MM", "M"}},
          {"Date", "31", {{2025, 12, 31}}, {"DD", "D"}}}},
        {{{"Date", "12", {{2012, 12, 12}}, {"DD", "D", "MM", "M", "YY"}},
          {"Date", "12", {{2012, 12, 12}}, {"DD", "D", "MM", "M", "YY"}},
          {"Date", "12", {{2012, 12, 12}}, {"DD", "D", "MM", "M", "YY"}},
          {"Date", "12", {{2012, 12, 12}}, {"DD", "D", "YY"}}}},
        {{{"Date", "31", {}, {}},
          {"Something else", "12", {}, {}},
          {"Something completely different", "2025", {}, {}}}},
        {{{"Date", "1", {}, {}},
          {"Date", "1", {}, {}},
          {"Date", "1", {}, {}},
          {"Date", "1", {}, {}}}},
        {{{"Date", "123", {}, {}},
          {"Date", "123", {}, {}},
          {"Date", "123", {}, {}},
          {"Date", "123", {}, {}}}},
        {{{"Date", "ash", {}, {}},
          {"Date", "sho", {}, {}},
          {"Date", "hte", {}, {}},
          {"Date", "tne", {}, {}},
          {"Date", "neo", {}, {}}}}}));

// Tests that the combined values of sequences of <input type=text> match
// certain format strings. For example,
//   <input type=text value=31>
//   <input type=text value=12>
//   <input type=text value=2025>
// represents the date 31/12/2025 and the resulting format strings for the
// three fields should be DD od D, MM or M, and YYYY, respectively.
TEST_P(FindDatesAndSetFormatStringsTest_MultipleTextInput, MultipleTextInput) {
  SCOPED_TRACE(testing::Message() << "Fields are:\n" << GetParam().ToString());
  std::vector<std::unique_ptr<AutofillField>> fields = base::ToVector(
      GetParam().fields, [](const DateMultipleTextParam::Field& field) {
        std::unique_ptr<AutofillField> f = CreateInput(field.value);
        f->set_label(base::UTF8ToUTF16(field.label));
        return f;
      });

  EXPECT_THAT(FindDatesAndSetFormatStrings(fields),
              ElementsAreArray(base::ToVector(
                  GetParam().fields,
                  [](const DateMultipleTextParam::Field& expectation) {
                    return HasDatesAndFormatStrings(expectation.dates,
                                                    expectation.format_strings);
                  })));
}

// Test fixture for DetermineAvailableFieldTypes().
class DetermineAvailableFieldTypesTest : public ::testing::Test {
 public:
  DetermineAvailableFieldTypesTest() {
    features_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillAiWithDataSchema,
                              features::kAutofillEnableLoyaltyCardsFilling,
                              features::
                                  kAutofillEnableEmailOrLoyaltyCardsFilling},
        /*disabled_features=*/{});
  }

 protected:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList features_;
};

// Tests that entities are included in the set of available field types.
TEST_F(DetermineAvailableFieldTypesTest, Entities) {
  EntityInstance entity = test::GetPassportEntityInstance();
  FieldTypeSet available_types = DetermineAvailableFieldTypes(
      /*profiles=*/{},
      /*credit_cards=*/{},
      /*entities=*/{entity},
      /*loyalty_cards=*/{},
      /*last_unlocked_credit_card_cvc=*/u"",
      /*recent_otps=*/{},
      /*app_locale=*/"en-US");
  EXPECT_THAT(
      available_types,
      UnorderedElementsAre(NAME_FULL, NAME_FIRST, NAME_LAST, NAME_LAST_SECOND,
                           PASSPORT_NUMBER, PASSPORT_EXPIRATION_DATE,
                           PASSPORT_ISSUE_DATE, PASSPORT_ISSUING_COUNTRY));
}

// Tests that loyalty cards are included in the set of available field types.
TEST_F(DetermineAvailableFieldTypesTest, LoyaltyCards) {
  LoyaltyCard card = test::CreateLoyaltyCard();
  FieldTypeSet available_types = DetermineAvailableFieldTypes(
      /*profiles=*/{},
      /*credit_cards=*/{},
      /*entities=*/{},
      /*loyalty_cards=*/{card},
      /*last_unlocked_credit_card_cvc=*/u"",
      /*recent_otps=*/{},
      /*app_locale=*/"");
  EXPECT_TRUE(available_types.contains(LOYALTY_MEMBERSHIP_ID));
}

}  // namespace
}  // namespace autofill
