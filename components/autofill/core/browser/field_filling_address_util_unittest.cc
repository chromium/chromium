// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/field_filler.h"

#include <memory>
#include <vector>

#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/address_normalizer.h"
#include "components/autofill/core/browser/address_normalizer_impl.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/null_storage.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"
#include "third_party/libaddressinput/src/cpp/test/testdata_source.h"

namespace autofill {

namespace {

using ::i18n::addressinput::NullStorage;
using ::i18n::addressinput::Source;
using ::i18n::addressinput::Storage;
using ::i18n::addressinput::TestdataSource;

AutofillField CreateTestSelectAutofillField(
    const std::vector<const char*>& values,
    ServerFieldType heuristic_type) {
  AutofillField field{test::CreateTestSelectField(values)};
  field.set_heuristic_type(GetActiveHeuristicSource(), heuristic_type);
  return field;
}

class FieldFillingAddressUtilTest : public testing::Test {
 public:
  FieldFillingAddressUtilTest(const FieldFillingAddressUtilTest&) = delete;
  FieldFillingAddressUtilTest& operator=(const FieldFillingAddressUtilTest&) =
      delete;

 protected:
  FieldFillingAddressUtilTest()
      : address_(std::make_unique<AutofillProfile>(test::GetFullProfile())) {}

  AutofillProfile* address() { return address_.get(); }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  std::unique_ptr<AutofillProfile> address_;
};

// Verify that non credit card related fields with the autocomplete attribute
// set to off are filled on desktop when the feature to Autofill all
// addresses is enabled (default).
TEST_F(FieldFillingAddressUtilTest,
       FillFormField_AutocompleteOffNotRespected_AddressField) {
  AutofillField field;
  field.should_autocomplete = false;
  field.set_heuristic_type(GetActiveHeuristicSource(), NAME_FIRST);

  // Non credit card related field.
  address()->SetRawInfo(NAME_FIRST, u"Test");
  FieldFiller filler(/*app_locale=*/"en-US", /*address_normalizer=*/nullptr);
  filler.FillFormField(field, address(), /*forced_fill_values=*/{}, &field,
                       /*cvc=*/std::u16string(),
                       mojom::ActionPersistence::kFill);

  // Verify that the field is filled in all circumstances.
  EXPECT_EQ(u"Test", field.value);
}

struct FieldFillingAddressUtilTestCase {
  HtmlFieldType field_type;
  size_t field_max_length;
  std::u16string expected_value;

