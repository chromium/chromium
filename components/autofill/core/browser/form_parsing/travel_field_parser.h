// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_TRAVEL_FIELD_PARSER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_TRAVEL_FIELD_PARSER_H_

#include <memory>

#include "base/memory/raw_ptr_exclusion.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

class TravelFieldParser : public FormFieldParser {
 public:
  ~TravelFieldParser() override;

  static std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                                AutofillScanner* scanner);

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  // All of the following fields are optional.
  raw_ptr<AutofillField> passport_;
  raw_ptr<AutofillField> origin_;
  raw_ptr<AutofillField> destination_;
  raw_ptr<AutofillField> flight_;
};
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_TRAVEL_FIELD_PARSER_H_
