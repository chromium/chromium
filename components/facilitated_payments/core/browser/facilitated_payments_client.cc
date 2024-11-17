// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/data_model/ewallet.h"
#include "components/facilitated_payments/core/ui_utils/facilitated_payments_ui_utils.h"

namespace payments::facilitated {

FacilitatedPaymentsClient::~FacilitatedPaymentsClient() = default;

void FacilitatedPaymentsClient::ShowPixPaymentPrompt(
    base::span<const autofill::BankAccount> bank_account_suggestions,
    base::OnceCallback<void(bool, int64_t)> on_user_decision_callback) {}

void FacilitatedPaymentsClient::ShowEwalletPaymentPrompt(
    base::span<const autofill::Ewallet> ewallet_suggestions,
    base::OnceCallback<void(bool, int64_t)> on_user_decision_callback) {}

void FacilitatedPaymentsClient::ShowProgressScreen() {}

void FacilitatedPaymentsClient::ShowErrorScreen() {}

void FacilitatedPaymentsClient::DismissPrompt() {}

void FacilitatedPaymentsClient::SetUiEventListener(
    base::RepeatingCallback<void(UiEvent)> ui_event_listener) {}

}  // namespace payments::facilitated
