// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/ewallet_manager.h"

#include "components/facilitated_payments/core/util/payment_link_validator.h"
#include "url/gurl.h"

namespace payments::facilitated {

EwalletManager::EwalletManager() = default;
EwalletManager::~EwalletManager() = default;

// TODO(crbug.com/40280186): Add tests for this method.
void EwalletManager::TriggerEwalletPushPayment(const GURL& payment_link_url,
                                               const GURL& page_url) {
  if (!PaymentLinkValidator().IsValid(payment_link_url.spec())) {
    return;
  }

  // TODO(crbug.com/40280186): check allowlist and trigger ewallet prompt.
}

}  // namespace payments::facilitated
