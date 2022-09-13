// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_STANDALONE_CVC_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_STANDALONE_CVC_FIELD_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

class AutofillField;
class AutofillScanner;
class LogManager;

// A form field that accepts a standalone cvc.
class StandaloneCvcField : public FormField {
 public:
  static std::unique_ptr<FormField> Parse(AutofillScanner* scanner,
                                          const LanguageCode& page_language,
                                          PatternSource pattern_source,
                                          LogManager* log_manager);

  explicit StandaloneCvcField(const AutofillField* field);

  ~StandaloneCvcField() override;

  StandaloneCvcField(const StandaloneCvcField&) = delete;
  StandaloneCvcField& operator=(const StandaloneCvcField&) = delete;

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  raw_ptr<const AutofillField> field_;

  // static
  static bool MatchGiftCard(AutofillScanner* scanner,
                            LogManager* log_manager,
                            const LanguageCode& page_language,
                            PatternSource pattern_source);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_STANDALONE_CVC_FIELD_H_
