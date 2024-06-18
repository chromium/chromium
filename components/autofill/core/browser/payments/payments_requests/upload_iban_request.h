// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_UPLOAD_IBAN_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_UPLOAD_IBAN_REQUEST_H_

#include <string>

#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill::payments {

class UploadIbanRequest : public PaymentsRequest {
 public:
  UploadIbanRequest(
      const PaymentsNetworkInterface::UploadIbanRequestDetails& details,
      bool full_sync_enabled,
      base::OnceCallback<
          void(payments::PaymentsAutofillClient::PaymentsRpcResult)> callback);
  UploadIbanRequest(const UploadIbanRequest&) = delete;
  UploadIbanRequest& operator=(const UploadIbanRequest&) = delete;
  ~UploadIbanRequest() override;

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::Value::Dict& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(
      payments::PaymentsAutofillClient::PaymentsRpcResult result) override;

 private:
  const PaymentsNetworkInterface::UploadIbanRequestDetails request_details_;
  // True when the user is both signed-in and has enabled sync.
  const bool full_sync_enabled_;
  base::OnceCallback<void(payments::PaymentsAutofillClient::PaymentsRpcResult)>
      callback_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_UPLOAD_IBAN_REQUEST_H_
