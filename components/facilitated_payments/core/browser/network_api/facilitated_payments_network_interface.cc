// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_network_interface.h"

#include "components/autofill/core/browser/payments/account_info_getter.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_request.h"
#include "components/facilitated_payments/core/browser/network_api/get_details_for_pix_account_linking_request.h"

namespace payments::facilitated {

FacilitatedPaymentsNetworkInterface::FacilitatedPaymentsNetworkInterface(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager& identity_manager,
    autofill::AccountInfoGetter& account_info_getter,
    bool is_off_the_record)
    : autofill::payments::MultipleRequestPaymentsNetworkInterfaceBase(
          url_loader_factory,
          identity_manager,
          is_off_the_record),
      account_info_getter_(account_info_getter) {}

FacilitatedPaymentsNetworkInterface::~FacilitatedPaymentsNetworkInterface() =
    default;

FacilitatedPaymentsNetworkInterface::RequestId
FacilitatedPaymentsNetworkInterface::InitiatePayment(
    std::unique_ptr<FacilitatedPaymentsInitiatePaymentRequestDetails>
        request_details,
    InitiatePaymentResponseCallback response_callback,
    const std::string& app_locale) {
  return IssueRequest(
      std::make_unique<FacilitatedPaymentsInitiatePaymentRequest>(
          std::move(request_details), std::move(response_callback), app_locale,
          account_info_getter_
              ->IsSyncFeatureEnabledForPaymentsServerMetrics()));
}

FacilitatedPaymentsNetworkInterface::RequestId
FacilitatedPaymentsNetworkInterface::GetDetailsForCreatePaymentInstrument(
    int64_t billing_customer_number,
    GetDetailsForCreatePaymentInstrumentResponseCallback response_callback,
    const std::string& app_locale) {
  return IssueRequest(std::make_unique<GetDetailsForPixAccountLinkingRequest>(
      billing_customer_number, std::move(response_callback), app_locale,
      account_info_getter_->IsSyncFeatureEnabledForPaymentsServerMetrics()));
}

}  // namespace payments::facilitated
