// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_WALLET_REMINDER_NOTICE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_WALLET_REMINDER_NOTICE_MANAGER_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"

namespace autofill {

class AutofillClient;
class CreditCard;
class EntityInstance;

namespace payments {

// Manages showing the Wallet Reminder Notice to inform users that their
// saved payment instruments or public passes are stored in Google Wallet.
class WalletReminderNoticeManager {
 public:
  using FlowType = RecordLegalReminderAcknowledgmentRequestDetails::FlowType;

  explicit WalletReminderNoticeManager(AutofillClient* client);
  WalletReminderNoticeManager(const WalletReminderNoticeManager&) = delete;
  WalletReminderNoticeManager& operator=(const WalletReminderNoticeManager&) =
      delete;
  virtual ~WalletReminderNoticeManager();

  // Checks if a user is eligible to see the Wallet reminder notice for a credit
  // card.
  bool IsWalletReminderNoticeEligible(const CreditCard& extracted_card);

  // Checks if a user is eligible to see the Wallet reminder notice for a public
  // pass.
  bool IsWalletReminderNoticeEligible(const EntityInstance& entity_instance);

  // Initiates the asynchronous flow to display the Wallet Reminder Notice by
  // issuing the GetWalletReminderNotice RPC via PaymentsNetworkInterface.
  // The request runs in the background without blocking the main thread. When
  // the server response is received, `OnGetWalletReminderNoticeResponse` is
  // invoked to process the result and trigger the UI. The caller should check
  // `IsWalletReminderNoticeEligible` before calling `ShowWalletReminderNotice`.
  // `flow_type` indicates the context used to determine the correct billable
  // service number for the RPCs.
  virtual void ShowWalletReminderNotice(FlowType flow_type);

  // Handles the response from the Payments server for the Wallet Reminder
  // Notice request.
  virtual void OnGetWalletReminderNoticeResponse(
      FlowType flow_type,
      PaymentsAutofillClient::PaymentsRpcResult result,
      const GetWalletReminderNoticeResponseDetails& response_details);

  // Handles the response from the Payments server for the Record Legal Reminder
  // Acknowledgement request.
  virtual void OnRecordLegalReminderAcknowledgmentResponse(
      PaymentsAutofillClient::PaymentsRpcResult result);

 private:
  PaymentsAutofillClient& GetPaymentsAutofillClient() {
    return *client_->GetPaymentsAutofillClient();
  }

  PaymentsNetworkInterface& GetPaymentsNetworkInterface() {
    return *GetPaymentsAutofillClient().GetPaymentsNetworkInterface();
  }

  const raw_ref<AutofillClient> client_;

  base::WeakPtrFactory<WalletReminderNoticeManager> weak_ptr_factory_{this};
};

}  // namespace payments
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_WALLET_REMINDER_NOTICE_MANAGER_H_
