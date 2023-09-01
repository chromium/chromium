// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/search_field.h"

#include <memory>

#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/common/form_data.h"

namespace autofill {

class SearchFieldTest
    : public FormFieldTestBase,
      public testing::TestWithParam<PatternProviderFeatureState> {
 public:
  SearchFieldTest() : FormFieldTestBase(GetParam()) {}
  SearchFieldTest(const SearchFieldTest&) = delete;
  SearchFieldTest& operator=(const SearchFieldTest&) = delete;

 protected:
  std::unique_ptr<FormField> Parse(
      AutofillScanner* scanner,
      const LanguageCode& page_language = LanguageCode("en")) override {
    return SearchField::Parse(scanner, page_language, *GetActivePatternSource(),
                              /*log_manager=*/nullptr);
  }
};

INSTANTIATE_TEST_SUITE_P(
    SearchFieldTest,
    SearchFieldTest,
    ::testing::ValuesIn(PatternProviderFeatureState::All()));

TEST_P(SearchFieldTest, ParseSearchTerm) {
  AddTextFormFieldData("search", "Search", SEARCH_TERM);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_P(SearchFieldTest, ParseNonSearchTerm) {
  AddTextFormFieldData("address", "Address", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

}  // namespace autofill
