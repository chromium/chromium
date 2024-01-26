// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_STANDALONE_CVC_FIELD_PARSER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_STANDALONE_CVC_FIELD_PARSER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

class AutofillField;
class AutofillScanner;

// A form field that accepts a standalone cvc.
class StandaloneCvcFieldParser : public FormFieldParser {
 public:
  static std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                                AutofillScanner* scanner);

  explicit StandaloneCvcFieldParser(const AutofillField* field);

  ~StandaloneCvcFieldParser() override;

  StandaloneCvcFieldParser(const StandaloneCvcFieldParser&) = delete;
  StandaloneCvcFieldParser& operator=(const StandaloneCvcFieldParser&) = delete;

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  raw_ptr<const AutofillField> field_;

  // static
  static bool MatchGiftCard(ParsingContext& context, AutofillScanner* scanner);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_STANDALONE_CVC_FIELD_PARSER_H_
