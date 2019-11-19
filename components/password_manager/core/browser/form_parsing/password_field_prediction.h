// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_PASSWORD_FIELD_PREDICTION_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_PASSWORD_FIELD_PREDICTION_H_

#include <stdint.h>
#include <vector>

#include "build/build_config.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/signatures_util.h"

namespace autofill {
class FormStructure;
}  // namespace autofill

namespace password_manager {

enum class CredentialFieldType {
  kNone,
  kUsername,
  kSingleUsername,
  kCurrentPassword,
  kNewPassword,
  kConfirmationPassword
};

// Transforms the general field type to the information useful for password
// forms.
CredentialFieldType DeriveFromServerFieldType(autofill::ServerFieldType type);

// Contains server predictions for a field.
struct PasswordFieldPrediction {
  // Field identifier generated in Blink on non-iOS platforms.
  uint32_t renderer_id;
#if defined(OS_IOS)
  base::string16 unique_id;
#endif
  autofill::FieldSignature signature;
  autofill::ServerFieldType type;
  bool may_use_prefilled_placeholder = false;
};

// Contains server predictions for a form.
struct FormPredictions {
  FormPredictions();
  FormPredictions(const FormPredictions&);
  FormPredictions& operator=(const FormPredictions&);
  FormPredictions(FormPredictions&&);
  FormPredictions& operator=(FormPredictions&&);
  ~FormPredictions();

  // Id of PasswordManagerDriver which corresponds to the frame of this form.
  int driver_id = 0;

  autofill::FormSignature form_signature;
  std::vector<PasswordFieldPrediction> fields;
};

// Extracts all password related server predictions from |form_structure|.
FormPredictions ConvertToFormPredictions(
    int driver_id,
    const autofill::FormStructure& form_structure);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_PASSWORD_FIELD_PREDICTION_H_
