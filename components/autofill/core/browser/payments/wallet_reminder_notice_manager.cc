// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/wallet_reminder_notice_manager.h"

#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"
#include "components/autofill/core/browser/payments/payments_util.h"
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
  GetWalletReminderNoticeRequestDetails request_details;
  request_details.app_locale = client_->GetAppLocale();
  request_details.billing_customer_number = GetBillingCustomerId(
      GetPaymentsAutofillClient().GetPaymentsDataManager());
  request_details.billable_service_number =
      kUnmaskPaymentMethodBillableServiceNumber;

  GetPaymentsNetworkInterface().GetWalletReminderNotice(
      request_details,
      base::BindOnce(
          &WalletReminderNoticeManager::OnGetWalletReminderNoticeResponse,
          weak_ptr_factory_.GetWeakPtr()));
}

void WalletReminderNoticeManager::OnGetWalletReminderNoticeResponse(
    PaymentsAutofillClient::PaymentsRpcResult result,
    const GetWalletReminderNoticeResponseDetails& response_details) {
  if (result != PaymentsAutofillClient::PaymentsRpcResult::kSuccess) {
    // TODO(crbug.com/549251808): Log network or server error as a reason why
    // the reminder notice wasn't shown.
    return;
  }
  if (response_details.has_user_been_shown_reminder) {
    return;
  }

  CHECK(!response_details.legal_message_lines.empty());
  CHECK(!response_details.acknowledgement_token.empty());
  CHECK_DEREF(GetPaymentsAutofillClient().GetWalletReminderNoticeUiDelegate())
      .ShowWalletReminderNotice(response_details.legal_message_lines);

  // Notify the Google Payments server that the user has been shown the Wallet
  // reminder notice.
  RecordLegalReminderAcknowledgmentRequestDetails request_details;
  request_details.app_locale = client_->GetAppLocale();
  request_details.billing_customer_number = GetBillingCustomerId(
      GetPaymentsAutofillClient().GetPaymentsDataManager());
  request_details.billable_service_number =
      kUnmaskPaymentMethodBillableServiceNumber;
  request_details.legal_message_token = response_details.acknowledgement_token;
  request_details.flow_type = RecordLegalReminderAcknowledgmentRequestDetails::
      FlowType::kChromeDownstream;

  GetPaymentsNetworkInterface().RecordLegalReminderAcknowledgment(
      request_details,
      base::BindOnce(&WalletReminderNoticeManager::
                         OnRecordLegalReminderAcknowledgmentResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WalletReminderNoticeManager::OnRecordLegalReminderAcknowledgmentResponse(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  // We only record that the reminder notice was shown if the acknowledgment was
  // successfully recorded. If we didn't record their acknowledgment, we should
  // show the user the reminder notice again.
  if (result == PaymentsAutofillClient::PaymentsRpcResult::kSuccess) {
    prefs::SetHasShownWalletReminderNotice(client_->GetPrefs());
  }
}

}  // namespace autofill::payments
