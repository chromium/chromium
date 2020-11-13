// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/phone_field.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

namespace {

const char* const kFieldTypes[] = {
    "text",
    "tel",
    "number",
};

}  // namespace

class PhoneFieldTest : public testing::Test {
 public:
  PhoneFieldTest() = default;
  PhoneFieldTest(const PhoneFieldTest&) = delete;
  PhoneFieldTest& operator=(const PhoneFieldTest&) = delete;

 protected:
  // Downcast for tests.
  static std::unique_ptr<PhoneField> Parse(AutofillScanner* scanner) {
    // An empty page_language means the language is unknown and patterns of all
    // languages are used.
    std::unique_ptr<FormField> field =
        PhoneField::Parse(scanner, /*page_language=*/"", nullptr);
    return std::unique_ptr<PhoneField>(
        static_cast<PhoneField*>(field.release()));
  }

  void Clear() {
    list_.clear();
    field_.reset();
    field_candidates_map_.clear();
  }

  void CheckField(const std::string& name,
                  ServerFieldType expected_type) const {
    auto it = field_candidates_map_.find(ASCIIToUTF16(name));
    ASSERT_TRUE(it != field_candidates_map_.end()) << name;
    EXPECT_EQ(expected_type, it->second.BestHeuristicType()) << name;
  }

  // Populates a select |field| with the |label|, the |name| and the |contents|.
  void CreateTestSelectField(const char* label,
                             const char* name,
                             const std::vector<const char*>& contents,
                             FormFieldData* field) {
    field->label = ASCIIToUTF16(label);
    field->name = ASCIIToUTF16(name);
    field->form_control_type = "select-one";

    std::vector<base::string16> contents16;
    for (auto* const element : contents)
      contents16.push_back(base::UTF8ToUTF16(element));

    field->option_contents = contents16;
  }

  std::vector<std::unique_ptr<AutofillField>> list_;
  std::unique_ptr<PhoneField> field_;
  FieldCandidatesMap field_candidates_map_;
};

TEST_F(PhoneFieldTest, Empty) {
  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_EQ(nullptr, field_.get());
}

TEST_F(PhoneFieldTest, NonParse) {
  list_.push_back(std::make_unique<AutofillField>());
  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_EQ(nullptr, field_.get());
}

TEST_F(PhoneFieldTest, ParseOneLinePhone) {
  FormFieldData field;

  for (const char* field_type : kFieldTypes) {
    Clear();

    field.form_control_type = field_type;
    field.label = ASCIIToUTF16("Phone");
    field.name = ASCIIToUTF16("phone");
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("phone1")));

    AutofillScanner scanner(list_);
    field_ = Parse(&scanner);
    ASSERT_NE(nullptr, field_.get());
    field_->AddClassificationsForTesting(&field_candidates_map_);
    CheckField("phone1", PHONE_HOME_WHOLE_NUMBER);
  }
}

TEST_F(PhoneFieldTest, ParseTwoLinePhone) {
  FormFieldData field;

  for (const char* field_type : kFieldTypes) {
    Clear();

    field.form_control_type = field_type;
    field.label = ASCIIToUTF16("Area Code");
    field.name = ASCIIToUTF16("area code");
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("areacode1")));

    field.label = ASCIIToUTF16("Phone");
    field.name = ASCIIToUTF16("phone");
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("phone2")));

    AutofillScanner scanner(list_);
    field_ = Parse(&scanner);
    ASSERT_NE(nullptr, field_.get());
    field_->AddClassificationsForTesting(&field_candidates_map_);
    CheckField("areacode1", PHONE_HOME_CITY_CODE);
    CheckField("phone2", PHONE_HOME_NUMBER);
  }
}