  FieldFillingAddressUtilTestCase(HtmlFieldType field_type,
                                  size_t field_max_length,
                                  std::u16string expected_value)
      : field_type(field_type),
        field_max_length(field_max_length),
        expected_value(expected_value) {}
};

struct AutofillPhoneFieldFillerTestCase
    : public FieldFillingAddressUtilTestCase {
  std::u16string phone_home_whole_number_value;

  AutofillPhoneFieldFillerTestCase(HtmlFieldType field_type,
                                   size_t field_max_length,
                                   std::u16string expected_value,
                                   std::u16string phone_home_whole_number_value)
      : FieldFillingAddressUtilTestCase(field_type,
                                        field_max_length,
                                        expected_value),
        phone_home_whole_number_value(phone_home_whole_number_value) {}
};

class PhoneNumberTest
    : public FieldFillingAddressUtilTest,
      public testing::WithParamInterface<AutofillPhoneFieldFillerTestCase> {};

TEST_P(PhoneNumberTest, FillPhoneNumber) {
  auto test_case = GetParam();
  AutofillField field;
  field.SetHtmlType(test_case.field_type, HtmlFieldMode());
  field.max_length = test_case.field_max_length;

  AutofillProfile address(AddressCountryCode("US"));
  address.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     test_case.phone_home_whole_number_value);
  FieldFiller filler(/*app_locale=*/"en-US", /*address_normalizer=*/nullptr);
  filler.FillFormField(field, &address, /*forced_fill_values=*/{}, &field,
                       /*cvc=*/std::u16string(),
                       mojom::ActionPersistence::kFill);
  EXPECT_EQ(test_case.expected_value, field.value);
}

INSTANTIATE_TEST_SUITE_P(
    FieldFillingAddressUtilTest,
    PhoneNumberTest,
    testing::Values(
        // Filling a prefix type field should just fill the prefix.
        AutofillPhoneFieldFillerTestCase{HtmlFieldType::kTelLocalPrefix,
                                         /*field_max_length=*/0, u"555",
                                         u"+15145554578"},
        // Filling a suffix type field with a phone number of 7 digits should
        // just fill the suffix.
        AutofillPhoneFieldFillerTestCase{HtmlFieldType::kTelLocalSuffix,
                                         /*field_max_length=*/0, u"4578",
                                         u"+15145554578"},
        // TODO(crbug.com/581485): There should be a test case where the full
        // number is requested (HtmlFieldType::kTel) but a
        // field_max_length of 3 would fill the prefix. Filling a phone type
        // field with a max length of 4 should fill only the suffix.
        AutofillPhoneFieldFillerTestCase{HtmlFieldType::kTel,
                                         /*field_max_length=*/4, u"4578",
                                         u"+15145554578"},
        // Filling a phone type field with a max length of 10 with a phone
        // number including the country code should fill the phone number
        // without the country code.
        AutofillPhoneFieldFillerTestCase{HtmlFieldType::kTel,
                                         /*field_max_length=*/10, u"5145554578",
                                         u"+15145554578"},
        // Filling a phone type field with a max length of 5 with a phone number
        // should fill with the last 5 digits of that phone number.
        AutofillPhoneFieldFillerTestCase{HtmlFieldType::kTel,
                                         /*field_max_length=*/5, u"54578",
                                         u"+15145554578"},
        // Filling a phone type field with a max length of 10 with a phone
        // number including the country code should fill the phone number
        // without the country code.
        AutofillPhoneFieldFillerTestCase{HtmlFieldType::kTel,
                                         /*field_max_length=*/10, u"123456789",
                                         u"+886123456789"}));

struct FillSelectTestCase {
  std::vector<const char*> select_values;
  const char16_t* input_value;
  const char16_t* expected_value_without_normalization;
  const char16_t* expected_value_with_normalization = nullptr;
};

class AutofillSelectWithStatesTest
    : public FieldFillingAddressUtilTest,
      public testing::WithParamInterface<FillSelectTestCase> {
 public:
  AutofillSelectWithStatesTest() {
    base::FilePath file_path;
    CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path));
    file_path = file_path.Append(FILE_PATH_LITERAL("third_party"))
                    .Append(FILE_PATH_LITERAL("libaddressinput"))
                    .Append(FILE_PATH_LITERAL("src"))
                    .Append(FILE_PATH_LITERAL("testdata"))
                    .Append(FILE_PATH_LITERAL("countryinfo.txt"));

    normalizer_ = std::make_unique<AddressNormalizerImpl>(
        std::unique_ptr<Source>(
            new TestdataSource(true, file_path.AsUTF8Unsafe())),
        std::unique_ptr<Storage>(new NullStorage), "en-US");

    test::PopulateAlternativeStateNameMapForTesting(
        "US", "California",
        {{.canonical_name = "California",
          .abbreviations = {"CA"},
          .alternative_names = {}}});
    test::PopulateAlternativeStateNameMapForTesting(
        "US", "North Carolina",
        {{.canonical_name = "North Carolina",
          .abbreviations = {"NC"},
          .alternative_names = {}}});
    // Make sure the normalizer is done initializing its member(s) in
    // background task(s).
    task_environment_.RunUntilIdle();
  }

  AutofillSelectWithStatesTest(const AutofillSelectWithStatesTest&) = delete;
  AutofillSelectWithStatesTest& operator=(const AutofillSelectWithStatesTest&) =
      delete;

 protected:
  AddressNormalizer* normalizer() { return normalizer_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<AddressNormalizerImpl> normalizer_;
};

