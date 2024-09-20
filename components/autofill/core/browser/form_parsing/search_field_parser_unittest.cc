// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/search_field_parser.h"

#include <memory>

#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/common/form_data.h"

namespace autofill {

class SearchFieldParserTest : public FormFieldParserTestBase,
                              public testing::Test {
 public:
  SearchFieldParserTest() = default;
  SearchFieldParserTest(const SearchFieldParserTest&) = delete;
  SearchFieldParserTest& operator=(const SearchFieldParserTest&) = delete;

 protected:
  std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                         AutofillScanner* scanner) override {
    return SearchFieldParser::Parse(context, scanner);
  }
};

TEST_F(SearchFieldParserTest, ParseSearchTerm) {
  AddTextFormFieldData("search", "Search", SEARCH_TERM);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(SearchFieldParserTest, ParseNonSearchTerm) {
  AddTextFormFieldData("address", "Address", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::kNotParsed);
}

}  // namespace autofill
