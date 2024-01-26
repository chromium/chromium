// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/iban_access_manager.h"

#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
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
                                   OnIbanFetchedCallback on_iban_fetched) {
  // If `Guid` has a value then that means that it's a local IBAN suggestion.
  // In this case, retrieving the complete IBAN value requires accessing the
  // saved IBAN from the PersonalDataManager.
  Suggestion::BackendId backend_id =
      suggestion.GetPayload<Suggestion::BackendId>();
  if (Suggestion::Guid* guid = absl::get_if<Suggestion::Guid>(&backend_id)) {
    const Iban* iban =
        client_->GetPersonalDataManager()->GetIbanByGUID(guid->value());
    if (iban) {
      Iban copy_iban = *iban;
      std::move(on_iban_fetched).Run(copy_iban.value());
      client_->GetPersonalDataManager()->RecordUseOfIban(copy_iban);
    }
    return;
  }

  int64_t instrument_id =
      absl::get<Suggestion::InstrumentId>(backend_id).value();

  // The suggestion is now presumed to be a masked server IBAN.
  // If there are no server IBANs in the PersonalDataManager that have the same
  // instrument ID as the provided BackendId, then abort the operation.
  if (!client_->GetPersonalDataManager()->GetIbanByInstrumentId(
          instrument_id)) {
    return;
  }

  client_->ShowAutofillProgressDialog(
      AutofillProgressDialogType::kServerIbanUnmaskProgressDialog,
      base::BindOnce(&IbanAccessManager::OnServerIbanUnmaskCancelled,
                     weak_ptr_factory_.GetWeakPtr()));

  // Construct `UnmaskIbanRequestDetails` and send `UnmaskIban` to fetch the
  // full value of the server IBAN.
  const Iban* iban =
      client_->GetPersonalDataManager()->GetIbanByInstrumentId(instrument_id);
  if (!iban) {
    return;
  }
  Iban copy_iban = *iban;
  client_->GetPersonalDataManager()->RecordUseOfIban(copy_iban);
  payments::PaymentsNetworkInterface::UnmaskIbanRequestDetails request_details;
  request_details.billable_service_number =
      payments::kUnmaskPaymentMethodBillableServiceNumber;
  request_details.billing_customer_number =
      payments::GetBillingCustomerId(client_->GetPersonalDataManager());
  request_details.instrument_id = instrument_id;
  base::TimeTicks unmask_request_timestamp = base::TimeTicks::Now();
  client_->GetPaymentsNetworkInterface()->UnmaskIban(
      request_details,
      base::BindOnce(&IbanAccessManager::OnUnmaskResponseReceived,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_iban_fetched),
                     unmask_request_timestamp));
}

void IbanAccessManager::OnUnmaskResponseReceived(
    OnIbanFetchedCallback on_iban_fetched,
    base::TimeTicks unmask_request_timestamp,
    AutofillClient::PaymentsRpcResult result,
    const std::u16string& value) {
  client_->CloseAutofillProgressDialog(
      /*show_confirmation_before_closing=*/false);
  bool is_successful = result == AutofillClient::PaymentsRpcResult::kSuccess;
  autofill_metrics::LogServerIbanUnmaskLatency(
      base::TimeTicks::Now() - unmask_request_timestamp, is_successful);
  autofill_metrics::LogServerIbanUnmaskStatus(is_successful);
  if (is_successful) {
    std::move(on_iban_fetched).Run(value);
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