TEST_F(PhoneFieldTest, ThreePartPhoneNumber) {
  // Phone in format <field> - <field> - <field> could be either
  // <area code> - <prefix> - <suffix>, or
  // <country code> - <area code> - <phone>. The only distinguishing feature is
  // size: <prefix> is no bigger than 3 characters, and <suffix> is no bigger
  // than 4.
  FormFieldData field;

  for (const char* field_type : kFieldTypes) {
    Clear();

    field.form_control_type = field_type;
    field.label = ASCIIToUTF16("Phone:");
    field.name = ASCIIToUTF16("dayphone1");
    field.max_length = 0;
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("areacode1")));

    field.label = ASCIIToUTF16("-");
    field.name = ASCIIToUTF16("dayphone2");
    field.max_length = 3;
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("prefix2")));

    field.label = ASCIIToUTF16("-");
    field.name = ASCIIToUTF16("dayphone3");
    field.max_length = 4;
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("suffix3")));

    field.label = ASCIIToUTF16("ext.:");
    field.name = ASCIIToUTF16("dayphone4");
    field.max_length = 0;
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("ext4")));

    AutofillScanner scanner(list_);
    field_ = Parse(&scanner);
    ASSERT_NE(nullptr, field_.get());
    field_->AddClassificationsForTesting(&field_candidates_map_);
    CheckField("areacode1", PHONE_HOME_CITY_CODE);
    CheckField("prefix2", PHONE_HOME_NUMBER);
    CheckField("suffix3", PHONE_HOME_NUMBER);
    EXPECT_TRUE(base::Contains(field_candidates_map_, ASCIIToUTF16("ext4")));
  }
}

// This scenario of explicitly labeled "prefix" and "suffix" phone numbers
// encountered in http://crbug.com/40694 with page
// https://www.wrapables.com/jsp/Signup.jsp.
TEST_F(PhoneFieldTest, ThreePartPhoneNumberPrefixSuffix) {
  FormFieldData field;

  for (const char* field_type : kFieldTypes) {
    Clear();

    field.form_control_type = field_type;
    field.label = ASCIIToUTF16("Phone:");
    field.name = ASCIIToUTF16("area");
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("areacode1")));

    field.label = base::string16();
    field.name = ASCIIToUTF16("prefix");
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("prefix2")));

    field.label = base::string16();
    field.name = ASCIIToUTF16("suffix");
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("suffix3")));

    AutofillScanner scanner(list_);
    field_ = Parse(&scanner);
    ASSERT_NE(nullptr, field_.get());
    field_->AddClassificationsForTesting(&field_candidates_map_);
    CheckField("areacode1", PHONE_HOME_CITY_CODE);
    CheckField("prefix2", PHONE_HOME_NUMBER);
    CheckField("suffix3", PHONE_HOME_NUMBER);
  }
}

TEST_F(PhoneFieldTest, ThreePartPhoneNumberPrefixSuffix2) {
  FormFieldData field;

  for (const char* field_type : kFieldTypes) {
    Clear();

    field.form_control_type = field_type;
    field.label = ASCIIToUTF16("(");
    field.name = ASCIIToUTF16("phone1");
    field.max_length = 3;
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("phone1")));

    field.label = ASCIIToUTF16(")");
    field.name = ASCIIToUTF16("phone2");
    field.max_length = 3;
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("phone2")));

    field.label = base::string16();
    field.name = ASCIIToUTF16("phone3");
    field.max_length = 4;
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("phone3")));

    AutofillScanner scanner(list_);
    field_ = Parse(&scanner);
    ASSERT_NE(nullptr, field_.get());
    field_->AddClassificationsForTesting(&field_candidates_map_);
    CheckField("phone1", PHONE_HOME_CITY_CODE);
    CheckField("phone2", PHONE_HOME_NUMBER);
    CheckField("phone3", PHONE_HOME_NUMBER);
  }
}

TEST_F(PhoneFieldTest, CountryAndCityAndPhoneNumber) {
  // Phone in format <country code>:3 - <city and number>:10
  // The |maxlength| is considered, otherwise it's too broad.
  FormFieldData field;

  for (const char* field_type : kFieldTypes) {
    Clear();

    field.form_control_type = field_type;
    field.label = ASCIIToUTF16("Phone Number");
    field.name = ASCIIToUTF16("CountryCode");
    field.max_length = 3;
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("country")));

    field.label = ASCIIToUTF16("Phone Number");
    field.name = ASCIIToUTF16("PhoneNumber");
    field.max_length = 10;
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("phone")));

    AutofillScanner scanner(list_);
    field_ = Parse(&scanner);
    ASSERT_NE(nullptr, field_.get());
    field_->AddClassificationsForTesting(&field_candidates_map_);
    CheckField("country", PHONE_HOME_COUNTRY_CODE);
    CheckField("phone", PHONE_HOME_CITY_AND_NUMBER);
  }
}

