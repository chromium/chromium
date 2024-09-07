// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/iban_access_manager.h"

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
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

void IbanAccessManager::FetchValue(const Suggestion::BackendId& backend_id,
                                   OnIbanFetchedCallback on_iban_fetched) {
  if (auto* form_data_importer = client_->GetFormDataImporter()) {
    // Reset the variable in FormDataImporter that denotes if non-interactive
    // authentication happened. This variable will be set to a value if a
    // payments autofill non-interactive flow successfully completes.
    form_data_importer
        ->SetPaymentMethodTypeIfNonInteractiveAuthenticationFlowCompleted(
            std::nullopt);
  }

  // If `Guid` has a value then that means that it's a local IBAN suggestion.
  // In this case, retrieving the complete IBAN value requires accessing the
  // saved IBAN from the PersonalDataManager.
  if (const Suggestion::Guid* guid =
          absl::get_if<Suggestion::Guid>(&backend_id)) {
    const Iban* iban = client_->GetPersonalDataManager()
                           ->payments_data_manager()
                           .GetIbanByGUID(guid->value());
    if (iban) {
      Iban iban_copy = *iban;
      client_->GetPersonalDataManager()
          ->payments_data_manager()
          .RecordUseOfIban(iban_copy);
      if (client_->GetPersonalDataManager()
              ->payments_data_manager()
              .IsPaymentMethodsMandatoryReauthEnabled()) {
        StartDeviceAuthenticationForFilling(
            std::move(on_iban_fetched), iban_copy.value(),
            NonInteractivePaymentMethodType::kLocalIban);
      } else {
        std::move(on_iban_fetched).Run(iban_copy.value());
        if (auto* form_data_importer = client_->GetFormDataImporter()) {
          form_data_importer
              ->SetPaymentMethodTypeIfNonInteractiveAuthenticationFlowCompleted(
                  payments::MandatoryReauthManager::
                      GetNonInteractivePaymentMethodType(
                          Iban::RecordType::kLocalIban));
        }
      }
    }
    return;
  }

  int64_t instrument_id =
      absl::get<Suggestion::InstrumentId>(backend_id).value();

  // The suggestion is now presumed to be a masked server IBAN.
  // If there are no server IBANs in the PersonalDataManager that have the same
  // instrument ID as the provided BackendId, then abort the operation.
  if (!client_->GetPersonalDataManager()
           ->payments_data_manager()
           .GetIbanByInstrumentId(instrument_id)) {
    return;
  }

  client_->GetPaymentsAutofillClient()->ShowAutofillProgressDialog(
      AutofillProgressDialogType::kServerIbanUnmaskProgressDialog,
      base::BindOnce(&IbanAccessManager::OnServerIbanUnmaskCancelled,
                     weak_ptr_factory_.GetWeakPtr()));

  // Construct `UnmaskIbanRequestDetails` and send `UnmaskIban` to fetch the
  // full value of the server IBAN.
  const Iban* iban = client_->GetPersonalDataManager()
                         ->payments_data_manager()
                         .GetIbanByInstrumentId(instrument_id);
  if (!iban) {
    return;
  }
  Iban iban_copy = *iban;
  client_->GetPersonalDataManager()->payments_data_manager().RecordUseOfIban(
      iban_copy);
  payments::PaymentsNetworkInterface::UnmaskIbanRequestDetails request_details;
  request_details.billable_service_number =
      payments::kUnmaskPaymentMethodBillableServiceNumber;
  request_details.billing_customer_number = payments::GetBillingCustomerId(
      &client_->GetPersonalDataManager()->payments_data_manager());
  request_details.instrument_id = instrument_id;
  base::TimeTicks unmask_request_timestamp = base::TimeTicks::Now();
  client_->GetPaymentsAutofillClient()
      ->GetPaymentsNetworkInterface()
      ->UnmaskIban(
          request_details,
          base::BindOnce(&IbanAccessManager::OnUnmaskResponseReceived,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(on_iban_fetched), unmask_request_timestamp));
}

