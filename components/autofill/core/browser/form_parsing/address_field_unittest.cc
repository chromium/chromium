// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/address_field.h"

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

class AddressFieldTest : public testing::Test {
 public:
  AddressFieldTest() {}

 protected:
  std::vector<std::unique_ptr<AutofillField>> list_;
  std::unique_ptr<AddressField> field_;
  FieldCandidatesMap field_candidates_map_;

  // Downcast for tests.
  static std::unique_ptr<AddressField> Parse(AutofillScanner* scanner) {
    std::unique_ptr<FormField> field = AddressField::Parse(scanner, nullptr);
    return std::unique_ptr<AddressField>(
        static_cast<AddressField*>(field.release()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AddressFieldTest);
};

TEST_F(AddressFieldTest, Empty) {
  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_EQ(nullptr, field_.get());
}

TEST_F(AddressFieldTest, NonParse) {
  list_.push_back(std::make_unique<AutofillField>());
  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_EQ(nullptr, field_.get());
}

TEST_F(AddressFieldTest, ParseOneLineAddress) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("addr1")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassifications(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("addr1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE1,
            field_candidates_map_[ASCIIToUTF16("addr1")].BestHeuristicType());
}

TEST_F(AddressFieldTest, ParseTwoLineAddress) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("addr1")));

  field.label = base::string16();
  field.name = ASCIIToUTF16("address2");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("addr2")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassifications(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("addr1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE1,
            field_candidates_map_[ASCIIToUTF16("addr1")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("addr2")) !=
              field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE2,
            field_candidates_map_[ASCIIToUTF16("addr2")].BestHeuristicType());
}

TEST_F(AddressFieldTest, ParseThreeLineAddress) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Address Line1");
  field.name = ASCIIToUTF16("Address1");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("addr1")));

  field.label = ASCIIToUTF16("Address Line2");
  field.name = ASCIIToUTF16("Address2");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("addr2")));

  field.label = ASCIIToUTF16("Address Line3");
  field.name = ASCIIToUTF16("Address3");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("addr3")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassifications(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("addr1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE1,
            field_candidates_map_[ASCIIToUTF16("addr1")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("addr2")) !=
              field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE2,
            field_candidates_map_[ASCIIToUTF16("addr2")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("addr3")) !=
              field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_LINE3,
            field_candidates_map_[ASCIIToUTF16("addr3")].BestHeuristicType());
}

TEST_F(AddressFieldTest, ParseStreetAddressFromTextArea) {
  FormFieldData field;
  field.form_control_type = "textarea";

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  list_.push_back(std::make_unique<AutofillField>(field, ASCIIToUTF16("addr")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassifications(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("addr")) !=
              field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_STREET_ADDRESS,
            field_candidates_map_[ASCIIToUTF16("addr")].BestHeuristicType());
}

TEST_F(AddressFieldTest, ParseCity) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("city1")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassifications(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("city1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_CITY,
            field_candidates_map_[ASCIIToUTF16("city1")].BestHeuristicType());
}

TEST_F(AddressFieldTest, ParseState) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("State");
  field.name = ASCIIToUTF16("state");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("state1")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassifications(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("state1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_STATE,
            field_candidates_map_[ASCIIToUTF16("state1")].BestHeuristicType());
}

TEST_F(AddressFieldTest, ParseZip) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Zip");
  field.name = ASCIIToUTF16("zip");
  list_.push_back(std::make_unique<AutofillField>(field, ASCIIToUTF16("zip1")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassifications(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("zip1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_ZIP,
            field_candidates_map_[ASCIIToUTF16("zip1")].BestHeuristicType());
}

TEST_F(AddressFieldTest, ParseStateAndZipOneLabel) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("State/Province, Zip/Postal Code");
  field.name = ASCIIToUTF16("state");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("state")));

  field.label = ASCIIToUTF16("State/Province, Zip/Postal Code");
  field.name = ASCIIToUTF16("zip");
  list_.push_back(std::make_unique<AutofillField>(field, ASCIIToUTF16("zip")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassifications(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("state")) !=
              field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_STATE,
            field_candidates_map_[ASCIIToUTF16("state")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("zip")) !=
              field_candidates_map_.end());
  EXPECT_EQ(ADDRESS_HOME_ZIP,
            field_candidates_map_[ASCIIToUTF16("zip")].BestHeuristicType());
}

TEST_F(AddressFieldTest, ParseCountry) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("country1")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassifications(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("country1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(
      ADDRESS_HOME_COUNTRY,
      field_candidates_map_[ASCIIToUTF16("country1")].BestHeuristicType());
}

TEST_F(AddressFieldTest, ParseCompany) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Company");
  field.name = ASCIIToUTF16("company");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("company1")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassifications(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("company1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(
      COMPANY_NAME,
      field_candidates_map_[ASCIIToUTF16("company1")].BestHeuristicType());
}

}  // namespace autofill
