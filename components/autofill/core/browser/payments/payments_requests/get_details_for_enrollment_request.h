// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_ENROLLMENT_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_ENROLLMENT_REQUEST_H_

#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill::payments {

// Payments request to fetch necessary information (i.e. ToS message) in
// preparation for the virtual card enrollment.
class GetDetailsForEnrollmentRequest : public PaymentsRequest {
 public:
  GetDetailsForEnrollmentRequest(
      const PaymentsNetworkInterface::GetDetailsForEnrollmentRequestDetails&
          request_details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const PaymentsNetworkInterface::
                                  GetDetailsForEnrollmentResponseDetails&)>
          callback);
  GetDetailsForEnrollmentRequest(const GetDetailsForEnrollmentRequest&) =
      delete;
  GetDetailsForEnrollmentRequest& operator=(
      const GetDetailsForEnrollmentRequest&) = delete;
  ~GetDetailsForEnrollmentRequest() override;

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::Value::Dict& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(
      PaymentsAutofillClient::PaymentsRpcResult result) override;

 private:
  friend class GetDetailsForEnrollmentRequestTest;

  // Used to store information to be populated to the request.
  PaymentsNetworkInterface::GetDetailsForEnrollmentRequestDetails
      request_details_;

  // Used to store information parsed from the response. Will be passed into the
  // |callback_| function as a param.
  PaymentsNetworkInterface::GetDetailsForEnrollmentResponseDetails
      response_details_;

  // The callback function to be invoked when the response is received.
  base::OnceCallback<void(
      PaymentsAutofillClient::PaymentsRpcResult,
      const PaymentsNetworkInterface::GetDetailsForEnrollmentResponseDetails&)>
      callback_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_ENROLLMENT_REQUEST_H_
