// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PASSWORD_MANAGER_PASSWORD_MANAGER_AUTOFILL_HELPER_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PASSWORD_MANAGER_PASSWORD_MANAGER_AUTOFILL_HELPER_DELEGATE_H_

#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class PasswordManagerAutofillHelperDelegate {
 public:
  virtual ~PasswordManagerAutofillHelperDelegate() = default;

  // Returns true if the field identified by `form_id` and `field_id` was last
  // filled by an OTP.
  virtual bool IsFieldFilledWithOtp(FormGlobalId form_id,
                                    FieldGlobalId field_id) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PASSWORD_MANAGER_PASSWORD_MANAGER_AUTOFILL_HELPER_DELEGATE_H_
