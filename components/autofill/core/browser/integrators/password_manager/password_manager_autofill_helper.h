// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PASSWORD_MANAGER_PASSWORD_MANAGER_AUTOFILL_HELPER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PASSWORD_MANAGER_PASSWORD_MANAGER_AUTOFILL_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/integrators/password_manager/password_manager_autofill_helper_delegate.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillClient;
class AutofillField;

class PasswordManagerAutofillHelper
    : public PasswordManagerAutofillHelperDelegate {
 public:
  explicit PasswordManagerAutofillHelper(AutofillClient* client);
  ~PasswordManagerAutofillHelper() override;

  PasswordManagerAutofillHelper(const PasswordManagerAutofillHelper&) = delete;
  PasswordManagerAutofillHelper& operator=(
      const PasswordManagerAutofillHelper&) = delete;

  // PasswordManagerAutofillHelperDelegate:
  bool IsFieldFilledWithOtp(FormGlobalId form_id,
                            FieldGlobalId field_id) override;

  // Returns true if the field is autofilled with an OTP.
  static bool IsOtpFilledField(const AutofillField& field);

 private:
  // Owner:
  raw_ptr<AutofillClient> client_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PASSWORD_MANAGER_PASSWORD_MANAGER_AUTOFILL_HELPER_H_
