// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/search_field.h"

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

class SearchFieldTest : public testing::Test {
 public:
  SearchFieldTest() {}

 protected:
  std::vector<std::unique_ptr<AutofillField>> list_;
  std::unique_ptr<SearchField> field_;
  FieldCandidatesMap field_candidates_map_;

  // Downcast for tests.
  static std::unique_ptr<SearchField> Parse(AutofillScanner* scanner) {
    std::unique_ptr<FormField> field = SearchField::Parse(scanner, nullptr);
    return std::unique_ptr<SearchField>(
        static_cast<SearchField*>(field.release()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchFieldTest);
};

TEST_F(SearchFieldTest, ParseSearchTerm) {
  FormFieldData search_field;
  search_field.form_control_type = "text";

  search_field.label = ASCIIToUTF16("Search");
  search_field.name = ASCIIToUTF16("search");

  list_.push_back(
      std::make_unique<AutofillField>(search_field, ASCIIToUTF16("search1")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassifications(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("search1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(SEARCH_TERM,
            field_candidates_map_[ASCIIToUTF16("search1")].BestHeuristicType());
}

TEST_F(SearchFieldTest, ParseNonSearchTerm) {
  FormFieldData address_field;
  address_field.form_control_type = "text";

  address_field.label = ASCIIToUTF16("Address");
  address_field.name = ASCIIToUTF16("address");

  list_.push_back(
      std::make_unique<AutofillField>(address_field, ASCIIToUTF16("address")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_EQ(nullptr, field_.get());
}

}  // namespace autofill
