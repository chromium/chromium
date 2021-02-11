// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_credential_enrollment_model.h"

namespace payments {

PaymentCredentialEnrollmentModel::PaymentCredentialEnrollmentModel() = default;

PaymentCredentialEnrollmentModel::~PaymentCredentialEnrollmentModel() = default;

base::WeakPtr<PaymentCredentialEnrollmentModel>
PaymentCredentialEnrollmentModel::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace payments
