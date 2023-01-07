// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_no_creds_view.h"

namespace payments {

// static
// Stub of create() method. Desktop implementation in
// secure_payment_confirmation_no_creds_dialog_view.cc
base::WeakPtr<SecurePaymentConfirmationNoCredsView>
SecurePaymentConfirmationNoCredsView::Create() {
  return nullptr;
}

}  // namespace payments
