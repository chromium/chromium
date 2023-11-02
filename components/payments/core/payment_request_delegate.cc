// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_request_delegate.h"

namespace payments {

PaymentRequestDelegate::PaymentRequestDelegate() = default;

PaymentRequestDelegate::~PaymentRequestDelegate() = default;

base::WeakPtr<PaymentRequestDelegate> PaymentRequestDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace payments
