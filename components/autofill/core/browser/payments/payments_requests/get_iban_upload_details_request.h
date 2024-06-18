// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_IBAN_UPLOAD_DETAILS_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_IBAN_UPLOAD_DETAILS_REQUEST_H_

#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill::payments {

class GetIbanUploadDetailsRequest : public PaymentsRequest {
 public:
  GetIbanUploadDetailsRequest(
      const bool full_sync_enabled,
      const std::string& app_locale,
      int64_t billing_customer_number,
      int billable_service_number,
      const std::string& country_code,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const std::u16string& validation_regex,
                              const std::u16string& context_token,
                              std::unique_ptr<base::Value::Dict>)> callback);
  GetIbanUploadDetailsRequest(const GetIbanUploadDetailsRequest&) = delete;
  GetIbanUploadDetailsRequest& operator=(const GetIbanUploadDetailsRequest&) =
      delete;
  ~GetIbanUploadDetailsRequest() override;

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::Value::Dict& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(
      PaymentsAutofillClient::PaymentsRpcResult result) override;

  std::u16string context_token_for_testing() const { return context_token_; }
  base::Value::Dict* legal_message_for_testing() const {
    return legal_message_.get();
  }

 private:
  const bool full_sync_enabled_;
  std::string app_locale_;
  std::u16string context_token_;
  std::u16string validation_regex_;
  std::unique_ptr<base::Value::Dict> legal_message_;
  const int64_t billing_customer_number_;
  const int billable_service_number_;
  std::string country_code_;
  base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                          const std::u16string& validation_regex,
                          const std::u16string& context_token,
                          std::unique_ptr<base::Value::Dict>)>
      callback_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_IBAN_UPLOAD_DETAILS_REQUEST_H_
