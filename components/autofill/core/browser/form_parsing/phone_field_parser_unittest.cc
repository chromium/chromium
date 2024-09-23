// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/phone_field_parser.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {
namespace {

const FormControlType kFieldTypes[] = {
    FormControlType::kInputText,
    FormControlType::kInputTelephone,
    FormControlType::kInputNumber,
};

class PhoneFieldParserTest : public testing::Test {
 public:
  PhoneFieldParserTest() = default;
  PhoneFieldParserTest(const PhoneFieldParserTest&) = delete;
  PhoneFieldParserTest& operator=(const PhoneFieldParserTest&) = delete;

 protected:
  // Downcast for tests.
  static std::unique_ptr<PhoneFieldParser> Parse(ParsingContext& context,
                                                 AutofillScanner* scanner) {
    // An empty page_language means the language is unknown and patterns of all
    // languages are used.
    std::unique_ptr<FormFieldParser> field =
        PhoneFieldParser::Parse(context, scanner);
    return std::unique_ptr<PhoneFieldParser>(
        static_cast<PhoneFieldParser*>(field.release()));
  }

  // Checks if the field with `id` was classified as `expected_type`.
  void CheckField(const FieldGlobalId id, FieldType expected_type) const;

  struct TestFieldData {
    FormControlType type;
    std::u16string label;
    std::u16string name;
    FieldType expected_type;
    // Rarely used fields. Placed at the end to simplify common use cases.
    uint64_t max_length = 0;
    // Options of a FormControlType::kSelectOne `type` element.
    std::vector<const char*> options;
  };

  // Creates a `FormFieldData` object with the provided properties, assigns a
  // unique renderer id and appends it to `list_`. The `FieldGlobalId` of the
  // object is returned.
  autofill::FieldGlobalId AppendField(const TestFieldData& field_data);

  // Clears the state, creates fields as specified in `fields` and tries to
  // parse it.
  // If `expect_success` is true, the `expected_type` of every field is
  // verified. Otherwise it is checked that nothing was parsed.
  // In case `expected_success` is false, `expected_types` can be omitted.
  void RunParsingTest(const std::vector<TestFieldData>& fields,
                      bool expect_success = true);

  FieldRendererId MakeFieldRendererId();

  void Clear();

