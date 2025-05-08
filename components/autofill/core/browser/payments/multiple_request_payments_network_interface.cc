// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/multiple_request_payments_network_interface.h"

#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/payments/payments_requests/create_card_request.h"
#include "components/autofill/core/browser/payments/payments_requests/get_details_for_create_card_request.h"

namespace autofill::payments {

MultipleRequestPaymentsNetworkInterface::
    MultipleRequestPaymentsNetworkInterface(
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        signin::IdentityManager& identity_manager,
        bool is_off_the_record)
    : MultipleRequestPaymentsNetworkInterfaceBase(url_loader_factory,
                                                  identity_manager,
                                                  is_off_the_record) {}

MultipleRequestPaymentsNetworkInterface::
    ~MultipleRequestPaymentsNetworkInterface() = default;

RequestId MultipleRequestPaymentsNetworkInterface::GetDetailsForCreateCard(
    const std::string& unique_country_code,
    const std::vector<ClientBehaviorConstants>& client_behavior_signals,
    const std::string& app_locale,
    GetDetailsForCreateCardCallback callback,
    const int billable_service_number,
    const int64_t billing_customer_number,
    UploadCardSource upload_card_source) {
  return IssueRequest(std::make_unique<GetDetailsForCreateCardRequest>(
      unique_country_code, client_behavior_signals, app_locale,
      std::move(callback), billable_service_number, billing_customer_number,
      upload_card_source));
}

RequestId MultipleRequestPaymentsNetworkInterface::CreateCard(
    const UploadCardRequestDetails& details,
    base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                            const std::string&)> callback) {
  return IssueRequest(
      std::make_unique<CreateCardRequest>(details, std::move(callback)));
}

}  // namespace autofill::payments
