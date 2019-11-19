// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/price_field.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

// static
std::unique_ptr<FormField> PriceField::Parse(AutofillScanner* scanner,
                                             LogManager* log_manager) {
  AutofillField* field;
  if (ParseFieldSpecifics(scanner, base::UTF8ToUTF16(kPriceRe),
                          MATCH_DEFAULT | MATCH_NUMBER | MATCH_SELECT |
                              MATCH_TEXT_AREA | MATCH_SEARCH,
                          &field, {log_manager, kPriceRe})) {
    return std::make_unique<PriceField>(field);
  }

  return nullptr;
}

PriceField::PriceField(const AutofillField* field) : field_(field) {}

void PriceField::AddClassifications(
    FieldCandidatesMap* field_candidates) const {
  AddClassification(field_, PRICE, kBasePriceParserScore, field_candidates);
}

}  // namespace autofill
