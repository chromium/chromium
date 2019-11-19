// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_EMAIL_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_EMAIL_FIELD_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"

namespace autofill {

class LogManager;

class EmailField : public FormField {
 public:
  static std::unique_ptr<FormField> Parse(AutofillScanner* scanner,
                                          LogManager* log_manager);
  explicit EmailField(const AutofillField* field);

 protected:
  void AddClassifications(FieldCandidatesMap* field_candidates) const override;

 private:
  const AutofillField* field_;

  DISALLOW_COPY_AND_ASSIGN(EmailField);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_EMAIL_FIELD_H_
