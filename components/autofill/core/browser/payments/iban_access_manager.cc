// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/iban_access_manager.h"

#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

IbanAccessManager::IbanAccessManager(AutofillClient* client)
    : client_(client) {}

IbanAccessManager::~IbanAccessManager() = default;

void IbanAccessManager::FetchValue(const Suggestion& suggestion,
                                   base::WeakPtr<Accessor> accessor) {
  if (!accessor) {
    return;
  }

  // If `ValueToFill` has a value then that means that it's a local IBAN
  // suggestion, and the full IBAN value is known already.
  if (absl::holds_alternative<Suggestion::ValueToFill>(suggestion.payload)) {
    const std::u16string value =
        suggestion.GetPayload<Suggestion::ValueToFill>().value();
    if (!value.empty()) {
      accessor->OnIbanFetched(value);
    }
    return;
  }

  // The suggestion is now presumed to be a masked server IBAN.
  // If there are no server IBANs in the PersonalDataManager that have the same
  // instrument ID as the provided BackendId, then abort the operation.
  if (!client_->GetPersonalDataManager()->GetIbanByInstrumentId(
          suggestion.GetBackendId<Suggestion::InstrumentId>().value())) {
    return;
  }

  client_->ShowAutofillProgressDialog(
      AutofillProgressDialogType::kServerIbanUnmaskProgressDialog,
      base::BindOnce(&IbanAccessManager::OnServerIbanUnmaskCancelled,
                     weak_ptr_factory_.GetWeakPtr()));

  // Construct `UnmaskIbanRequestDetails` and send `UnmaskIban` to fetch the
  // full value of the server IBAN.
  payments::PaymentsNetworkInterface::UnmaskIbanRequestDetails request_details;
  request_details.billable_service_number =
      payments::kUnmaskPaymentMethodBillableServiceNumber;
  request_details.billing_customer_number =
      payments::GetBillingCustomerId(client_->GetPersonalDataManager());
  request_details.instrument_id =
      suggestion.GetBackendId<Suggestion::InstrumentId>().value();
  client_->GetPaymentsNetworkInterface()->UnmaskIban(
      request_details,
      base::BindOnce(&IbanAccessManager::OnUnmaskResponseReceived,
                     weak_ptr_factory_.GetWeakPtr(), accessor));
}

void IbanAccessManager::OnUnmaskResponseReceived(
    base::WeakPtr<Accessor> accessor,
    AutofillClient::PaymentsRpcResult result,
    const std::u16string& value) {
  client_->CloseAutofillProgressDialog(
      /*show_confirmation_before_closing=*/false);
  if (accessor && result == AutofillClient::PaymentsRpcResult::kSuccess &&
      !value.empty()) {
    accessor->OnIbanFetched(value);
    return;
  }
  AutofillErrorDialogContext error_context;
  error_context.type =
      AutofillErrorDialogType::kMaskedServerIbanUnmaskingTemporaryError;
  client_->ShowAutofillErrorDialog(error_context);
}

void IbanAccessManager::OnServerIbanUnmaskCancelled() {
  // TODO(b/296651899): Log the cancel metrics.
}

}  // namespace autofill