TEST_F(PhoneFieldTest, CountryAndCityAndPhoneNumberWithLongerMaxLength) {
  // Phone in format <country code>:3 - <city and number>:14
  // The |maxlength| is considered, otherwise it's too broad.
  FormFieldData field;

  for (const char* field_type : kFieldTypes) {
    Clear();

    field.form_control_type = field_type;
    field.label = ASCIIToUTF16("Phone Number");
    field.name = ASCIIToUTF16("CountryCode");
    field.max_length = 3;
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("country")));

    // Verify if websites expect a longer formatted number like:
    // (514)-123-1234, autofill is able to classify correctly.
    field.label = ASCIIToUTF16("Phone Number");
    field.name = ASCIIToUTF16("PhoneNumber");
    field.max_length = 14;
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("phone")));

    AutofillScanner scanner(list_);
    field_ = Parse(&scanner);
    ASSERT_NE(nullptr, field_.get());
    field_->AddClassificationsForTesting(&field_candidates_map_);
    CheckField("country", PHONE_HOME_COUNTRY_CODE);
    CheckField("phone", PHONE_HOME_CITY_AND_NUMBER);
  }
}

// Tests if the country code, city code and phone number fields are correctly
// classified by the heuristic when the phone code is a select element.
TEST_F(PhoneFieldTest, CountryCodeIsSelectElement) {
  FormFieldData field;

  field.label = ASCIIToUTF16("Phone Country Code");
  field.name = ASCIIToUTF16("ccode");
  field.form_control_type = "select-one";
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("countryCode")));

  field.label = ASCIIToUTF16("Phone City Code");
  field.name = ASCIIToUTF16("areacode");
  field.form_control_type = "text";
  field.max_length = 3;
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("cityCode")));

  field.label = ASCIIToUTF16("Phone Number");
  field.name = ASCIIToUTF16("phonenumber");
  field.max_length = 0;
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("phoneNumber")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassificationsForTesting(&field_candidates_map_);
  CheckField("countryCode", PHONE_HOME_COUNTRY_CODE);
  CheckField("cityCode", PHONE_HOME_CITY_CODE);
  CheckField("phoneNumber", PHONE_HOME_NUMBER);
}

// Tests if the country code, city code and phone number fields are correctly
// classified by the heuristic when the phone code field is a select element
// consisting of valid options.
TEST_F(PhoneFieldTest, CountryCodeWithOptions) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableAugmentedPhoneCountryCode);

  FormFieldData field;

  // Options consisting of the country code followed by the country names.
  std::vector<const char*> augmented_field_options_list = {
      "(+91) India",     "(+49) Germany",  "(+1) United States", "(+20) Egypt",
      "(+1242) Bahamas", "(+593) Ecuador", "(+7) Russia"};
  CreateTestSelectField("PC", "PC", augmented_field_options_list, &field);
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("countryCode")));

  field.label = ASCIIToUTF16("Phone City Code");
  field.name = ASCIIToUTF16("areacode");
  field.form_control_type = "text";
  field.max_length = 3;
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("cityCode")));

  field.label = ASCIIToUTF16("Phone Number");
  field.name = ASCIIToUTF16("phonenumber");
  field.max_length = 0;
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("phoneNumber")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassificationsForTesting(&field_candidates_map_);
  CheckField("countryCode", PHONE_HOME_COUNTRY_CODE);
  CheckField("cityCode", PHONE_HOME_CITY_CODE);
  CheckField("phoneNumber", PHONE_HOME_NUMBER);
}

