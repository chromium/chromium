// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/ewallet_manager.h"

#include <algorithm>

#include "base/check_deref.h"
#include "components/autofill/core/browser/data_model/ewallet.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/util/payment_link_validator.h"
#include "url/gurl.h"

namespace payments::facilitated {

EwalletManager::EwalletManager(FacilitatedPaymentsClient* client)
    : client_(CHECK_DEREF(client)) {}
EwalletManager::~EwalletManager() = default;

// TODO(crbug.com/40280186): Add tests for this method.
void EwalletManager::TriggerEwalletPushPayment(const GURL& payment_link_url,
                                               const GURL& page_url) {
  if (!PaymentLinkValidator().IsValid(payment_link_url.spec())) {
    return;
  }

  base::span<const autofill::Ewallet> ewallet_accounts =
      client_->GetPaymentsDataManager()->GetEwalletAccounts();
  std::vector<autofill::Ewallet> supported_ewallets;
  supported_ewallets.reserve(ewallet_accounts.size());
  std::ranges::copy_if(
      ewallet_accounts, std::back_inserter(supported_ewallets),
      [&payment_link_url](const autofill::Ewallet& ewallet) {
        return ewallet.SupportsPaymentLink(payment_link_url.spec());
      });

  if (supported_ewallets.size() == 0) {
    return;
  }

  // TODO(crbug.com/40280186): check allowlist and trigger ewallet prompt.
}

}  // namespace payments::facilitated
