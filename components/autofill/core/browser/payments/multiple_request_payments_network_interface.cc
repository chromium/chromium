// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/multiple_request_payments_network_interface.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/create_card_request.h"
#include "components/autofill/core/browser/payments/payments_requests/get_details_for_create_card_request.h"
#include "components/autofill/core/browser/payments/payments_requests/get_details_for_enrollment_request.h"
#include "components/autofill/core/browser/payments/payments_requests/update_virtual_card_enrollment_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

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
    const UploadCardRequestDetails& details,
    GetDetailsForCreateCardCallback callback) {
  CHECK_LE(details.profiles.size(), 1U);
  std::string unique_country_code;
  // `details.profiles` contain the unique address for the user. It may be empty
  // if there is no address or multiple conflicting addresses detected.
  if (!details.profiles.empty()) {
    unique_country_code = base::UTF16ToUTF8(
        details.profiles[0].GetRawInfo(FieldType::ADDRESS_HOME_COUNTRY));
  }
  return IssueRequest(std::make_unique<GetDetailsForCreateCardRequest>(
      unique_country_code, details.client_behavior_signals, details.app_locale,
      std::move(callback), payments::kUploadPaymentMethodBillableServiceNumber,
      details.billing_customer_number, details.upload_card_source));
}

RequestId MultipleRequestPaymentsNetworkInterface::CreateCard(
    const UploadCardRequestDetails& details,
    base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                            const std::string&)> callback) {
  return IssueRequest(
      std::make_unique<CreateCardRequest>(details, std::move(callback)));
}

RequestId
MultipleRequestPaymentsNetworkInterface::GetVirtualCardEnrollmentDetails(
    const GetDetailsForEnrollmentRequestDetails& request_details,
    base::OnceCallback<void(PaymentsRpcResult,
                            const GetDetailsForEnrollmentResponseDetails&)>
        callback) {
  return IssueRequest(std::make_unique<GetDetailsForEnrollmentRequest>(
      request_details, std::move(callback)));
}

RequestId MultipleRequestPaymentsNetworkInterface::UpdateVirtualCardEnrollment(
    const UpdateVirtualCardEnrollmentRequestDetails& request_details,
    base::OnceCallback<void(PaymentsRpcResult)> callback) {
  return IssueRequest(std::make_unique<UpdateVirtualCardEnrollmentRequest>(
      request_details, std::move(callback)));
}

}  // namespace autofill::payments
