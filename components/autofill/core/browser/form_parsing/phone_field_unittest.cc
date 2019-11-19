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
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
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
  PhoneFieldTest() {}

 protected:
  // Downcast for tests.
  static std::unique_ptr<PhoneField> Parse(AutofillScanner* scanner) {
    std::unique_ptr<FormField> field = PhoneField::Parse(scanner, nullptr);
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

  std::vector<std::unique_ptr<AutofillField>> list_;
  std::unique_ptr<PhoneField> field_;
  FieldCandidatesMap field_candidates_map_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PhoneFieldTest);
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
    field_->AddClassifications(&field_candidates_map_);
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
    field_->AddClassifications(&field_candidates_map_);
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
    field_->AddClassifications(&field_candidates_map_);
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
    field_->AddClassifications(&field_candidates_map_);
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
    field_->AddClassifications(&field_candidates_map_);
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
    field_->AddClassifications(&field_candidates_map_);
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
    field_->AddClassifications(&field_candidates_map_);
    CheckField("country", PHONE_HOME_COUNTRY_CODE);
    CheckField("phone", PHONE_HOME_CITY_AND_NUMBER);
  }
}

}  // namespace autofill
