// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_NUMERIC_QUANTITY_FIELD_PARSER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_NUMERIC_QUANTITY_FIELD_PARSER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

class AutofillField;
class AutofillScanner;

// Numeric quantities that are not eligible to be filled by Autofill.
class NumericQuantityFieldParser : public FormFieldParser {
 public:
  static std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                                AutofillScanner* scanner);

  NumericQuantityFieldParser(const NumericQuantityFieldParser&) = delete;
  NumericQuantityFieldParser& operator=(const NumericQuantityFieldParser&) =
      delete;

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  explicit NumericQuantityFieldParser(const AutofillField* field);

  raw_ptr<const AutofillField> field_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_NUMERIC_QUANTITY_FIELD_PARSER_H_
