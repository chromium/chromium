// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_CREATE_CARD_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_CREATE_CARD_REQUEST_H_

#include <vector>

#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill::payments {

class GetDetailsForCreateCardRequest : public PaymentsRequest {
 public:
  GetDetailsForCreateCardRequest(
      const std::string& unique_country_code,
      const std::vector<ClientBehaviorConstants>& client_behavior_signals,
      const std::string& app_locale,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const std::u16string&,
                              std::unique_ptr<base::Value::Dict>,
                              std::vector<std::pair<int, int>>)> callback,
      const int billable_service_number,
      const int64_t billing_customer_number,
      UploadCardSource upload_card_source);
  GetDetailsForCreateCardRequest(const GetDetailsForCreateCardRequest&) =
      delete;
  GetDetailsForCreateCardRequest& operator=(
      const GetDetailsForCreateCardRequest&) = delete;
  ~GetDetailsForCreateCardRequest() override;

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::Value::Dict& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(
      PaymentsAutofillClient::PaymentsRpcResult result) override;

 private:
  friend class GetDetailsForCreateCardTestApi;

  const std::string unique_country_code_;
  const std::vector<ClientBehaviorConstants> client_behavior_signals_;
  const std::string app_locale_;
  base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                          const std::u16string&,
                          std::unique_ptr<base::Value::Dict>,
                          std::vector<std::pair<int, int>>)>
      callback_;
  const int billable_service_number_;
  const UploadCardSource upload_card_source_;
  const int64_t billing_customer_number_;

  std::u16string context_token_;
  std::unique_ptr<base::Value::Dict> legal_message_;
  std::vector<std::pair<int, int>> supported_card_bin_ranges_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_CREATE_CARD_REQUEST_H_
