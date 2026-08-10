// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/wallet_reminder_notice_manager.h"

#include <utility>

#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/wallet_reminder_notice_ui_delegate.h"

namespace autofill::payments {

WalletReminderNoticeManager::WalletReminderNoticeManager(AutofillClient* client)
    : client_(*client) {}

WalletReminderNoticeManager::~WalletReminderNoticeManager() = default;

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
  // TODO(crbug.com/540389575): We need to store the Gaia ID associated with
  // this Chrome profile in the PrefService and use this Gaia ID to determine
  // whether to show the reminder notice.
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
