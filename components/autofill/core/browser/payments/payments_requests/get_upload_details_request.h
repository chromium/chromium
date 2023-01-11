// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_UPLOAD_DETAILS_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_UPLOAD_DETAILS_REQUEST_H_

#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill::payments {

class GetUploadDetailsRequest : public PaymentsRequest {
 public:
  GetUploadDetailsRequest(
      const std::vector<AutofillProfile>& addresses,
      const int detected_values,
      const std::vector<const char*>& active_experiments,
      const bool full_sync_enabled,
      const std::string& app_locale,
      base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                              const std::u16string&,
                              std::unique_ptr<base::Value::Dict>,
                              std::vector<std::pair<int, int>>)> callback,
      const int billable_service_number,
      const int64_t billing_customer_number,
      PaymentsClient::UploadCardSource upload_card_source);
  GetUploadDetailsRequest(const GetUploadDetailsRequest&) = delete;
  GetUploadDetailsRequest& operator=(const GetUploadDetailsRequest&) = delete;
  ~GetUploadDetailsRequest() override;

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::Value::Dict& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(AutofillClient::PaymentsRpcResult result) override;

 private:
  // Helper for ParseResponse(). Input format should be :"1234,30000-55555,765",
  // where ranges are separated by commas and items separated with a dash means
  // the start and ends of the range. Items without a dash have the same start
  // and end (ex. 1234-1234)
  std::vector<std::pair<int, int>> ParseSupportedCardBinRangesString(
      const std::string& supported_card_bin_ranges_string);

  const std::vector<AutofillProfile> addresses_;
  const int detected_values_;
  const std::vector<const char*> active_experiments_;
  const bool full_sync_enabled_;
  std::string app_locale_;
  base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                          const std::u16string&,
                          std::unique_ptr<base::Value::Dict>,
                          std::vector<std::pair<int, int>>)>
      callback_;
  std::u16string context_token_;
  std::unique_ptr<base::Value::Dict> legal_message_;
  std::vector<std::pair<int, int>> supported_card_bin_ranges_;
  const int billable_service_number_;
  PaymentsClient::UploadCardSource upload_card_source_;
  const int64_t billing_customer_number_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_UPLOAD_DETAILS_REQUEST_H_