TEST_P(AutofillSelectWithStatesTest, FillSelectWithStates) {
  auto test_case = GetParam();
  AutofillField field = CreateTestSelectAutofillField(test_case.select_values,
                                                      ADDRESS_HOME_STATE);
  // Without a normalizer.
  AutofillProfile address = test::GetFullProfile();
  address.SetRawInfo(ADDRESS_HOME_STATE, test_case.input_value);
  FieldFiller filler(/*app_locale=*/"en-US", /*address_normalizer=*/nullptr);
  filler.FillFormField(field, &address, /*forced_fill_values=*/{}, &field,
                       /*cvc=*/std::u16string(),
                       mojom::ActionPersistence::kFill);
  // nullptr means we expect them not to match without normalization.
  if (test_case.expected_value_without_normalization != nullptr) {
    EXPECT_EQ(test_case.expected_value_without_normalization, field.value);
  }

  // With a normalizer.
  AutofillProfile canadian_address = test::GetFullCanadianProfile();
  canadian_address.SetRawInfo(ADDRESS_HOME_STATE, test_case.input_value);
  // Fill a first time without loading the rules for the region.
  FieldFiller canadian_filler(/*app_locale=*/"en-US", normalizer());
  canadian_filler.FillFormField(
      field, &canadian_address, /*forced_fill_values=*/{}, &field,
      /*cvc=*/std::u16string(), mojom::ActionPersistence::kFill);
  // If the expectation with normalization is nullptr, this means that the same
  // result than without a normalizer is expected.
  if (test_case.expected_value_with_normalization == nullptr) {
    EXPECT_EQ(test_case.expected_value_without_normalization, field.value);
  } else {
    // We needed a normalizer with loaded rules. The first fill should have
    // failed.
    EXPECT_NE(test_case.expected_value_with_normalization, field.value);

    // Load the rules and try again.
    normalizer()->LoadRulesForRegion("CA");
    canadian_filler.FillFormField(
        field, &canadian_address, /*forced_fill_values=*/{}, &field,
        /*cvc=*/std::u16string(), mojom::ActionPersistence::kFill);
    EXPECT_EQ(test_case.expected_value_with_normalization, field.value);
  }
}

INSTANTIATE_TEST_SUITE_P(
    FieldFillingAddressUtilTest,
    AutofillSelectWithStatesTest,
    testing::Values(
        // Filling the abbreviation.
        FillSelectTestCase{{"Alabama", "California"}, u"CA", u"California"},
        // Attempting to fill the full name in a select full of abbreviations.
        FillSelectTestCase{{"AL", "CA"}, u"California", u"CA"},
        // Different case and diacritics.
        FillSelectTestCase{{"QUÉBEC", "ALBERTA"}, u"Quebec", u"QUÉBEC"},
        // The value and the field options are different but normalize to the
        // same (NB).
        FillSelectTestCase{{"Nouveau-Brunswick", "Alberta"},
                           u"New Brunswick",
                           nullptr,
                           u"Nouveau-Brunswick"},
        FillSelectTestCase{{"NB", "AB"}, u"New Brunswick", nullptr, u"NB"},
        FillSelectTestCase{{"NB", "AB"}, u"Nouveau-Brunswick", nullptr, u"NB"},
        FillSelectTestCase{{"Nouveau-Brunswick", "Alberta"},
                           u"NB",
                           nullptr,
                           u"Nouveau-Brunswick"},
        FillSelectTestCase{{"New Brunswick", "Alberta"},
                           u"NB",
                           nullptr,
                           u"New Brunswick"},
        // Inexact state names.
        FillSelectTestCase{
            {"SC - South Carolina", "CA - California", "NC - North Carolina"},
            u"California",
            u"CA - California"},
        // Don't accidentally match "Virginia" to "West Virginia".
        FillSelectTestCase{
            {"WV - West Virginia", "VA - Virginia", "NV - North Virginia"},
            u"Virginia",
            u"VA - Virginia"},
        // Do accidentally match "Virginia" to "West Virginia".
        // TODO(crbug.com/624770): This test should not pass, but it does
        // because "Virginia" is a substring of "West Virginia".
        FillSelectTestCase{{"WV - West Virginia", "TX - Texas"},
                           u"Virginia",
                           u"WV - West Virginia"},
        // Tests that substring matches work for full state names (a full token
        // match isn't required). Also tests that matches work for states with
        // whitespace in the middle.
        FillSelectTestCase{{"California.", "North Carolina."},
                           u"North Carolina",
                           u"North Carolina."},
        FillSelectTestCase{{"NC - North Carolina", "CA - California"},
                           u"CA",
                           u"CA - California"}));

