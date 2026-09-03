// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/wallet_reminder_notice_manager.h"

#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/metrics/payments/wallet_reminder_notice_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/ui/payments/wallet_reminder_notice_ui_delegate.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"

namespace autofill::payments {

namespace {

int GetBillableServiceNumber(
    RecordLegalReminderAcknowledgmentRequestDetails::FlowType flow_type) {
  switch (flow_type) {
    case RecordLegalReminderAcknowledgmentRequestDetails::FlowType::
        kWalletPass:
      return kWalletPassBillableServiceNumber;
    case RecordLegalReminderAcknowledgmentRequestDetails::FlowType::
        kChromeDownstream:
      return kUnmaskPaymentMethodBillableServiceNumber;
    case RecordLegalReminderAcknowledgmentRequestDetails::FlowType::kUnknown:
      NOTREACHED();
  }
}

}  // namespace

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
          CreditCard::RecordType::kMaskedServerCard &&
      extracted_card.record_type() != CreditCard::RecordType::kVirtualCard) {
    return false;
  }
  if (prefs::HasShownWalletReminderNotice(client_->GetPrefs())) {
    autofill_metrics::LogWalletReminderNoticeShowResult(
        autofill_metrics::WalletReminderNoticeShowResult::
            kNotShownAlreadyAcknowledgedAccordingToPref);
    return false;
  }
  return true;
}

bool WalletReminderNoticeManager::IsWalletReminderNoticeEligible(
    const EntityInstance& entity_instance) {
  if (!base::FeatureList::IsEnabled(
          autofill::features::
              kAutofillEnableWalletReminderNoticePublicPass)) {
    return false;
  }
  // The notice applies to any entity that is a public pass upstreamed to
  // wallet (e.g., Vehicles) and is not read-only. Note: The entity's
  // `record_type` is determined based on Wallet sync permissions, so checking
  // for `kPublic` here safely encapsulates both the type and permission checks.
  if (GetWalletPassType(entity_instance.type(),
                        entity_instance.record_type()) !=
          EntityInstance::WalletPassType::kPublic ||
      entity_instance.are_attributes_read_only()) {
    return false;
  }
  if (prefs::HasShownWalletReminderNotice(client_->GetPrefs())) {
    autofill_metrics::LogWalletReminderNoticeShowResult(
        autofill_metrics::WalletReminderNoticeShowResult::
            kNotShownAlreadyAcknowledgedAccordingToPref);
    return false;
  }
  return true;
}

void WalletReminderNoticeManager::ShowWalletReminderNotice(FlowType flow_type) {
  GetWalletReminderNoticeRequestDetails request_details;
  request_details.app_locale = client_->GetAppLocale();
  request_details.billing_customer_number = GetBillingCustomerId(
      GetPaymentsAutofillClient().GetPaymentsDataManager());
  request_details.billable_service_number = GetBillableServiceNumber(flow_type);

  GetPaymentsNetworkInterface().GetWalletReminderNotice(
      request_details,
      base::BindOnce(
          &WalletReminderNoticeManager::OnGetWalletReminderNoticeResponse,
          weak_ptr_factory_.GetWeakPtr(), flow_type));
}

void WalletReminderNoticeManager::OnGetWalletReminderNoticeResponse(
    FlowType flow_type,
    PaymentsAutofillClient::PaymentsRpcResult result,
    const GetWalletReminderNoticeResponseDetails& response_details) {
  if (result != PaymentsAutofillClient::PaymentsRpcResult::kSuccess) {
    autofill_metrics::LogWalletReminderNoticeShowResult(
        autofill_metrics::WalletReminderNoticeShowResult::
            kNotShownNetworkOrServerError);
    return;
  }
  if (response_details.has_user_been_shown_reminder) {
    autofill_metrics::LogWalletReminderNoticeShowResult(
        autofill_metrics::WalletReminderNoticeShowResult::
            kNotShownAlreadyAcknowledgedAccordingToServer);
    return;
  }

  CHECK(!response_details.legal_message_lines.empty());
  CHECK(!response_details.acknowledgement_token.empty());
  CHECK_DEREF(GetPaymentsAutofillClient().GetWalletReminderNoticeUiDelegate())
      .ShowWalletReminderNotice(response_details.legal_message_lines);
  autofill_metrics::LogWalletReminderNoticeShowResult(
      autofill_metrics::WalletReminderNoticeShowResult::kShown);

  // Notify the Google Payments server that the user has been shown the Wallet
  // reminder notice.
  RecordLegalReminderAcknowledgmentRequestDetails request_details;
  request_details.app_locale = client_->GetAppLocale();
  request_details.billing_customer_number = GetBillingCustomerId(
      GetPaymentsAutofillClient().GetPaymentsDataManager());
  request_details.billable_service_number = GetBillableServiceNumber(flow_type);
  request_details.legal_message_token = response_details.acknowledgement_token;
  request_details.flow_type = flow_type;

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
