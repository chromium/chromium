// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/search_field.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

// static
std::unique_ptr<FormField> SearchField::Parse(AutofillScanner* scanner,
                                              LogManager* log_manager) {
  AutofillField* field;
  if (ParseFieldSpecifics(scanner, base::UTF8ToUTF16(kSearchTermRe),
                          MATCH_DEFAULT | MATCH_SEARCH | MATCH_TEXT_AREA,
                          &field, {log_manager, "kSearchTermRe"})) {
    return std::make_unique<SearchField>(field);
  }

  return nullptr;
}

SearchField::SearchField(const AutofillField* field) : field_(field) {}

void SearchField::AddClassifications(
    FieldCandidatesMap* field_candidates) const {
  AddClassification(field_, SEARCH_TERM, kBaseSearchParserScore,
                    field_candidates);
}

}  // namespace autofill
