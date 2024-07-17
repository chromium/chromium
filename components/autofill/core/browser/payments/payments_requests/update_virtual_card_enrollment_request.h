// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_UPDATE_VIRTUAL_CARD_ENROLLMENT_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_UPDATE_VIRTUAL_CARD_ENROLLMENT_REQUEST_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace base {
class Value;
}  // namespace base

namespace autofill {
namespace payments {

// Payments request to enroll or unenroll a credit card into a virtual card.
// Virtual Card Numbers allow a user to check out online with a credit
// card using a credit card number that is different from the credit card's
// original number.
class UpdateVirtualCardEnrollmentRequest : public PaymentsRequest {
 public:
  UpdateVirtualCardEnrollmentRequest(
      const PaymentsNetworkInterface::UpdateVirtualCardEnrollmentRequestDetails&
          request_details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult)>
          callback);
  UpdateVirtualCardEnrollmentRequest(
      const UpdateVirtualCardEnrollmentRequest&) = delete;
  UpdateVirtualCardEnrollmentRequest& operator=(
      const UpdateVirtualCardEnrollmentRequest&) = delete;
  ~UpdateVirtualCardEnrollmentRequest() override;

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::Value::Dict& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(
      PaymentsAutofillClient::PaymentsRpcResult result) override;
  std::string GetHistogramName() const override;
  std::optional<base::TimeDelta> GetTimeout() const override;

 private:
  friend class UpdateVirtualCardEnrollmentRequestTest;

  // Modifies the base::Value that |request_dict| points to by setting all of
  // the fields needed for an Enroll request.
  void BuildEnrollRequestDictionary(base::Value::Dict* request_dict);

  // Modifies the base::Value that |request_dict| points to by setting all of
  // the fields needed for an Unenroll request.
  void BuildUnenrollRequestDictionary(base::Value::Dict* request_dict);

  PaymentsNetworkInterface::UpdateVirtualCardEnrollmentRequestDetails
      request_details_;
  base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult)> callback_;
  std::optional<std::string> enroll_result_;
};

}  // namespace payments
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_UPDATE_VIRTUAL_CARD_ENROLLMENT_REQUEST_H_
