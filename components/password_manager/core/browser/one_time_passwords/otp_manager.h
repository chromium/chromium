// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ONE_TIME_PASSWORDS_OTP_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ONE_TIME_PASSWORDS_OTP_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {
class FormData;
}  // namespace autofill

namespace password_manager {

class OtpFormManager;
class PasswordManagerClient;

// A class in charge of handling one time passwords, one per tab.
class OtpManager {
 public:
  explicit OtpManager(PasswordManagerClient* client);

  ~OtpManager();

  // Processes the classification model predictions received via Autofill.
  void ProcessClassificationModelPredictions(
      const autofill::FormData& form,
      const base::flat_map<autofill::FieldGlobalId, autofill::FieldType>&
          field_predictions);

#if defined(UNIT_TEST)
  const base::flat_map<autofill::FormGlobalId, OtpFormManager>& form_managers()
      const {
    return form_managers_;
  }
#endif  // defined(UNIT_TEST)

 private:
  // The client that owns this class and is guaranteed to outlive it.
  const raw_ptr<PasswordManagerClient> client_;

  // Managers managing individual forms.
  base::flat_map<autofill::FormGlobalId, OtpFormManager> form_managers_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ONE_TIME_PASSWORDS_OTP_MANAGER_H_
