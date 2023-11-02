// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_model.h"

namespace payments {

SecurePaymentConfirmationModel::SecurePaymentConfirmationModel() = default;

SecurePaymentConfirmationModel::~SecurePaymentConfirmationModel() = default;

base::WeakPtr<SecurePaymentConfirmationModel>
SecurePaymentConfirmationModel::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace payments