TEST_F(FieldFillingAddressUtilTest, FillSelectWithCountries) {
  AutofillField field = CreateTestSelectAutofillField({"Albania", "Canada"},
                                                      ADDRESS_HOME_COUNTRY);
  AutofillProfile address = test::GetFullProfile();
  address.SetRawInfo(ADDRESS_HOME_COUNTRY, u"CA");
  FieldFiller filler(/*app_locale=*/"en-US", /*address_normalizer=*/nullptr);
  filler.FillFormField(field, &address, /*forced_fill_values=*/{}, &field,
                       /*cvc=*/std::u16string(),
                       mojom::ActionPersistence::kFill);
  EXPECT_EQ(u"Canada", field.value);
}

TEST_F(FieldFillingAddressUtilTest, FillStreetAddressTextArea) {
  AutofillField field;
  field.form_control_type = FormControlType::kTextArea;
  FieldFiller filler(/*app_locale=*/"en-US", /*address_normalizer=*/nullptr);
  field.set_heuristic_type(GetActiveHeuristicSource(),
                           ADDRESS_HOME_STREET_ADDRESS);

  std::u16string value = u"123 Fake St.\nApt. 42";
  address()->SetInfo(AutofillType(ADDRESS_HOME_STREET_ADDRESS), value, "en-US");
  filler.FillFormField(field, address(), /*forced_fill_values=*/{}, &field,
                       /*cvc=*/std::u16string(),
                       mojom::ActionPersistence::kFill);
  EXPECT_EQ(value, field.value);

  std::u16string ja_value = u"桜丘町26-1\nセルリアンタワー6階";
  address()->SetInfo(AutofillType(ADDRESS_HOME_STREET_ADDRESS), ja_value,
                     "ja-JP");
  address()->set_language_code("ja-JP");
  filler.FillFormField(field, address(), /*forced_fill_values=*/{}, &field,
                       /*cvc=*/std::u16string(),
                       mojom::ActionPersistence::kFill);
  EXPECT_EQ(ja_value, field.value);
}

TEST_F(FieldFillingAddressUtilTest, FillStreetAddressTextField) {
  AutofillField field;
  field.form_control_type = FormControlType::kInputText;
  field.set_server_predictions(
      {::autofill::test::CreateFieldPrediction(ADDRESS_HOME_STREET_ADDRESS)});
  FieldFiller filler(/*app_locale=*/"en-US", /*address_normalizer=*/nullptr);

  std::u16string value = u"123 Fake St.\nApt. 42";
  address()->SetInfo(AutofillType(ADDRESS_HOME_STREET_ADDRESS), value, "en-US");
  filler.FillFormField(field, address(), /*forced_fill_values=*/{}, &field,
                       /*cvc=*/std::u16string(),
                       mojom::ActionPersistence::kFill);
  EXPECT_EQ(u"123 Fake St., Apt. 42", field.value);

  std::u16string ja_value = u"桜丘町26-1\nセルリアンタワー6階";
  address()->SetInfo(AutofillType(ADDRESS_HOME_STREET_ADDRESS), ja_value,
                     "ja-JP");
  address()->set_language_code("ja-JP");
  filler.FillFormField(field, address(), /*forced_fill_values=*/{}, &field,
                       /*cvc=*/std::u16string(),
                       mojom::ActionPersistence::kFill);
  EXPECT_EQ(u"桜丘町26-1セルリアンタワー6階", field.value);
}

