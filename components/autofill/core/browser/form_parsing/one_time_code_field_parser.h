// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ONE_TIME_CODE_FIELD_PARSER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ONE_TIME_CODE_FIELD_PARSER_H_

#include <memory>

#include "components/autofill/core/browser/form_parsing/form_field_parser.h"

namespace autofill {

class AutofillScanner;

class OneTimeCodeFieldParser : public FormFieldParser {
 public:
  static std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                                AutofillScanner& scanner);
  explicit OneTimeCodeFieldParser(FieldAndMatchInfo match);

  OneTimeCodeFieldParser(const OneTimeCodeFieldParser&) = delete;
  OneTimeCodeFieldParser& operator=(const OneTimeCodeFieldParser&) = delete;

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  FieldAndMatchInfo match_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ONE_TIME_CODE_FIELD_PARSER_H_
