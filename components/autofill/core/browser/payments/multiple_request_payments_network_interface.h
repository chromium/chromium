// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_MULTIPLE_REQUEST_PAYMENTS_NETWORK_INTERFACE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_MULTIPLE_REQUEST_PAYMENTS_NETWORK_INTERFACE_H_

#include "components/autofill/core/browser/payments/multiple_request_payments_network_interface_base.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"

namespace autofill::payments {

// Issues Payments RPCs and manages responses and failure conditions for
// Autofill Payments. Multiple requests may be active at the same time.
// Sending another request will not affect any pending requests.
class MultipleRequestPaymentsNetworkInterface
    : public MultipleRequestPaymentsNetworkInterfaceBase {
 public:
  using RequestId = base::StrongAlias<struct RequestIdTag, std::string>;
  using GetDetailsForCreateCardCallback = base::OnceCallback<void(
      PaymentsAutofillClient::PaymentsRpcResult result,
      const std::u16string& context_token,
      std::unique_ptr<base::Value::Dict> legal_message,
      std::vector<std::pair<int, int>> supported_card_bin_ranges)>;

  // `identity_manager` must outlive this.
  MultipleRequestPaymentsNetworkInterface(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager& identity_manager,
      bool is_off_the_record = false);

  MultipleRequestPaymentsNetworkInterface(
      const MultipleRequestPaymentsNetworkInterface&) = delete;
  MultipleRequestPaymentsNetworkInterface& operator=(
      const MultipleRequestPaymentsNetworkInterface&) = delete;

  ~MultipleRequestPaymentsNetworkInterface() override;

  // Sends a preflight request to determine if the user meets the Payments
  // service's conditions for card saving.
  virtual RequestId GetDetailsForCreateCard(
      const UploadCardRequestDetails& details,
      GetDetailsForCreateCardCallback callback);

  // Sends a request to save the card.
  virtual RequestId CreateCard(
      const UploadCardRequestDetails& details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const std::string&)> callback);

  // Retrieve information necessary for the enrollment from the server. This is
  // invoked before we show the bubble to request user consent for the
  // enrollment.
  virtual RequestId GetVirtualCardEnrollmentDetails(
      const GetDetailsForEnrollmentRequestDetails& request_details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const GetDetailsForEnrollmentResponseDetails&)>
          callback);

  // The user has chosen to change the virtual-card enrollment of a credit card.
  // Send the necessary information for the server to identify the credit card
  // for which virtual-card enrollment will be updated, as well as metadata so
  // that the server understands the context for the request.
  virtual RequestId UpdateVirtualCardEnrollment(
      const UpdateVirtualCardEnrollmentRequestDetails& request_details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult)>
          callback);
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_MULTIPLE_REQUEST_PAYMENTS_NETWORK_INTERFACE_H_
