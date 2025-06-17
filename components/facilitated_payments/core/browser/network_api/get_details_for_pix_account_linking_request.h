// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_GET_DETAILS_FOR_PIX_ACCOUNT_LINKING_REQUEST_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_GET_DETAILS_FOR_PIX_ACCOUNT_LINKING_REQUEST_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_network_interface.h"

namespace payments::facilitated {

// This class is used for making a request to the Payments server to check
// whether the user is eligible for Pix account linking.
class GetDetailsForPixAccountLinkingRequest
    : public autofill::payments::PaymentsRequest {
 public:
  GetDetailsForPixAccountLinkingRequest(
      const int64_t billing_customer_number,
      base::OnceCallback<
          void(autofill::payments::PaymentsAutofillClient::PaymentsRpcResult,
               bool)> response_callback,
      const std::string& app_locale,
      const bool full_sync_enabled);
  GetDetailsForPixAccountLinkingRequest(
      const GetDetailsForPixAccountLinkingRequest&) = delete;
  GetDetailsForPixAccountLinkingRequest& operator=(
      const GetDetailsForPixAccountLinkingRequest&) = delete;
  ~GetDetailsForPixAccountLinkingRequest() override;

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
  FRIEND_TEST_ALL_PREFIXES(
      GetDetailsForPixAccountLinkingRequestTest,
      ParseResponse_Success_AccountLinkingEligibilitySetToTrue);
  FRIEND_TEST_ALL_PREFIXES(
      GetDetailsForPixAccountLinkingRequestTest,
      ParseResponse_SuccessWithoutPixAccountLinkingDetails_AccountLinkingEligibilitySetToTrue);
  FRIEND_TEST_ALL_PREFIXES(GetDetailsForPixAccountLinkingRequestTest,
                           ParseResponseNotCalled_ResponseNotComplete);
  FRIEND_TEST_ALL_PREFIXES(GetDetailsForPixAccountLinkingRequestTest,
                           ParseResponse_Error);
  // Request properties
  const int64_t billing_customer_number_;
  base::OnceCallback<
      void(autofill::payments::PaymentsAutofillClient::PaymentsRpcResult, bool)>
      response_callback_;
  const std::string app_locale_;
  const bool full_sync_enabled_;

  // Response properties
  bool is_eligible_for_pix_account_linking_ = false;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_GET_DETAILS_FOR_PIX_ACCOUNT_LINKING_REQUEST_H_
