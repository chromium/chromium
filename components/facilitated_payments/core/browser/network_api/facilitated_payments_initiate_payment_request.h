// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_FACILITATED_PAYMENTS_INITIATE_PAYMENT_REQUEST_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_FACILITATED_PAYMENTS_INITIATE_PAYMENT_REQUEST_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_request_details.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_response_details.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_network_interface.h"

namespace payments::facilitated {

// This class is used for making a payment request to the Payments server. It is
// used by all FOPs under Facilitated Payments. It encapsulates the info
// required for making the server call, and pipes the server response back to
// the `FacilitatedPaymentsManager` through a callback.
class FacilitatedPaymentsInitiatePaymentRequest
    : public autofill::payments::PaymentsRequest {
 public:
  FacilitatedPaymentsInitiatePaymentRequest(
      std::unique_ptr<FacilitatedPaymentsInitiatePaymentRequestDetails>
          request_details,
      FacilitatedPaymentsNetworkInterface::InitiatePaymentResponseCallback
          response_callback,
      const std::string& app_locale,
      const bool full_sync_enabled);
  FacilitatedPaymentsInitiatePaymentRequest(
      const FacilitatedPaymentsInitiatePaymentRequest&) = delete;
  FacilitatedPaymentsInitiatePaymentRequest& operator=(
      const FacilitatedPaymentsInitiatePaymentRequest&) = delete;
  ~FacilitatedPaymentsInitiatePaymentRequest() override;

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::Value::Dict& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result)
      override;

 private:
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsInitiatePaymentRequestTest,
                           ParseResponse_WithActionToken);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsInitiatePaymentRequestTest,
                           ParseResponse_WithCorruptActionToken);
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsInitiatePaymentRequestTest,
                           ParseResponse_WithErrorMessage);

  std::unique_ptr<FacilitatedPaymentsInitiatePaymentRequestDetails>
      request_details_;
  std::unique_ptr<FacilitatedPaymentsInitiatePaymentResponseDetails>
      response_details_;
  FacilitatedPaymentsNetworkInterface::InitiatePaymentResponseCallback
      response_callback_;
  std::string app_locale_;
  const bool full_sync_enabled_;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_FACILITATED_PAYMENTS_INITIATE_PAYMENT_REQUEST_H_
