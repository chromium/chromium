// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/network_api/multiple_request_facilitated_payments_network_interface.h"

#include "components/autofill/core/browser/payments/account_info_getter.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_request.h"

namespace payments::facilitated {

MultipleRequestFacilitatedPaymentsNetworkInterface::
    MultipleRequestFacilitatedPaymentsNetworkInterface(
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        signin::IdentityManager& identity_manager,
        autofill::AccountInfoGetter& account_info_getter,
        bool is_off_the_record)
    : autofill::payments::MultipleRequestPaymentsNetworkInterfaceBase(
          url_loader_factory,
          identity_manager,
          is_off_the_record),
      account_info_getter_(account_info_getter) {}

MultipleRequestFacilitatedPaymentsNetworkInterface::
    ~MultipleRequestFacilitatedPaymentsNetworkInterface() = default;

MultipleRequestFacilitatedPaymentsNetworkInterface::RequestId
MultipleRequestFacilitatedPaymentsNetworkInterface::InitiatePayment(
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

}  // namespace payments::facilitated
