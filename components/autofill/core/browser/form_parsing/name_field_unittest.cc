// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/name_field.h"

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

class NameFieldTest : public testing::Test {
 public:
  NameFieldTest() {}

 protected:
  std::vector<std::unique_ptr<AutofillField>> list_;
  std::unique_ptr<NameField> field_;
  FieldCandidatesMap field_candidates_map_;

  // Downcast for tests.
  static std::unique_ptr<NameField> Parse(AutofillScanner* scanner) {
    std::unique_ptr<FormField> field = NameField::Parse(scanner, nullptr);
    return std::unique_ptr<NameField>(static_cast<NameField*>(field.release()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NameFieldTest);
};

TEST_F(NameFieldTest, FirstMiddleLast) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("First");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("Middle Name");
  field.name = ASCIIToUTF16("Middle");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name2")));

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("Last");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name3")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassifications(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_FIRST,
            field_candidates_map_[ASCIIToUTF16("name1")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name2")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_MIDDLE,
            field_candidates_map_[ASCIIToUTF16("name2")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name3")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_LAST,
            field_candidates_map_[ASCIIToUTF16("name3")].BestHeuristicType());
}

TEST_F(NameFieldTest, FirstMiddleLast2) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = base::string16();
  field.name = ASCIIToUTF16("firstName");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name1")));

  field.label = base::string16();
  field.name = ASCIIToUTF16("middleName");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name2")));

  field.label = base::string16();
  field.name = ASCIIToUTF16("lastName");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name3")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassifications(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_FIRST,
            field_candidates_map_[ASCIIToUTF16("name1")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name2")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_MIDDLE,
            field_candidates_map_[ASCIIToUTF16("name2")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name3")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_LAST,
            field_candidates_map_[ASCIIToUTF16("name3")].BestHeuristicType());
}

TEST_F(NameFieldTest, FirstLast) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = base::string16();
  field.name = ASCIIToUTF16("first_name");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name1")));

  field.label = base::string16();
  field.name = ASCIIToUTF16("last_name");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name2")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassifications(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_FIRST,
            field_candidates_map_[ASCIIToUTF16("name1")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name2")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_LAST,
            field_candidates_map_[ASCIIToUTF16("name2")].BestHeuristicType());
}

TEST_F(NameFieldTest, FirstLast2) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name");
  field.name = ASCIIToUTF16("first_name");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("Name");
  field.name = ASCIIToUTF16("last_name");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name2")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassifications(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_FIRST,
            field_candidates_map_[ASCIIToUTF16("name1")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name2")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_LAST,
            field_candidates_map_[ASCIIToUTF16("name2")].BestHeuristicType());
}

TEST_F(NameFieldTest, FirstLastMiddleWithSpaces) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First  Name");
  field.name = ASCIIToUTF16("first_name");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("Middle  Name");
  field.name = ASCIIToUTF16("middle_name");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name2")));

  field.label = ASCIIToUTF16("Last  Name");
  field.name = ASCIIToUTF16("last_name");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name3")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassifications(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_FIRST,
            field_candidates_map_[ASCIIToUTF16("name1")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name2")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_MIDDLE,
            field_candidates_map_[ASCIIToUTF16("name2")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name3")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_LAST,
            field_candidates_map_[ASCIIToUTF16("name3")].BestHeuristicType());
}

TEST_F(NameFieldTest, FirstLastEmpty) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name");
  field.name = ASCIIToUTF16("first_name");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name1")));

  field.label = base::string16();
  field.name = ASCIIToUTF16("last_name");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name2")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassifications(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_FIRST,
            field_candidates_map_[ASCIIToUTF16("name1")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name2")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_LAST,
            field_candidates_map_[ASCIIToUTF16("name2")].BestHeuristicType());
}

TEST_F(NameFieldTest, FirstMiddleLastEmpty) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name");
  field.name = ASCIIToUTF16("first_name");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name1")));

  field.label = base::string16();
  field.name = ASCIIToUTF16("middle_name");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name2")));

  field.label = base::string16();
  field.name = ASCIIToUTF16("last_name");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name3")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassifications(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_FIRST,
            field_candidates_map_[ASCIIToUTF16("name1")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name2")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_MIDDLE_INITIAL,
            field_candidates_map_[ASCIIToUTF16("name2")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name3")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_LAST,
            field_candidates_map_[ASCIIToUTF16("name3")].BestHeuristicType());
}

TEST_F(NameFieldTest, MiddleInitial) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("first_name");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("MI");
  field.name = ASCIIToUTF16("middle_name");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name2")));

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("last_name");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name3")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassifications(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_FIRST,
            field_candidates_map_[ASCIIToUTF16("name1")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name2")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_MIDDLE_INITIAL,
            field_candidates_map_[ASCIIToUTF16("name2")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name3")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_LAST,
            field_candidates_map_[ASCIIToUTF16("name3")].BestHeuristicType());
}

TEST_F(NameFieldTest, MiddleInitialNoLastName) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("first_name");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("MI");
  field.name = ASCIIToUTF16("middle_name");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name2")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_EQ(nullptr, field_.get());
}

// This case is from the dell.com checkout page.  The middle initial "mi" string
// came at the end following other descriptive text.  http://crbug.com/45123.
TEST_F(NameFieldTest, MiddleInitialAtEnd) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = base::string16();
  field.name = ASCIIToUTF16("XXXnameXXXfirst");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name1")));

  field.label = base::string16();
  field.name = ASCIIToUTF16("XXXnameXXXmi");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name2")));

  field.label = base::string16();
  field.name = ASCIIToUTF16("XXXnameXXXlast");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name3")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassifications(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_FIRST,
            field_candidates_map_[ASCIIToUTF16("name1")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name2")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_MIDDLE_INITIAL,
            field_candidates_map_[ASCIIToUTF16("name2")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name3")) !=
              field_candidates_map_.end());
  EXPECT_EQ(NAME_LAST,
            field_candidates_map_[ASCIIToUTF16("name3")].BestHeuristicType());
}

}  // namespace autofill
