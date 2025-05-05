// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/one_time_passwords/otp_manager.h"

#include "components/autofill/core/common/form_data.h"
#include "components/password_manager/core/browser/password_manager_client.h"

namespace password_manager {

OtpManager::OtpManager(PasswordManagerClient* client) : client_(client) {
  DCHECK(client_);
}

void OtpManager::ProcessClassificationModelPredictions(
    const autofill::FormData& form,
    const base::flat_map<autofill::FieldGlobalId, autofill::FieldType>&
        field_predictions) {
  // TODO(415269545): Rationalize predictions and trigger OTP fetching if
  // needed.

  client_->InformPasswordChangeServiceOfOtpPresent();
}

}  // namespace password_manager
