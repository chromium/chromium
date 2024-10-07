// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/alternative_name_field_parser.h"

#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"

namespace autofill {

// static
std::unique_ptr<FormFieldParser> AlternativeNameFieldParser::Parse(
    ParsingContext& context,
    AutofillScanner* scanner) {
  NOTIMPLEMENTED();
  return nullptr;
}

void AlternativeNameFieldParser::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  NOTIMPLEMENTED();
}

}  // namespace autofill
