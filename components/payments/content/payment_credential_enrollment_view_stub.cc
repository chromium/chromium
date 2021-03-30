// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_credential_enrollment_view.h"

namespace payments {

// static
base::WeakPtr<PaymentCredentialEnrollmentView>
PaymentCredentialEnrollmentView::Create() {
  return nullptr;
}

PaymentCredentialEnrollmentView::PaymentCredentialEnrollmentView() = default;
PaymentCredentialEnrollmentView::~PaymentCredentialEnrollmentView() = default;

}  // namespace payments
