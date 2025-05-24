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
  std::optional<FieldAndMatchInfo> match;
  if (ParseField(context, scanner, "SEARCH_TERM", &match)) {
    return std::make_unique<SearchFieldParser>(std::move(*match));
  }
  return nullptr;
}

SearchFieldParser::SearchFieldParser(FieldAndMatchInfo match)
    : match_(std::move(match)) {}

void SearchFieldParser::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(match_, SEARCH_TERM, kBaseSearchParserScore,
                    field_candidates);
}

}  // namespace autofill
