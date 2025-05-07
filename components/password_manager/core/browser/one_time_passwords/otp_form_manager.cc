// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/one_time_passwords/otp_form_manager.h"

namespace password_manager {

OtpFormManager::OtpFormManager(
    autofill::FormGlobalId form_id,
    const std::vector<autofill::FieldGlobalId>& otp_field_ids)
    : form_id_(form_id), otp_field_ids_(std::move(otp_field_ids)) {
  // TODO(crbug.com/415273770): Trigger OTP fetching if needed.
}

OtpFormManager::OtpFormManager(OtpFormManager&&) = default;
OtpFormManager& OtpFormManager::operator=(OtpFormManager&&) = default;

OtpFormManager::~OtpFormManager() = default;

void OtpFormManager::ProcessUpdatedPredictions(
    const std::vector<autofill::FieldGlobalId>& otp_field_ids) {
  otp_field_ids_ = std::move(otp_field_ids);

  // TODO(crbug.com/415273770): Check if OTP source has changed.
}

}  // namespace password_manager
