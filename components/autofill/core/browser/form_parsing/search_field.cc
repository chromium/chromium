// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/search_field.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

// static
std::unique_ptr<FormField> SearchField::Parse(
    AutofillScanner* scanner,
    const GeoIpCountryCode& client_country,
    const LanguageCode& page_language,
    PatternSource pattern_source,
    LogManager* log_manager) {
  raw_ptr<AutofillField> field;
  base::span<const MatchPatternRef> patterns =
      GetMatchPatterns(SEARCH_TERM, page_language, pattern_source);

  if (ParseFieldSpecifics(scanner, kSearchTermRe,
                          kDefaultMatchParamsWith<MatchFieldType::kSearch,
                                                  MatchFieldType::kTextArea>,
                          patterns, &field, {log_manager, "kSearchTermRe"})) {
    return std::make_unique<SearchField>(field);
  }

  return nullptr;
}

SearchField::SearchField(const AutofillField* field) : field_(field) {}

void SearchField::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(field_, SEARCH_TERM, kBaseSearchParserScore,
                    field_candidates);
}

}  // namespace autofill
