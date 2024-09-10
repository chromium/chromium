// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/search_field_parser.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

// static
std::unique_ptr<FormFieldParser> SearchFieldParser::Parse(
    ParsingContext& context,
    AutofillScanner* scanner) {
  raw_ptr<AutofillField> field;
  base::span<const MatchPatternRef> patterns = GetMatchPatterns(
      SEARCH_TERM, context.page_language, context.pattern_file);
  if (ParseField(context, scanner, patterns, &field, "SEARCH_TERM")) {
    return std::make_unique<SearchFieldParser>(field);
  }
  return nullptr;
}

SearchFieldParser::SearchFieldParser(const AutofillField* field)
    : field_(field) {}

void SearchFieldParser::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(field_, SEARCH_TERM, kBaseSearchParserScore,
                    field_candidates);
}

}  // namespace autofill