void IbanAccessManager::OnUnmaskResponseReceived(
    OnIbanFetchedCallback on_iban_fetched,
    base::TimeTicks unmask_request_timestamp,
    payments::PaymentsAutofillClient::PaymentsRpcResult result,
    const std::u16string& value) {
  bool is_successful =
      result == payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess;
  autofill_metrics::LogServerIbanUnmaskLatency(
      base::TimeTicks::Now() - unmask_request_timestamp, is_successful);
  autofill_metrics::LogServerIbanUnmaskStatus(is_successful);
  if (is_successful) {
    if (client_->GetPersonalDataManager()
            ->payments_data_manager()
            .IsPaymentMethodsMandatoryReauthEnabled()) {
      // On some operating systems (for example, macOS and Windows), the
      // device authentication prompt freezes Chrome. Thus we can only trigger
      // the prompt after the progress dialog has been closed, which we can do
      // by using the `no_interactive_authentication_callback` parameter in
      // `PaymentsAutofillClient::CloseAutofillProgressDialog()`.
      client_->GetPaymentsAutofillClient()->CloseAutofillProgressDialog(
          /*show_confirmation_before_closing=*/false,
          /*no_interactive_authentication_callback=*/base::BindOnce(
              // `StartDeviceAuthenticationForFilling()` will asynchronously
              // trigger the re-authentication flow, so we should avoid
              // calling `Reset()` until the re-authentication flow is
              // complete.
              &IbanAccessManager::StartDeviceAuthenticationForFilling,
              weak_ptr_factory_.GetWeakPtr(), std::move(on_iban_fetched), value,
              NonInteractivePaymentMethodType::kServerIban));
    } else {
      client_->GetPaymentsAutofillClient()->CloseAutofillProgressDialog(
          /*show_confirmation_before_closing=*/false,
          /*no_interactive_authentication_callback=*/base::OnceClosure());
      std::move(on_iban_fetched).Run(value);
      if (auto* form_data_importer = client_->GetFormDataImporter()) {
        form_data_importer
            ->SetPaymentMethodTypeIfNonInteractiveAuthenticationFlowCompleted(
                payments::MandatoryReauthManager::
                    GetNonInteractivePaymentMethodType(
                        Iban::RecordType::kServerIban));
      }
    }
    return;
  }
  AutofillErrorDialogContext error_context;
  error_context.type =
      AutofillErrorDialogType::kMaskedServerIbanUnmaskingTemporaryError;
  client_->GetPaymentsAutofillClient()->ShowAutofillErrorDialog(error_context);
}

void IbanAccessManager::OnServerIbanUnmaskCancelled() {
  // TODO(crbug.com/296651899): Log the cancel metrics.
}

void IbanAccessManager::StartDeviceAuthenticationForFilling(
    OnIbanFetchedCallback on_iban_fetched,
    const std::u16string& value,
    NonInteractivePaymentMethodType non_interactive_payment_method_type) {
  client_->GetPaymentsAutofillClient()
      ->GetOrCreatePaymentsMandatoryReauthManager()
      ->StartDeviceAuthentication(
          non_interactive_payment_method_type,
          base::BindOnce(
              &IbanAccessManager::OnDeviceAuthenticationResponseForFilling,
              weak_ptr_factory_.GetWeakPtr(), std::move(on_iban_fetched), value,
              non_interactive_payment_method_type,
              client_->GetPaymentsAutofillClient()
                  ->GetOrCreatePaymentsMandatoryReauthManager()
                  ->GetAuthenticationMethod()));
}

void IbanAccessManager::OnDeviceAuthenticationResponseForFilling(
    OnIbanFetchedCallback on_iban_fetched,
    const std::u16string& value,
    NonInteractivePaymentMethodType non_interactive_payment_method_type,
    payments::MandatoryReauthAuthenticationMethod authentication_method,
    bool successful_auth) {
  LogMandatoryReauthCheckoutFlowUsageEvent(
      non_interactive_payment_method_type, authentication_method,
      successful_auth
          ? autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
                kFlowSucceeded
          : autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
                kFlowFailed);
  if (successful_auth) {
    std::move(on_iban_fetched).Run(value);
  }
}

}  // namespace autofill
