// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/ewallet_manager.h"

#include <algorithm>

#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "components/autofill/core/browser/data_model/ewallet.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/facilitated_payments/core/util/payment_link_validator.h"
#include "url/gurl.h"

namespace payments::facilitated {

EwalletManager::EwalletManager(
    FacilitatedPaymentsClient* client,
    FacilitatedPaymentsApiClientCreator api_client_creator)
    : client_(CHECK_DEREF(client)),
      api_client_creator_(std::move(api_client_creator)) {}
EwalletManager::~EwalletManager() = default;

// TODO(crbug.com/40280186): Add tests for this method.
void EwalletManager::TriggerEwalletPushPayment(const GURL& payment_link_url,
                                               const GURL& page_url) {
  if (!PaymentLinkValidator().IsValid(payment_link_url.spec())) {
    return;
  }

  // Ewallet payment flow can't be completed in the landscape mode as the
  // Payments server doesn't support it yet.
  if (client_->IsInLandscapeMode()) {
    return;
  }

  base::span<const autofill::Ewallet> ewallet_accounts =
      client_->GetPaymentsDataManager()->GetEwalletAccounts();
  supported_ewallets_.reserve(ewallet_accounts.size());
  std::ranges::copy_if(
      ewallet_accounts, std::back_inserter(supported_ewallets_),
      [&payment_link_url](const autofill::Ewallet& ewallet) {
        return ewallet.SupportsPaymentLink(payment_link_url.spec());
      });

  if (supported_ewallets_.size() == 0) {
    return;
  }

  // TODO(crbug.com/40280186): check allowlist.

  if (!GetApiClient()) {
    return;
  }

  GetApiClient()->IsAvailable(
      base::BindOnce(&EwalletManager::OnApiAvailabilityReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EwalletManager::Reset() {
  supported_ewallets_.clear();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

FacilitatedPaymentsApiClient* EwalletManager::GetApiClient() {
  if (!api_client_) {
    if (api_client_creator_) {
      api_client_ = std::move(api_client_creator_).Run();
    }
  }

  return api_client_.get();
}

void EwalletManager::OnApiAvailabilityReceived(bool is_api_available) {
  if (!is_api_available) {
    return;
  }

  // TODO(crbug.com/40280186): Implement the callback.
  client_->ShowEwalletPaymentPrompt(supported_ewallets_, base::DoNothing());
}

}  // namespace payments::facilitated
