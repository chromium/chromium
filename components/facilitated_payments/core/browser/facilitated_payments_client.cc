// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"

#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "components/autofill/core/browser/data_model/payments/bank_account.h"
#include "components/autofill/core/browser/data_model/payments/ewallet.h"
#include "components/facilitated_payments/core/browser/pix_account_linking_manager.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_ui_utils.h"

namespace payments::facilitated {

FacilitatedPaymentsClient::FacilitatedPaymentsClient()
    : pix_account_linking_manager_(
          std::make_unique<PixAccountLinkingManager>(/* client= */ this)) {}

FacilitatedPaymentsClient::~FacilitatedPaymentsClient() = default;

void FacilitatedPaymentsClient::ShowPixPaymentPrompt(
    base::span<const autofill::BankAccount> bank_account_suggestions,
    base::OnceCallback<void(int64_t)> on_payment_account_selected) {}

void FacilitatedPaymentsClient::ShowEwalletPaymentPrompt(
    base::span<const autofill::Ewallet> ewallet_suggestions,
    base::OnceCallback<void(int64_t)> on_payment_account_selected) {}

void FacilitatedPaymentsClient::ShowProgressScreen() {}

void FacilitatedPaymentsClient::ShowErrorScreen() {}

void FacilitatedPaymentsClient::DismissPrompt() {}

void FacilitatedPaymentsClient::SetUiEventListener(
    base::RepeatingCallback<void(UiEvent)> ui_event_listener) {}

void FacilitatedPaymentsClient::InitPixAccountLinkingFlow() {
  pix_account_linking_manager_->MaybeShowPixAccountLinkingPrompt();
}

bool FacilitatedPaymentsClient::IsPixAccountLinkingSupported() const {
  return false;
}

void FacilitatedPaymentsClient::ShowPixAccountLinkingPrompt() {}

void FacilitatedPaymentsClient::SetPixAccountLinkingManagerForTesting(
    std::unique_ptr<PixAccountLinkingManager> pix_account_linking_manager) {
  pix_account_linking_manager_ = std::move(pix_account_linking_manager);
}

}  // namespace payments::facilitated