// Tests that text state fields are filled correctly depending on their
// maxlength attribute value.
struct FillStateTextTestCase {
  HtmlFieldType field_type;
  size_t field_max_length;
  std::u16string value_to_fill;
  std::u16string expected_value;
  bool should_fill;
};

class AutofillStateTextTest
    : public FieldFillingAddressUtilTest,
      public testing::WithParamInterface<FillStateTextTestCase> {};

TEST_P(AutofillStateTextTest, FillStateText) {
  auto test_case = GetParam();
  AutofillField field;
  field.SetHtmlType(test_case.field_type, HtmlFieldMode());
  field.max_length = test_case.field_max_length;

  FieldFiller filler(/*app_locale=*/"en-US", /*address_normalizer=*/nullptr);
  AutofillProfile address = test::GetFullProfile();
  address.SetRawInfo(ADDRESS_HOME_STATE, test_case.value_to_fill);
  bool has_filled = filler.FillFormField(
      field, &address, /*forced_fill_values=*/{}, &field,
      /*cvc=*/std::u16string(), mojom::ActionPersistence::kFill);

  EXPECT_EQ(test_case.should_fill, has_filled);
  EXPECT_EQ(test_case.expected_value, field.value);
}

INSTANTIATE_TEST_SUITE_P(
    FieldFillingAddressUtilTest,
    AutofillStateTextTest,
    testing::Values(
        // Filling a state to a text field with the default maxlength value
        // should
        // fill the state value as is.
        FillStateTextTestCase{HtmlFieldType::kAddressLevel1,
                              /* default value */ 0, u"New York", u"New York",
                              true},
        FillStateTextTestCase{HtmlFieldType::kAddressLevel1,
                              /* default value */ 0, u"NY", u"NY", true},
        // Filling a state to a text field with a maxlength value equal to the
        // value's length should fill the state value as is.
        FillStateTextTestCase{HtmlFieldType::kAddressLevel1, 8, u"New York",
                              u"New York", true},
        // Filling a state to a text field with a maxlength value lower than the
        // value's length but higher than the value's abbreviation should fill
        // the state abbreviation.
        FillStateTextTestCase{HtmlFieldType::kAddressLevel1, 2, u"New York",
                              u"NY", true},
        FillStateTextTestCase{HtmlFieldType::kAddressLevel1, 2, u"NY", u"NY",
                              true},
        // Filling a state to a text field with a maxlength value lower than the
        // value's length and the value's abbreviation should not fill at all.
        FillStateTextTestCase{HtmlFieldType::kAddressLevel1, 1, u"New York",
                              u"", false},
        FillStateTextTestCase{HtmlFieldType::kAddressLevel1, 1, u"NY", u"",
                              false},
        // Filling a state to a text field with a maxlength value lower than the
        // value's length and that has no associated abbreviation should not
        // fill at all.
        FillStateTextTestCase{HtmlFieldType::kAddressLevel1, 3, u"Quebec", u"",
                              false}));

// Tests that augment phone country code fields are filled correctly.
struct FillAugmentedPhoneCountryCodeTestCase {
  std::vector<const char*> phone_country_code_selection_options;
  std::u16string phone_home_whole_number_value;
  std::u16string expected_value;
};

class AutofillFillAugmentedPhoneCountryCodeTest
    : public FieldFillingAddressUtilTest,
      public testing::WithParamInterface<
          FillAugmentedPhoneCountryCodeTestCase> {};

void DoTestFillAugmentedPhoneCountryCodeField(
    const FillAugmentedPhoneCountryCodeTestCase& test_case,
    FormControlType field_type) {
  AutofillField field(test::CreateTestSelectOrSelectListField(
      /*label=*/"", /*name=*/"", /*value=*/"", /*autocomplete=*/"",
      test_case.phone_country_code_selection_options,
      test_case.phone_country_code_selection_options, field_type));
  field.set_heuristic_type(GetActiveHeuristicSource(), PHONE_HOME_COUNTRY_CODE);

  AutofillProfile address(AddressCountryCode("US"));
  address.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     test_case.phone_home_whole_number_value);
  FieldFiller filler(/*app_locale=*/"en-US", /*address_normalizer=*/nullptr);
  filler.FillFormField(field, &address, /*forced_fill_values=*/{}, &field,
                       /*cvc=*/std::u16string(),
                       mojom::ActionPersistence::kFill);
  EXPECT_EQ(field.value, test_case.expected_value);
}

