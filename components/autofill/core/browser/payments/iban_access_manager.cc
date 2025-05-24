// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/iban_access_manager.h"

#include <variant>

#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

IbanAccessManager::IbanAccessManager(AutofillClient* client)
    : client_(client) {}

IbanAccessManager::~IbanAccessManager() = default;

void IbanAccessManager::FetchValue(const Suggestion::Payload& payload,
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
  if (const Suggestion::Guid* guid = std::get_if<Suggestion::Guid>(&payload)) {
    const Iban* iban = GetPaymentsDataManager().GetIbanByGUID(guid->value());
    if (iban) {
      Iban iban_copy = *iban;
      GetPaymentsDataManager().RecordUseOfIban(iban_copy);
      if (GetPaymentsDataManager().IsPaymentMethodsMandatoryReauthEnabled()) {
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

  int64_t instrument_id = std::get<Suggestion::InstrumentId>(payload).value();

  // The suggestion is now presumed to be a masked server IBAN.
  // If there are no server IBANs in the PersonalDataManager that have the same
  // instrument ID as the provided `instrument_id`, then abort the operation.
  if (!GetPaymentsDataManager().GetIbanByInstrumentId(instrument_id)) {
    return;
  }

  GetPaymentsAutofillClient().ShowAutofillProgressDialog(
      AutofillProgressDialogType::kServerIbanUnmaskProgressDialog,
      base::BindOnce(&IbanAccessManager::OnServerIbanUnmaskCancelled,
                     weak_ptr_factory_.GetWeakPtr()));

  // Construct `UnmaskIbanRequestDetails` and send `UnmaskIban` to fetch the
  // full value of the server IBAN.
  const Iban* iban =
      GetPaymentsDataManager().GetIbanByInstrumentId(instrument_id);
  if (!iban) {
    return;
  }
  Iban iban_copy = *iban;
  GetPaymentsDataManager().RecordUseOfIban(iban_copy);
  payments::UnmaskIbanRequestDetails request_details;
  request_details.billing_customer_number =
      payments::GetBillingCustomerId(GetPaymentsDataManager());
  request_details.instrument_id = instrument_id;
  base::TimeTicks unmask_request_timestamp = base::TimeTicks::Now();
  GetPaymentsAutofillClient().GetPaymentsNetworkInterface()->UnmaskIban(
      request_details,
      base::BindOnce(&IbanAccessManager::OnUnmaskResponseReceived,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_iban_fetched),
                     unmask_request_timestamp));
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
    if (GetPaymentsDataManager().IsPaymentMethodsMandatoryReauthEnabled()) {
      // On some operating systems (for example, macOS and Windows), the
      // device authentication prompt freezes Chrome. Thus we can only trigger
      // the prompt after the progress dialog has been closed, which we can do
      // by using the `no_interactive_authentication_callback` parameter in
      // `PaymentsAutofillClient::CloseAutofillProgressDialog()`.
      GetPaymentsAutofillClient().CloseAutofillProgressDialog(
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
      GetPaymentsAutofillClient().CloseAutofillProgressDialog(
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

  // Immediately close the progress dialog before showing the error dialog.
  GetPaymentsAutofillClient().CloseAutofillProgressDialog(
      /*show_confirmation_before_closing=*/false,
      /*no_interactive_authentication_callback=*/base::OnceClosure());
  AutofillErrorDialogContext error_context;
  error_context.type =
      AutofillErrorDialogType::kMaskedServerIbanUnmaskingTemporaryError;
  GetPaymentsAutofillClient().ShowAutofillErrorDialog(error_context);
}

void IbanAccessManager::OnServerIbanUnmaskCancelled() {
  // TODO(crbug.com/296651899): Log the cancel metrics.
}

void IbanAccessManager::StartDeviceAuthenticationForFilling(
    OnIbanFetchedCallback on_iban_fetched,
    const std::u16string& value,
    NonInteractivePaymentMethodType non_interactive_payment_method_type) {
  GetPaymentsAutofillClient()
      .GetOrCreatePaymentsMandatoryReauthManager()
      ->StartDeviceAuthentication(
          non_interactive_payment_method_type,
          base::BindOnce(
              &IbanAccessManager::OnDeviceAuthenticationResponseForFilling,
              weak_ptr_factory_.GetWeakPtr(), std::move(on_iban_fetched), value,
              non_interactive_payment_method_type,
              GetPaymentsAutofillClient()
                  .GetOrCreatePaymentsMandatoryReauthManager()
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

payments::PaymentsAutofillClient&
IbanAccessManager::GetPaymentsAutofillClient() {
  return *client_->GetPaymentsAutofillClient();
}

PaymentsDataManager& IbanAccessManager::GetPaymentsDataManager() {
  return client_->GetPersonalDataManager().payments_data_manager();
}

}  // namespace autofill
