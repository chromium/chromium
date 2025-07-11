// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PASSWORD_MANAGER_OTP_SUGGESTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PASSWORD_MANAGER_OTP_SUGGESTION_DELEGATE_H_

#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

// This delegate is queried for OTP suggestions for a given field.
// This interface is required for communication from //components/autofill to
// //components/password_manager/core/browser/one_time_passwords.
// //components/password_manager depends on //components/autofill, so to allow
// communication from //components/autofill to //components/password_manager,
// this interface is injected via `AutofillClient`.
class OtpSuggestionDelegate {
 public:
  virtual ~OtpSuggestionDelegate() = default;

  // Returns whether OTP suggestions can be shown on `field` (if the field was
  // parsed as an OTP field, and a OTP value was successfully retrieved).
  virtual bool IsFieldEligibleForOtpFilling(
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id) const = 0;

  // Invokes `callback` with the OTP value suggestions for a given `form_id` &
  // `field_id`.
  virtual void GetOtpSuggestions(
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id,
      base::OnceCallback<void(std::vector<std::string>)> callback) const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PASSWORD_MANAGER_OTP_SUGGESTION_DELEGATE_H_
