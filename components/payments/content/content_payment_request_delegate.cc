// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/content_payment_request_delegate.h"

namespace payments {

ContentPaymentRequestDelegate::~ContentPaymentRequestDelegate() = default;

base::WeakPtr<ContentPaymentRequestDelegate>
ContentPaymentRequestDelegate::GetContentWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

ContentPaymentRequestDelegate::ContentPaymentRequestDelegate() = default;

}  // namespace payments