  std::vector<std::unique_ptr<AutofillField>> list_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<PhoneFieldParser> field_;
  uint64_t id_counter_ = 0;
  FieldCandidatesMap field_candidates_map_;
};

void PhoneFieldParserTest::CheckField(const FieldGlobalId id,
                                      FieldType expected_type) const {
  auto it = field_candidates_map_.find(id);
  ASSERT_TRUE(it != field_candidates_map_.end());
  EXPECT_EQ(expected_type, it->second.BestHeuristicType());
}

autofill::FieldGlobalId PhoneFieldParserTest::AppendField(
    const TestFieldData& field_data) {
  FormFieldData field;
  field.set_form_control_type(field_data.type);
  field.set_label(field_data.label);
  field.set_name(field_data.name);
  field.set_max_length(field_data.max_length);
  std::vector<SelectOption> options;
  for (auto* const element : field_data.options) {
    options.push_back({.value = u"", .text = base::UTF8ToUTF16(element)});
  }
  field.set_options(std::move(options));
  field.set_renderer_id(MakeFieldRendererId());
  list_.push_back(std::make_unique<AutofillField>(field));
  return list_.back()->global_id();
}

void PhoneFieldParserTest::RunParsingTest(
    const std::vector<TestFieldData>& fields,
    bool expect_success) {
  Clear();

  // Construct all the test fields.
  std::vector<autofill::FieldGlobalId> global_ids;
  for (const TestFieldData& field : fields) {
    global_ids.push_back(AppendField(field));
  }

  // Parse.
  AutofillScanner scanner(list_);
  ParsingContext context(GeoIpCountryCode(""), LanguageCode(""),
                         *GetActivePatternFile());
  field_ = Parse(context, &scanner);
  ASSERT_EQ(expect_success, field_.get() != nullptr);

  // Verify expecations.
  if (expect_success) {
    field_->AddClassificationsForTesting(field_candidates_map_);
    for (size_t i = 0; i < fields.size(); i++) {
      CheckField(global_ids[i], fields[i].expected_type);
    }
  }
}

FieldRendererId PhoneFieldParserTest::MakeFieldRendererId() {
  return FieldRendererId(++id_counter_);
}

void PhoneFieldParserTest::Clear() {
  field_candidates_map_.clear();
  field_.reset();
  list_.clear();
}

TEST_F(PhoneFieldParserTest, Empty) {
  RunParsingTest({}, /*expect_success=*/false);
}

TEST_F(PhoneFieldParserTest, NonParse) {
  list_.push_back(std::make_unique<AutofillField>());
  RunParsingTest({}, /*expect_success=*/false);
}

TEST_F(PhoneFieldParserTest, ParseOneLinePhoneWithDefaultToCityAndNumber) {
  for (FormControlType field_type : kFieldTypes) {
    RunParsingTest(
        {{field_type, u"Phone", u"phone", PHONE_HOME_CITY_AND_NUMBER}});
  }

  for (FormControlType field_type : kFieldTypes) {
    RunParsingTest({{field_type, u"Phone (e.g. +49...)", u"phone",
                     PHONE_HOME_WHOLE_NUMBER}});
  }
}

TEST_F(PhoneFieldParserTest, ParseTwoLinePhone) {
  for (FormControlType field_type : kFieldTypes) {
    RunParsingTest(
        {{field_type, u"Area Code", u"area code", PHONE_HOME_CITY_CODE},
         {field_type, u"Phone", u"phone", PHONE_HOME_NUMBER}});
  }
}

// Phone in format <field> - <field> - <field> could be either
// <area code> - <prefix> - <suffix>, or
// <country code> - <area code> - <phone>. The only distinguishing feature is
// size: <prefix> is no bigger than 3 characters, and <suffix> is no bigger
// than 4.
TEST_F(PhoneFieldParserTest, ThreePartPhoneNumber) {
  for (FormControlType field_type : kFieldTypes) {
    RunParsingTest(
        {{field_type, u"Phone:", u"dayphone1", PHONE_HOME_CITY_CODE},
         {field_type, u"-", u"dayphone2", PHONE_HOME_NUMBER_PREFIX,
          /*max_length=*/3},
         {field_type, u"-", u"dayphone3", PHONE_HOME_NUMBER_SUFFIX,
          /*max_length=*/4},
         {field_type, u"ext.:", u"dayphone4", PHONE_HOME_EXTENSION}});
  }
}

// This scenario of explicitly labeled "prefix" and "suffix" phone numbers
// encountered in http://crbug.com/40694 with page
// https://www.wrapables.com/jsp/Signup.jsp.
TEST_F(PhoneFieldParserTest, ThreePartPhoneNumberPrefixSuffix) {
  for (FormControlType field_type : kFieldTypes) {
    RunParsingTest({{field_type, u"Phone:", u"area", PHONE_HOME_CITY_CODE},
                    {field_type, u"", u"prefix", PHONE_HOME_NUMBER_PREFIX},
                    {field_type, u"", u"suffix", PHONE_HOME_NUMBER_SUFFIX,
                     /*max_length=*/4}});
  }
}

// Phone in format <country code> - <city and number>
TEST_F(PhoneFieldParserTest, CountryAndCityAndPhoneNumber) {
  for (FormControlType field_type : kFieldTypes) {
    RunParsingTest({{field_type, u"Phone Number", u"CountryCode",
                     PHONE_HOME_COUNTRY_CODE, /*max_length=*/3},
                    {field_type, u"Phone Number", u"PhoneNumber",
                     PHONE_HOME_CITY_AND_NUMBER}});
  }
}

// Tests that when a phone field is parsed, a metric indicating the used grammar
// is emitted.
TEST_F(PhoneFieldParserTest, GrammarMetrics) {
  // PHONE_HOME_WHOLE_NUMBER corresponds to the last grammar, which is at index
  // 14 of the grammars array in PhoneFieldParser::GetPhoneGrammars. We thus
  // expect that 14 is logged.
  base::HistogramTester histogram_tester;
  RunParsingTest({{FormControlType::kInputText, u"Phone", u"phone",
                   PHONE_HOME_CITY_AND_NUMBER}});
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.FieldPrediction.PhoneNumberGrammarUsage2"),
              BucketsAre(base::Bucket(14, 1)));
}

// Tests if the country code, city code and phone number fields are correctly
// classified by the heuristic when the phone code is a select element.
TEST_F(PhoneFieldParserTest, CountryCodeIsSelectElement) {
  RunParsingTest({{FormControlType::kSelectOne, u"Phone Country Code", u"ccode",
                   PHONE_HOME_COUNTRY_CODE},
                  {FormControlType::kInputText, u"Phone City Code", u"areacode",
                   PHONE_HOME_CITY_CODE,
                   /*max_length=*/3},
                  {FormControlType::kInputText, u"Phone Number", u"phonenumber",
                   PHONE_HOME_NUMBER}});
}

