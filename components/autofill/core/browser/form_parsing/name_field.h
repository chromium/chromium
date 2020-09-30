// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_NAME_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_NAME_FIELD_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"

namespace autofill {

class AutofillScanner;
class LogManager;

// A form field that can parse either a FullNameField or a FirstLastNameField.
class NameField : public FormField {
 public:
  static std::unique_ptr<FormField> Parse(AutofillScanner* scanner,
                                          const std::string& page_language,
                                          LogManager* log_manager);

#ifdef UNIT_TEST
  // Calls the protected method |AddClassification| for testing.
  void AddClassificationsForTesting(
      FieldCandidatesMap* field_candidates) const {
    AddClassifications(field_candidates);
  }
#endif

 protected:
  NameField() {}

  void AddClassifications(FieldCandidatesMap* field_candidates) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NameField);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_NAME_FIELD_H_