TEST_P(AutofillFillAugmentedPhoneCountryCodeTest,
       FillAugmentedPhoneCountryCodeField) {
  DoTestFillAugmentedPhoneCountryCodeField(GetParam(),
                                           FormControlType::kSelectOne);
}

TEST_P(AutofillFillAugmentedPhoneCountryCodeTest,
       FillAugmentedPhoneCountryCodeSelectListField) {
  DoTestFillAugmentedPhoneCountryCodeField(GetParam(),
                                           FormControlType::kSelectList);
}

INSTANTIATE_TEST_SUITE_P(
    FieldFillingAddressUtilTest,
    AutofillFillAugmentedPhoneCountryCodeTest,
    testing::Values(
        // Filling phone country code selection field when one of the options
        // exactly matches the phone country code.
        FillAugmentedPhoneCountryCodeTestCase{{"91", "1", "20", "49"},
                                              u"+15145554578",
                                              u"1"},
        // Filling phone country code selection field when the options
        // are preceded by a plus sign and the field is of
        // `PHONE_HOME_COUNTRY_CODE` type.
        FillAugmentedPhoneCountryCodeTestCase{{"+91", "+1", "+20", "+49"},
                                              u"+918890888888",
                                              u"+91"},
        // Filling phone country code selection field when the options
        // are preceded by a '00' and the field is of `PHONE_HOME_COUNTRY_CODE`
        // type.
        FillAugmentedPhoneCountryCodeTestCase{{"0091", "001", "0020", "0049"},
                                              u"+918890888888",
                                              u"0091"},
        // Filling phone country code selection field when the options are
        // composed of the country code and the country name.
        FillAugmentedPhoneCountryCodeTestCase{
            {"Please select an option", "+91 (India)", "+1 (United States)",
             "+20 (Egypt)", "+49 (Germany)"},
            u"+49151669087345",
            u"+49 (Germany)"},
        // Filling phone country code selection field when the options are
        // composed of the country code having whitespace and the country name.
        FillAugmentedPhoneCountryCodeTestCase{
            {"Please select an option", "(00 91) India", "(00 1) United States",
             "(00 20) Egypt", "(00 49) Germany"},
            u"+49151669087345",
            u"(00 49) Germany"},
        // Filling phone country code selection field when the options are
        // composed of the country code that is preceded by '00' and the country
        // name.
        FillAugmentedPhoneCountryCodeTestCase{
            {"Please select an option", "(0091) India", "(001) United States",
             "(0020) Egypt", "(0049) Germany"},
            u"+49151669087345",
            u"(0049) Germany"}));

// Tests that the abbreviated state names are selected correctly.
TEST_F(FieldFillingAddressUtilTest, FillSelectAbbreviatedState) {
  test::ClearAlternativeStateNameMapForTesting();
  test::PopulateAlternativeStateNameMapForTesting();

  AutofillField field = CreateTestSelectAutofillField({"BA", "BB", "BC", "BY"},
                                                      ADDRESS_HOME_STATE);
  AutofillProfile address(AddressCountryCode("DE"));
  address.SetRawInfo(ADDRESS_HOME_STATE, u"Bavaria");

  FieldFiller filler(/*app_locale=*/"en-US", /*address_normalizer=*/nullptr);
  filler.FillFormField(field, &address, /*forced_fill_values=*/{}, &field,
                       /*cvc=*/std::u16string(),
                       mojom::ActionPersistence::kFill);
  EXPECT_EQ(u"BY", field.value);
}

