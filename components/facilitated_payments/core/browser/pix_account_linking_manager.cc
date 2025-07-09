// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/pix_account_linking_manager.h"

#include "base/check_deref.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"

namespace payments::facilitated {

PixAccountLinkingManager::PixAccountLinkingManager(
    FacilitatedPaymentsClient* client)
    : client_(CHECK_DEREF(client)) {}

PixAccountLinkingManager::~PixAccountLinkingManager() {
  if (is_prompt_showing_) {
    // The prompt closed unexpectedly, so the internal state is not updated. The
    // event listener would log metrics accordingly.
    client_->DismissPrompt();
  }
}

void PixAccountLinkingManager::MaybeShowPixAccountLinkingPrompt() {
  // Reset to default state to prepare for a new account linking flow.
  Reset();
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

void PixAccountLinkingManager::Reset() {
  is_eligible_for_pix_account_linking_ = std::nullopt;
  if (is_prompt_showing_) {
    // This should NOT happen as the account linking flow cannot be triggered
    // when the bottom sheet is open.
    // TODO(crbug.com/427597144): Replace with CHECK(!is_prompt_showing_) in
    // MaybeShowPixAccountLinkingPrompt after M144.
    base::debug::DumpWithoutCrashing();
    client_->DismissPrompt();
  }
  is_prompt_showing_ = false;
  weak_ptr_factory_.InvalidateWeakPtrs();
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
  is_prompt_showing_ = true;
  client_->ShowPixAccountLinkingPrompt(
      base::BindOnce(&PixAccountLinkingManager::OnAccepted,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PixAccountLinkingManager::OnDeclined,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PixAccountLinkingManager::DismissPrompt() {
  if (!is_prompt_showing_) {
    return;
  }
  is_prompt_showing_ = false;
  client_->DismissPrompt();
}

void PixAccountLinkingManager::OnAccepted() {
  // TODO(crbug.com/419108993): Add metrics.
  DismissPrompt();
  client_->GetDeviceDelegate()->LaunchPixAccountLinkingPage();
}

void PixAccountLinkingManager::OnDeclined() {
  // TODO(crbug.com/419108993): Add metrics.
  DismissPrompt();
  client_->GetPaymentsDataManager()
      ->SetFacilitatedPaymentsPixAccountLinkingUserPref(/* enabled= */ false);
}

void PixAccountLinkingManager::OnUiScreenEvent(UiEvent ui_event_type) {
  switch (ui_event_type) {
    case UiEvent::kNewScreenShown: {
      CHECK(is_prompt_showing_);
      // TODO(crbug.com/419108993): Add specific logging for Pix Account Linking
      // prompt shown.
      break;
    }
    case UiEvent::kScreenCouldNotBeShown: {
      CHECK(is_prompt_showing_);
      // TODO(crbug.com/419108993): Log that the prompt show failed.
      is_prompt_showing_ = false;
      break;
    }
    case UiEvent::kScreenClosedNotByUser: {
      if (is_prompt_showing_) {
        // TODO(crbug.com/419108993): Log that the prompt was closed
        // unexpectedly.
      }
      // TODO(crbug.com/419108993): Add specific logging for Pix Account Linking
      // prompt closed not by user.
      is_prompt_showing_ = false;
      break;
    }
    case UiEvent::kScreenClosedByUser: {
      CHECK(is_prompt_showing_);
      // TODO(crbug.com/419108993): Add specific logging for Pix Account Linking
      // prompt closed by user.
      is_prompt_showing_ = false;
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
