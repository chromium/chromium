// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_no_creds_model.h"

namespace payments {

SecurePaymentConfirmationNoCredsModel::SecurePaymentConfirmationNoCredsModel() =
    default;

SecurePaymentConfirmationNoCredsModel::
    ~SecurePaymentConfirmationNoCredsModel() = default;

base::WeakPtr<SecurePaymentConfirmationNoCredsModel>
SecurePaymentConfirmationNoCredsModel::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace payments
