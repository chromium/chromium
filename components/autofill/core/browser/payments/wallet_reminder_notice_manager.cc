// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/wallet_reminder_notice_manager.h"

#include <utility>

#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/wallet_reminder_notice_ui_delegate.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"

namespace autofill::payments {

WalletReminderNoticeManager::WalletReminderNoticeManager(AutofillClient* client)
    : client_(*client) {}

WalletReminderNoticeManager::~WalletReminderNoticeManager() = default;

bool WalletReminderNoticeManager::IsWalletReminderNoticeEligible(
    const CreditCard& extracted_card) {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableWalletReminderNotice)) {
    return false;
  }
  if (extracted_card.record_type() !=
      CreditCard::RecordType::kMaskedServerCard) {
    return false;
  }
  if (prefs::HasShownWalletReminderNotice(client_->GetPrefs())) {
    return false;
  }
  return true;
}

void WalletReminderNoticeManager::ShowWalletReminderNotice() {
  // TODO(crbug.com/540389575): Issue the `GetWalletReminderNotice` RPC
  // asynchronously via `PaymentsNetworkInterface` once the endpoint is added.
  // The request is non-blocking; UI display and acknowledgment reporting will
  // be triggered in `OnGetWalletReminderNoticeResponse` when the response
  // arrives.
}

void WalletReminderNoticeManager::OnGetWalletReminderNoticeResponse(
    const LegalMessageLines& legal_message_lines,
    const std::string& acknowledgement_token,
    bool has_user_acknowledged) {
  // TODO(crbug.com/540389575): Check HasShownWalletReminderNotice(~) before
  // showing the reminder notice and call SetHasShownWalletReminderNotice(~)
  // when `has_user_acknowledged` is `true`.
  if (has_user_acknowledged) {
    return;
  }

  if (legal_message_lines.empty() || acknowledgement_token.empty()) {
    return;
  }

  if (PaymentsAutofillClient* payments_client =
          client_->GetPaymentsAutofillClient()) {
    if (WalletReminderNoticeUiDelegate* ui_delegate =
            payments_client->GetWalletReminderNoticeUiDelegate()) {
      ui_delegate->ShowWalletReminderNotice(legal_message_lines);
    }
  }

  // TODO(crbug.com/540389575): Dispatch fire-and-forget
  // ReportWalletReminderNoticeAcknowledged(acknowledgement_token) via
  // PaymentsNetworkInterface.
}

}  // namespace autofill::payments
