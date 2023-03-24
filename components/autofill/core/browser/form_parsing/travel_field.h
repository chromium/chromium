// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_TRAVEL_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_TRAVEL_FIELD_H_

#include <memory>

#include "base/memory/raw_ptr_exclusion.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

class LogManager;

class TravelField : public FormField {
 public:
  ~TravelField() override;

  static std::unique_ptr<FormField> Parse(AutofillScanner* scanner,
                                          const LanguageCode& page_language,
                                          PatternSource pattern_source,
                                          LogManager* log_manager);

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  // All of the following fields are optional.
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION AutofillField* passport_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION AutofillField* origin_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION AutofillField* destination_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION AutofillField* flight_;
};
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_TRAVEL_FIELD_H_
