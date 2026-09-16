// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_GET_DETAILS_FOR_EWALLET_ACCOUNT_LINKING_REQUEST_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_GET_DETAILS_FOR_EWALLET_ACCOUNT_LINKING_REQUEST_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_network_interface.h"

namespace payments::facilitated {

// This class is used for making a request to the Payments server to check
// whether the user is eligible for eWallet account linking.
class GetDetailsForEwalletAccountLinkingRequest
    : public autofill::payments::PaymentsRequest {
 public:
  GetDetailsForEwalletAccountLinkingRequest(
      const int64_t billing_customer_number,
      const std::vector<uint8_t>& client_token,
      base::OnceCallback<
          void(autofill::payments::PaymentsAutofillClient::PaymentsRpcResult,
               bool,
               const std::vector<uint8_t>&)> response_callback,
      const std::string& app_locale,
      const bool full_sync_enabled,
      base::DictValue account_linking_payload);
  GetDetailsForEwalletAccountLinkingRequest(
      const GetDetailsForEwalletAccountLinkingRequest&) = delete;
  GetDetailsForEwalletAccountLinkingRequest& operator=(
      const GetDetailsForEwalletAccountLinkingRequest&) = delete;
  ~GetDetailsForEwalletAccountLinkingRequest() override;

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::DictValue& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result)
      override;

 private:
  FRIEND_TEST_ALL_PREFIXES(
      GetDetailsForEwalletAccountLinkingRequestTest,
      ParseResponse_Success_AccountLinkingEligibilitySetToTrue);
  FRIEND_TEST_ALL_PREFIXES(
      GetDetailsForEwalletAccountLinkingRequestTest,
      ParseResponse_SuccessWithoutEwalletAccountLinkingDetails_AccountLinkingEligibilitySetToFalse);
  FRIEND_TEST_ALL_PREFIXES(GetDetailsForEwalletAccountLinkingRequestTest,
                           ParseResponse_SuccessWithActionToken);
  FRIEND_TEST_ALL_PREFIXES(
      GetDetailsForEwalletAccountLinkingRequestTest,
      ParseResponse_SuccessWithoutActionToken_AccountLinkingEligibilitySetToFalse);
  FRIEND_TEST_ALL_PREFIXES(GetDetailsForEwalletAccountLinkingRequestTest,
                           ParseResponse_ErrorWithDetails);
  FRIEND_TEST_ALL_PREFIXES(GetDetailsForEwalletAccountLinkingRequestTest,
                           ParseResponse_CorruptActionToken);
  FRIEND_TEST_ALL_PREFIXES(GetDetailsForEwalletAccountLinkingRequestTest,
                           ParseResponse_EmptyActionToken);
  FRIEND_TEST_ALL_PREFIXES(GetDetailsForEwalletAccountLinkingRequestTest,
                           ParseResponse_Error);
  // Request properties
  const int64_t billing_customer_number_;
  const std::vector<uint8_t> client_token_;
  base::OnceCallback<void(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult,
      bool,
      const std::vector<uint8_t>&)>
      response_callback_;
  const std::string app_locale_;
  const bool full_sync_enabled_;
  base::DictValue account_linking_payload_;

  // Response properties
  bool is_eligible_for_ewallet_account_linking_ = false;
  std::vector<uint8_t> action_token_;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_GET_DETAILS_FOR_EWALLET_ACCOUNT_LINKING_REQUEST_H_