// Tests if the country code, city code and phone number fields are correctly
// classified by the heuristic when the phone code field is a select element
// consisting of valid options.
TEST_F(PhoneFieldParserTest, CountryCodeWithOptions) {
  // Options consisting of the country code followed by the country names.
  std::vector<const char*> augmented_field_options_list = {
      "(+91) India",     "(+49) Germany",  "(+1) United States", "(+20) Egypt",
      "(+1242) Bahamas", "(+593) Ecuador", "(+7) Russia"};

  RunParsingTest({{FormControlType::kSelectOne, u"PC", u"PC",
                   PHONE_HOME_COUNTRY_CODE, 0, augmented_field_options_list},
                  {FormControlType::kInputText, u"Phone City Code", u"areacode",
                   PHONE_HOME_CITY_CODE},
                  {FormControlType::kInputText, u"Phone Number", u"phonenumber",
                   PHONE_HOME_NUMBER}});
}

// Tests if the country code field is correctly classified by the heuristic when
// the phone code is a select element and consists of valid options.
TEST_F(PhoneFieldParserTest, IsPhoneCountryCodeField) {
  std::vector<std::vector<const char*>> augmented_field_options_list = {
      // Options with the country name followed by the country code in brackets.
      {"India(+91) ", "Germany(+49)", "United States(+1)", "Egypt(+20)",
       "Bahamas(+1242)", "Ecuador(+593)", "Russia(+7)"},

      // Options consisting of the country code totaling more than 20.
      {"+91",   "+49",  "+1",   "+20",  "+1242", "+593", "+7",
       "+1441", "+211", "+212", "+30",  "+31",   "+32",  "+33",
       "+34",   "+51",  "52",   "+673", "+674",  "+81",  "+82"},

      // Options consisting of the country code totaling more than 20 with an
      // additional placeholder option.
      {"+91",   "+49",
       "+1",    "+20",
       "+1242", "+593",
       "+7",    "+1441",
       "+211",  "+212",
       "+30",   "+31",
       "+32",   "+33",
       "+34",   "+51",
       "52",    "+673",
       "+674",  "+81",
       "+82",   "Please select an option"},

      // Options with the country name followed by the country code in brackets
      // along with a placeholder option.
      {"Please select an option", "(+91) India", "(+49) Germany",
       "(+1) United States", "(+20) Egypt", "(+1242) Bahamas", "(+593) Ecuador",
       "(+7) Russia"},

      // Options with the phone country code followed by the country
      // abbreviation.
      {"91 IN", "49 DE", "1 US", "20 E", "1242 B", "593 EQ", "7 R"},

      // Options with the phone country code that are preceded by '00' and
      // followed by the country abbreviation.
      {"(0091) IN", "(0049) DE", "(001) US", "(0020) E", "(001242) B",
       "(00593) EQ", "(007) R"},

      // Options with the phone country code that are preceded by '00' and
      // followed by the country abbreviation with single space in between.
      {"(00 91) IN", "(00 49) DE", "(00 1) US", "(00 20) E", "(00 1242) B",
       "(00 593) EQ", "(00 7) R"},

      // Options with the phone country code preceded by '00' with multiple
      // spaces in between to align them.
      {"00  91", "00  49", "00   1", "00  20", "001242", "00 593", "00   7"},

      // Options with the phone country code preceded by '00'.
      {"0091", "0049", "001", "0020", "001242", "00593", "007"}};

  for (size_t i = 0; i < augmented_field_options_list.size(); ++i) {
    // TODO(crbug.com/40158319): The country code check fails in iteration 4.
    if (i == 4)
      continue;

    SCOPED_TRACE(testing::Message() << "i = " << i);
    RunParsingTest(
        {{FormControlType::kSelectOne, u"PC", u"PC", PHONE_HOME_COUNTRY_CODE, 0,
          augmented_field_options_list[i]},
         {FormControlType::kInputText, u"Phone Number", u"phonenumber",
          PHONE_HOME_CITY_AND_NUMBER}});
  }
}

// Tests that the month field is not classified as |PHONE_HOME_COUNTRY_CODE|.
TEST_F(PhoneFieldParserTest, IsMonthField) {
  std::vector<std::vector<const char*>> augmented_field_options_list = {
      // Month options in numeric.
      {"01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12"},

      // Month options in numeric followed by the respective text.
      {"(01) Jan", "(02) Feb", "(03) March", "(04) April", "(05) May",
       "(06) June", "(07) July", "(08) August", "(09) Sept", "(10) Oct",
       "(11) Nov", "(12) Dec"}};

  for (size_t i = 0; i < augmented_field_options_list.size(); ++i) {
    SCOPED_TRACE(testing::Message() << "i = " << i);
    // Expected types don't matter, as parsing is expected to be unsuccessful.
    RunParsingTest(
        {{FormControlType::kSelectOne, u"Month", u"Month", UNKNOWN_TYPE, 0,
          augmented_field_options_list[i]},
         {FormControlType::kInputText, u"Phone Number", u"phonenumber"}},
        /*expect_success=*/false);
  }
}

