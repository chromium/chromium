// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/pix_account_linking_manager.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"

namespace payments::facilitated {

PixAccountLinkingManager::PixAccountLinkingManager(
    FacilitatedPaymentsClient* client)
    : client_(CHECK_DEREF(client)) {}

PixAccountLinkingManager::~PixAccountLinkingManager() = default;

void PixAccountLinkingManager::MaybeShowPixAccountLinkingPrompt() {
  if (!client_->GetDeviceDelegate()->IsPixAccountLinkingSupported()) {
    return;
  }

  if (!client_->GetPaymentsDataManager()
           ->IsFacilitatedPaymentsPixAccountLinkingUserPrefEnabled()) {
    return;
  }
  // Make a request to payments backend to check if user is eligible for pix
  // account linking.
  auto billing_customer_id = autofill::payments::GetBillingCustomerId(
      CHECK_DEREF(client_->GetPaymentsDataManager()));
  if (billing_customer_id == 0) {
    // If the user is not a payments customer and has copied a Pix code, we
    // automatically assume that they are eligible for account linking.
    is_eligible_for_pix_account_linking_ = true;
  } else {
    // The user is an existing payments customer. Make a backend call to check
    // eligibility for Pix account linking.
    client_->GetMultipleRequestFacilitatedPaymentsNetworkInterface()
        ->GetDetailsForCreatePaymentInstrument(
            billing_customer_id,
            base::BindOnce(
                &PixAccountLinkingManager::
                    OnGetDetailsForCreatePaymentInstrumentResponseReceived,
                weak_ptr_factory_.GetWeakPtr()),
            client_->GetPaymentsDataManager()->app_locale());
  }
  // TODO(crbug.com/417330610): Move this to after the user comes back to Chrome
  // and GetDetailsForCreatePaymentInstrument is completed.
  client_->GetDeviceDelegate()->SetOnReturnToChromeCallback(base::BindOnce(
      &PixAccountLinkingManager::ShowPixAccountLinkingPromptIfEligible,
      weak_ptr_factory_.GetWeakPtr()));
}

void PixAccountLinkingManager::ShowPixAccountLinkingPromptIfEligible() {
  // If the server-side eligibility check is incomplete, or if ineligible for
  // account linking, exit.
  if (!is_eligible_for_pix_account_linking_.has_value() ||
      !is_eligible_for_pix_account_linking_.value()) {
    return;
  }

  client_->SetUiEventListener(
      base::BindRepeating(&PixAccountLinkingManager::OnUiScreenEvent,
                          weak_ptr_factory_.GetWeakPtr()));
  client_->ShowPixAccountLinkingPrompt(
      base::BindOnce(&PixAccountLinkingManager::OnAccepted,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PixAccountLinkingManager::OnDeclined,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PixAccountLinkingManager::OnAccepted() {
  // TODO(crbug.com/419108993): Add metrics.
  client_->DismissPrompt();
  client_->GetDeviceDelegate()->LaunchPixAccountLinkingPage();
}

void PixAccountLinkingManager::OnDeclined() {
  // TODO(crbug.com/419108993): Add metrics.
  client_->DismissPrompt();
  client_->GetPaymentsDataManager()
      ->SetFacilitatedPaymentsPixAccountLinkingUserPref(/* enabled= */ false);
}

void PixAccountLinkingManager::OnUiScreenEvent(UiEvent ui_event_type) {
  switch (ui_event_type) {
    case UiEvent::kNewScreenShown: {
      // TODO(crbug.com/419108993): Add specific logging for Pix Account Linking
      // prompt shown.
      break;
    }
    case UiEvent::kScreenClosedNotByUser: {
      // TODO(crbug.com/419108993): Add specific logging for Pix Account Linking
      // prompt closed not by user.
      break;
    }
    case UiEvent::kScreenClosedByUser: {
      // TODO(crbug.com/419108993): Add specific logging for Pix Account Linking
      // prompt closed by user.
      break;
    }
    default:
      NOTREACHED() << "Unhandled UiEvent "
                   << base::to_underlying(ui_event_type);
  }
}

void PixAccountLinkingManager::
    OnGetDetailsForCreatePaymentInstrumentResponseReceived(
        autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result,
        bool is_eligible_for_pix_account_linking) {
  // TODO(crbug.com/419108993): Log the result and eligibility for account
  // linking.
  is_eligible_for_pix_account_linking_ = is_eligible_for_pix_account_linking;
}

}  // namespace payments::facilitated
