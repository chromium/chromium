// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_TRAVEL_FIELD_PARSER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_TRAVEL_FIELD_PARSER_H_

#include <memory>

#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

class TravelFieldParser : public FormFieldParser {
 public:
  TravelFieldParser();
  ~TravelFieldParser() override;

  static std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                                AutofillScanner* scanner);

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  std::optional<FieldAndMatchInfo> passport_;
  std::optional<FieldAndMatchInfo> origin_;
  std::optional<FieldAndMatchInfo> destination_;
  std::optional<FieldAndMatchInfo> flight_;
};
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_TRAVEL_FIELD_PARSER_H_