// Tests that the localized state names are selected correctly.
TEST_F(FieldFillingAddressUtilTest, FillSelectLocalizedState) {
  test::ClearAlternativeStateNameMapForTesting();
  test::PopulateAlternativeStateNameMapForTesting();

  AutofillField field = CreateTestSelectAutofillField(
      {"Bayern", "Berlin", "Brandenburg", "Bremen"}, ADDRESS_HOME_STATE);
  AutofillProfile address(AddressCountryCode("DE"));
  address.SetRawInfo(ADDRESS_HOME_STATE, u"Bavaria");

  FieldFiller filler(/*app_locale=*/"en-US", /*address_normalizer=*/nullptr);
  filler.FillFormField(field, &address, /*forced_fill_values=*/{}, &field,
                       /*cvc=*/std::u16string(),
                       mojom::ActionPersistence::kFill);
  EXPECT_EQ(u"Bayern", field.value);
}

// Tests that the state names are selected correctly when the state name exists
// as a substring in the selection options.
TEST_F(FieldFillingAddressUtilTest, FillSelectLocalizedStateSubstring) {
  test::ClearAlternativeStateNameMapForTesting();
  test::PopulateAlternativeStateNameMapForTesting();

  AutofillField field = CreateTestSelectAutofillField(
      {"Bavaria Has Munich", "Berlin has Berlin"}, ADDRESS_HOME_STATE);
  AutofillProfile address(AddressCountryCode("DE"));
  address.SetRawInfo(ADDRESS_HOME_STATE, u"Bavaria");

  FieldFiller filler(/*app_locale=*/"en-US", /*address_normalizer=*/nullptr);
  filler.FillFormField(field, &address, /*forced_fill_values=*/{}, &field,
                       /*cvc=*/std::u16string(),
                       mojom::ActionPersistence::kFill);
  EXPECT_EQ(u"Bavaria Has Munich", field.value);
}

// Tests that the state abbreviations are filled in the text field when the
// field length is limited.
TEST_F(FieldFillingAddressUtilTest, FillStateAbbreviationInTextField) {
  test::ClearAlternativeStateNameMapForTesting();
  test::PopulateAlternativeStateNameMapForTesting();

  AutofillField field(test::CreateTestFormField("State", "state", "",
                                                FormControlType::kInputText));
  field.set_heuristic_type(GetActiveHeuristicSource(), ADDRESS_HOME_STATE);
  field.max_length = 4;

  AutofillProfile address(AddressCountryCode("DE"));
  address.SetRawInfo(ADDRESS_HOME_STATE, u"Bavaria");

  FieldFiller filler(/*app_locale=*/"en-US", /*address_normalizer=*/nullptr);
  filler.FillFormField(field, &address, /*forced_fill_values=*/{}, &field,
                       /*cvc=*/std::u16string(),
                       mojom::ActionPersistence::kFill);
  EXPECT_EQ(u"BY", field.value);
}

// Tests that the state names are selected correctly even though the state
// value saved in the address is not recognized by the AlternativeStateNameMap.
TEST_F(FieldFillingAddressUtilTest, FillStateFieldWithSavedValueInProfile) {
  test::ClearAlternativeStateNameMapForTesting();
  test::PopulateAlternativeStateNameMapForTesting();

  AutofillField field = CreateTestSelectAutofillField(
      {"Bavari", "Berlin", "Lower Saxony"}, ADDRESS_HOME_STATE);
  AutofillProfile address(AddressCountryCode("DE"));
  address.SetRawInfo(ADDRESS_HOME_STATE, u"Bavari");

  FieldFiller filler(/*app_locale=*/"en-US", /*address_normalizer=*/nullptr);
  filler.FillFormField(field, &address, /*forced_fill_values=*/{}, &field,
                       /*cvc=*/std::u16string(),
                       mojom::ActionPersistence::kFill);
  EXPECT_EQ(u"Bavari", field.value);
}

