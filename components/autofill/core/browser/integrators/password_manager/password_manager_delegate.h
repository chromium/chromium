// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PASSWORD_MANAGER_PASSWORD_MANAGER_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PASSWORD_MANAGER_PASSWORD_MANAGER_DELEGATE_H_

#include "components/autofill/core/common/password_form_fill_data.h"

namespace autofill {

// This delegate is queried for PWM suggestions for a given field. It injects
// Password Manager logic into `AutofillManager::OnAskForForValuesToFill`.
// If password suggestions are required, the work is performed by the underlying
// `PasswordAutofillManager`.
class PasswordManagerDelegate {
 public:
  virtual ~PasswordManagerDelegate() = default;

  virtual void ShowSuggestions(
      const autofill::TriggeringField& triggering_field) = 0;

#if BUILDFLAG(IS_ANDROID)
  virtual void ShowKeyboardReplacingSurface(
      const autofill::PasswordSuggestionRequest& request) = 0;
#endif  // BUILDFLAG(IS_ANDROID)
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PASSWORD_MANAGER_PASSWORD_MANAGER_DELEGATE_H_
