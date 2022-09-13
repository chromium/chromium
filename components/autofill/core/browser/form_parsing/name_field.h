// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_NAME_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_NAME_FIELD_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

class AutofillScanner;
class LogManager;

// A form field that can parse either a FullNameField or a FirstLastNameField.
class NameField : public FormField {
 public:
  static std::unique_ptr<FormField> Parse(AutofillScanner* scanner,
                                          const LanguageCode& page_language,
                                          PatternSource pattern_source,
                                          LogManager* log_manager);

  NameField(const NameField&) = delete;
  NameField& operator=(const NameField&) = delete;

 protected:
  NameField() = default;

  void AddClassifications(FieldCandidatesMap& field_candidates) const override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_NAME_FIELD_H_
