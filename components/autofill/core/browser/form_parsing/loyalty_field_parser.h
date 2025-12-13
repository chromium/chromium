// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_LOYALTY_FIELD_PARSER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_LOYALTY_FIELD_PARSER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"

namespace autofill {

// A form field that accepts Loyalty Card IDs.
class LoyaltyFieldParser : public FormFieldParser {
 public:
  static std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                                AutofillScanner& scanner);
  explicit LoyaltyFieldParser(FieldAndMatchInfo match);

  LoyaltyFieldParser(const LoyaltyFieldParser&) = delete;
  LoyaltyFieldParser& operator=(const LoyaltyFieldParser&) = delete;

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  FieldAndMatchInfo match_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_LOYALTY_FIELD_PARSER_H_
