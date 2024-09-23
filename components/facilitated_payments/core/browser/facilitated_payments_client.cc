// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"

#include "base/functional/callback.h"

namespace payments::facilitated {

FacilitatedPaymentsClient::~FacilitatedPaymentsClient() = default;

bool FacilitatedPaymentsClient::ShowPixPaymentPrompt(
    base::span<const autofill::BankAccount> bank_account_suggestions,
    base::OnceCallback<void(bool, int64_t)> on_user_decision_callback) {
  return false;
}

void FacilitatedPaymentsClient::ShowProgressScreen() {}

void FacilitatedPaymentsClient::ShowErrorScreen() {}

void FacilitatedPaymentsClient::DismissPrompt() {}

}  // namespace payments::facilitated