// Tests that the day field is not classified as |PHONE_HOME_COUNTRY_CODE|.
TEST_F(PhoneFieldParserTest, IsDayField) {
  std::vector<std::vector<const char*>> augmented_field_options_list = {
      // Numeric day options.
      {"01", "02", "03", "04", "05", "06", "07", "08", "09", "10",
       "11", "12", "13", "14", "15", "16", "17", "18", "19", "20",
       "21", "22", "23", "24", "25", "26", "27", "28", "29", "30"},

      // Numeric day options with a select option placeholder.
      {"Please select an option",
       "01",
       "02",
       "03",
       "04",
       "05",
       "06",
       "07",
       "08",
       "09",
       "10",
       "11",
       "12",
       "13",
       "14",
       "15",
       "16",
       "17",
       "18",
       "19",
       "20",
       "21",
       "22",
       "23",
       "24",
       "25",
       "26",
       "27",
       "28",
       "29",
       "30"}};

  for (size_t i = 0; i < augmented_field_options_list.size(); ++i) {
    SCOPED_TRACE(testing::Message() << "i = " << i);
    // Expected types don't matter, as parsing is expected to be unsuccessful.
    RunParsingTest(
        {{FormControlType::kSelectOne, u"Field", u"Field", UNKNOWN_TYPE, 0,
          augmented_field_options_list[i]},
         {FormControlType::kInputText, u"Phone Number", u"phonenumber"}},
        /*expect_success=*/false);
  }
}

// Tests that the field is not classified as |PHONE_HOME_COUNTRY_CODE|.
TEST_F(PhoneFieldParserTest, IsYearField) {
  std::vector<std::vector<const char*>> augmented_field_options_list = {
      // Numeric four digit year options.
      {"1990", "1991", "1992", "1993", "1994", "1995", "1996",
       "1997", "1998", "1999", "2000", "2001", "2002", "2003",
       "2004", "2005", "2006", "2007", "2008", "2009", "2010"},

      // Numeric four digit year options less than 10 in total.
      {"1990", "1991", "1992", "1993", "1994"},

      // Numeric four digit year options in decreasing order.
      {"2025", "2024", "2023", "2022", "2021", "2020"},

      // Numeric two digit year options.
      {"90", "91", "92", "93", "94", "95", "96", "97", "98", "99", "00", "01",
       "02", "03", "04", "05", "06"},

      // Numeric two digit year options along with an additional placeholder
      // option.
      {"Please select an option", "90", "91", "92", "93", "94", "95", "96",
       "97", "98", "99", "00", "01", "02", "03", "04", "05", "06"},

      // Numeric two digit year options along with an additional placeholder
      // option less than 10 in total.
      {"Please select an option", "90", "91", "92", "93", "94"}};

  for (size_t i = 0; i < augmented_field_options_list.size(); ++i) {
    SCOPED_TRACE(testing::Message() << "i = " << i);
    // Expected types don't matter, as parsing is expected to be unsuccessful.
    RunParsingTest(
        {{FormControlType::kSelectOne, u"Field", u"Field", UNKNOWN_TYPE, 0,
          augmented_field_options_list[i]},
         {FormControlType::kInputText, u"Phone Number", u"phonenumber"}},
        /*expect_success=*/false);
  }
}

// Tests that the timezone field is not classified as |PHONE_HOME_COUNTRY_CODE|.
TEST_F(PhoneFieldParserTest, IsTimeZoneField) {
  std::vector<std::vector<const char*>> augmented_field_options_list = {
      // Time Zone options.
      {"Yemen (UTC+03:00)", "Uruguay (UTC−03:00)", "UAE (UTC+04:00)",
       "Uganda (UTC+03:00)", "Turkey (UTC+03:00)", "Taiwan (UTC+08:00)",
       "Sweden (UTC+01:00)"},

      // Time Zone options with a placeholder select element.
      {"Please select an option", "Yemen (UTC+03:00)", "Uruguay (UTC−03:00)",
       "UAE (UTC+04:00)", "Uganda (UTC+03:00)", "Turkey (UTC+03:00)",
       "Taiwan (UTC+08:00)", "Sweden (UTC+01:00)"}};

  for (size_t i = 0; i < augmented_field_options_list.size(); ++i) {
    SCOPED_TRACE(testing::Message() << "i = " << i);
    // Expected types don't matter, as parsing is expected to be unsuccessful.
    RunParsingTest(
        {{FormControlType::kSelectOne, u"Time Zone", u"TimeZone", UNKNOWN_TYPE,
          0, augmented_field_options_list[i]},
         {FormControlType::kInputText, u"Phone Number", u"phonenumber"}},
        /*expect_success=*/false);
  }
}

}  // namespace
}  // namespace autofill