// Tests if the country code field is correctly classified by the heuristic when
// the phone code is a select element and consists of valid options.
TEST_F(PhoneFieldTest, IsPhoneCountryCodeField) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableAugmentedPhoneCountryCode);

  FormFieldData field;
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
    SCOPED_TRACE(testing::Message() << "i = " << i);
    const auto& options_list = augmented_field_options_list[i];
    CreateTestSelectField("PC", "PC", options_list, &field);
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("countryCode")));

    field.label = ASCIIToUTF16("Phone Number");
    field.name = ASCIIToUTF16("phonenumber");
    field.max_length = 14;
    field.form_control_type = "text";
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("phoneNumber")));

    AutofillScanner scanner(list_);
    field_ = Parse(&scanner);
    ASSERT_NE(nullptr, field_.get());
    field_->AddClassificationsForTesting(&field_candidates_map_);
    CheckField("countryCode", PHONE_HOME_COUNTRY_CODE);
  }
}  // namespace autofill

// Tests that the month field is not classified as |PHONE_HOME_COUNTRY_CODE|.
TEST_F(PhoneFieldTest, IsMonthField) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableAugmentedPhoneCountryCode);

  FormFieldData field;
  std::vector<std::vector<const char*>> augmented_field_options_list = {
      // Month options in numeric.
      {"01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12"},

      // Month options in numeric followed by the respective text.
      {"(01) Jan", "(02) Feb", "(03) March", "(04) April", "(05) May",
       "(06) June", "(07) July", "(08) August", "(09) Sept", "(10) Oct",
       "(11) Nov", "(12) Dec"}};

  for (size_t i = 0; i < augmented_field_options_list.size(); ++i) {
    SCOPED_TRACE(testing::Message() << "i = " << i);
    const auto& options_list = augmented_field_options_list[i];
    CreateTestSelectField("Month", "Month", options_list, &field);
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("months")));

    field.label = ASCIIToUTF16("Phone Number");
    field.name = ASCIIToUTF16("phonenumber");
    field.max_length = 14;
    field.form_control_type = "text";
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("phoneNumber")));

    AutofillScanner scanner(list_);
    field_ = Parse(&scanner);
    ASSERT_EQ(nullptr, field_.get());
  }
}

// Tests that the day field is not classified as |PHONE_HOME_COUNTRY_CODE|.
TEST_F(PhoneFieldTest, IsDayField) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableAugmentedPhoneCountryCode);

  FormFieldData field;
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
    const auto& options_list = augmented_field_options_list[i];
    CreateTestSelectField("Field", "Field", options_list, &field);
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("day")));

    field.label = ASCIIToUTF16("Phone Number");
    field.name = ASCIIToUTF16("phonenumber");
    field.max_length = 14;
    field.form_control_type = "text";
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("phoneNumber")));

    AutofillScanner scanner(list_);
    field_ = Parse(&scanner);
    ASSERT_EQ(nullptr, field_.get());
  }
}

// Tests that the field is not classified as |PHONE_HOME_COUNTRY_CODE|.
TEST_F(PhoneFieldTest, IsYearField) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableAugmentedPhoneCountryCode);

  FormFieldData field;
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
    const auto& options_list = augmented_field_options_list[i];
    CreateTestSelectField("Field", "Field", options_list, &field);
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("year")));

    field.label = ASCIIToUTF16("Phone Number");
    field.name = ASCIIToUTF16("phonenumber");
    field.max_length = 14;
    field.form_control_type = "text";
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("phoneNumber")));

    AutofillScanner scanner(list_);
    field_ = Parse(&scanner);
    ASSERT_EQ(nullptr, field_.get());
  }
}

// Tests that the timezone field is not classified as |PHONE_HOME_COUNTRY_CODE|.
TEST_F(PhoneFieldTest, IsTimeZoneField) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(
      features::kAutofillEnableAugmentedPhoneCountryCode);

  FormFieldData field;
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
    const auto& options_list = augmented_field_options_list[i];
    CreateTestSelectField("Time Zone", "TimeZone", options_list, &field);
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("timeZone")));

    field.label = ASCIIToUTF16("Phone Number");
    field.name = ASCIIToUTF16("phonenumber");
    field.max_length = 14;
    field.form_control_type = "text";
    list_.push_back(
        std::make_unique<AutofillField>(field, ASCIIToUTF16("phoneNumber")));

    AutofillScanner scanner(list_);
    field_ = Parse(&scanner);
    ASSERT_EQ(nullptr, field_.get());
  }
}

}  // namespace autofill
