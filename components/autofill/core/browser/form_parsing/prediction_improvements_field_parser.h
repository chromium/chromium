// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_PREDICTION_IMPROVEMENTS_FIELD_PARSER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_PREDICTION_IMPROVEMENTS_FIELD_PARSER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"

namespace autofill {

class AutofillField;
class AutofillScanner;

// Parser to find fields that are eligible for prediction improvements.
class PredictionImprovementsFieldParser : public FormFieldParser {
 public:
  static std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                                AutofillScanner* scanner);
  explicit PredictionImprovementsFieldParser(const AutofillField* field);

  PredictionImprovementsFieldParser(const PredictionImprovementsFieldParser&) =
      delete;
  PredictionImprovementsFieldParser& operator=(
      const PredictionImprovementsFieldParser&) = delete;

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(PredictionImprovementsFieldParser, ParseSearchTerm);
  FRIEND_TEST_ALL_PREFIXES(PredictionImprovementsFieldParser,
                           ParseNonSearchTerm);

  raw_ptr<const AutofillField> field_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_PREDICTION_IMPROVEMENTS_FIELD_PARSER_H_
