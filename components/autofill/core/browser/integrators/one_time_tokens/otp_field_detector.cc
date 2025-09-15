// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_field_detector.h"

#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/form_structure.h"

namespace autofill {

OtpFieldDetector::OtpFieldDetector() = default;
OtpFieldDetector::~OtpFieldDetector() = default;

base::CallbackListSubscription
OtpFieldDetector::RegisterOtpFieldsDetectedCallback(
    OtpFieldDetector::OtpFieldsDetectedCallback callback) {
  return callback_list_otp_fields_detected_.Add(std::move(callback));
}

base::CallbackListSubscription
OtpFieldDetector::RegisterOtpFieldsSubmittedCallback(
    OtpFieldDetector::OtpFieldsSubmittedCallback callback) {
  return callback_list_otp_fields_submitted_.Add(std::move(callback));
}

bool OtpFieldDetector::IsOtpFieldPresent() const {
  const bool is_otp_present = !forms_with_otps_.empty();
  // TODO(crbug.com/415273270) This metric could be improved because
  // 1) there is no guarantee inside `OtpFieldDetector` that
  //   `IsOneTimeTokenFieldPresent()` is called only once
  // 2) because the `OtpFieldDetector` also also considers OTP fields
  //    in iframes (i.e. the "InMainFrame" suffix is incorrect).
  // This exists for legacy purposes.
  base::UmaHistogramBoolean("PasswordManager.OtpPresentInMainTab",
                            is_otp_present);
  return is_otp_present;
}

void OtpFieldDetector::AddFormAndNotifyIfNecessary(FormGlobalId form_id) {
  // Memorize the state of `forms_with_otps_` before the update, then perform
  // the update and only then notify the callbacks to ensure that
  // `IsOneTimeTokenFieldPresent()` returns the correct answer when called by
  // a notified callback.
  const bool was_empty = forms_with_otps_.empty();
  forms_with_otps_.insert(form_id);
  if (was_empty) {
    callback_list_otp_fields_detected_.Notify();
  }
}

void OtpFieldDetector::RemoveFormAndNotifyIfNecessary(FormGlobalId form_id) {
  if (!forms_with_otps_.contains(form_id)) {
    return;
  }
  forms_with_otps_.erase(form_id);
  if (forms_with_otps_.empty()) {
    callback_list_otp_fields_submitted_.Notify();
  }
}

}  // namespace autofill
