// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ONE_TIME_PASSWORDS_OTP_FORM_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ONE_TIME_PASSWORDS_OTP_FORM_MANAGER_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/unique_ids.h"

namespace password_manager {

// A class in charge of handling individual OTP forms, one instance per form.
class OtpFormManager {
 public:
  OtpFormManager(autofill::FormGlobalId form_id,
                 const std::vector<autofill::FieldGlobalId>& otp_field_ids);

  OtpFormManager(const OtpFormManager&) = delete;
  OtpFormManager& operator=(const OtpFormManager&) = delete;
  OtpFormManager(OtpFormManager&&);
  OtpFormManager& operator=(OtpFormManager&&);

  ~OtpFormManager();

  // Forms can change dynamically during their lifetime. Ensure the most recent
  // data is used for form filling.
  void ProcessUpdatedPredictions(
      const std::vector<autofill::FieldGlobalId>& otp_field_ids);

#if defined(UNIT_TEST)
  const std::vector<autofill::FieldGlobalId>& otp_field_ids() const {
    return otp_field_ids_;
  }
#endif  // defined(UNIT_TEST)

 private:
  autofill::FormGlobalId form_id_;

  std::vector<autofill::FieldGlobalId> otp_field_ids_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ONE_TIME_PASSWORDS_OTP_FORM_MANAGER_H_