// Tests that Autofill does not wrongly fill the state when the appropriate
// state is not in the list of selection options given that the abbreviation is
// saved in the profile.
TEST_F(FieldFillingAddressUtilTest, FillStateFieldWhenStateIsNotInOptions) {
  test::ClearAlternativeStateNameMapForTesting();
  test::PopulateAlternativeStateNameMapForTesting(
      "US", "Colorado",
      {{.canonical_name = "Colorado",
        .abbreviations = {"CO"},
        .alternative_names = {}}});

  AutofillField field = CreateTestSelectAutofillField(
      {"Connecticut", "California"}, ADDRESS_HOME_STATE);
  AutofillProfile address(AddressCountryCode("US"));
  address.SetRawInfo(ADDRESS_HOME_STATE, u"CO");

  FieldFiller filler(/*app_locale=*/"en-US", /*address_normalizer=*/nullptr);
  filler.FillFormField(field, &address, /*forced_fill_values=*/{}, &field,
                       /*cvc=*/std::u16string(),
                       mojom::ActionPersistence::kFill);
  EXPECT_EQ(u"", field.value);
}

// Tests that Autofill uses the static states data of US as a fallback mechanism
// for filling when |AlternativeStateNameMap| is not populated.
TEST_F(FieldFillingAddressUtilTest,
       FillStateFieldWhenAlternativeStateNameMapIsNotPopulated) {
  test::ClearAlternativeStateNameMapForTesting();

  AutofillField field = CreateTestSelectAutofillField(
      {"Colorado", "Connecticut", "California"}, ADDRESS_HOME_STATE);
  AutofillProfile address(AddressCountryCode("US"));
  address.SetRawInfo(ADDRESS_HOME_STATE, u"CO");

  FieldFiller filler(/*app_locale=*/"en-US", /*address_normalizer=*/nullptr);
  filler.FillFormField(field, &address, /*forced_fill_values=*/{}, &field,
                       /*cvc=*/std::u16string(),
                       mojom::ActionPersistence::kFill);
  EXPECT_EQ(u"Colorado", field.value);
}

// Tests that Autofill fills upper case abbreviation in the input field when
// field length is limited.
TEST_F(FieldFillingAddressUtilTest, FillUpperCaseAbbreviationInStateTextField) {
  test::ClearAlternativeStateNameMapForTesting();
  test::PopulateAlternativeStateNameMapForTesting("DE", "Bavaria",
                                                  {{.canonical_name = "Bavaria",
                                                    .abbreviations = {"by"},
                                                    .alternative_names = {}}});

  AutofillField field{test::CreateTestFormField("State", "state", "",
                                                FormControlType::kInputText)};
  field.set_heuristic_type(GetActiveHeuristicSource(), ADDRESS_HOME_STATE);
  field.max_length = 4;

  AutofillProfile address(AddressCountryCode("DE"));
  address.SetRawInfo(ADDRESS_HOME_STATE, u"Bavaria");

  FieldFiller filler(/*app_locale=*/"en-US", /*address_normalizer=*/nullptr);
  filler.FillFormField(field, &address, /*forced_fill_values=*/{}, &field,
                       /*cvc=*/std::u16string(),
                       mojom::ActionPersistence::kFill);
  EXPECT_EQ(u"BY", field.value);
}

// Tests that Autofill does not fill the state when abbreviated data is stored
// in the profile and none of the options match with the abbreviated state.
TEST_F(FieldFillingAddressUtilTest,
       DoNotFillStateFieldWhenAbbrStoredInProfileAndNotInOptionsList) {
  test::ClearAlternativeStateNameMapForTesting();

  AutofillField field = CreateTestSelectAutofillField(
      {"Colombia", "Connecticut", "California"}, ADDRESS_HOME_STATE);
  AutofillProfile address(AddressCountryCode("US"));
  address.SetRawInfo(ADDRESS_HOME_STATE, u"CO");

  FieldFiller filler(/*app_locale=*/"en-US", /*address_normalizer=*/nullptr);
  filler.FillFormField(field, &address, /*forced_fill_values=*/{}, &field,
                       /*cvc=*/std::u16string(),
                       mojom::ActionPersistence::kFill);
  EXPECT_EQ(u"", field.value);
}

}  // namespace

}  // namespace autofill
