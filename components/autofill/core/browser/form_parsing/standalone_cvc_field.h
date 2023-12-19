// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_STANDALONE_CVC_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_STANDALONE_CVC_FIELD_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

class AutofillField;
class AutofillScanner;

// A form field that accepts a standalone cvc.
class StandaloneCvcField : public FormField {
 public:
  static std::unique_ptr<FormField> Parse(ParsingContext& context,
                                          AutofillScanner* scanner);

  explicit StandaloneCvcField(const AutofillField* field);

  ~StandaloneCvcField() override;

  StandaloneCvcField(const StandaloneCvcField&) = delete;
  StandaloneCvcField& operator=(const StandaloneCvcField&) = delete;

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  raw_ptr<const AutofillField> field_;

  // static
  static bool MatchGiftCard(ParsingContext& context, AutofillScanner* scanner);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_STANDALONE_CVC_FIELD_H_
