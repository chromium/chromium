// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_MULTIPLE_REQUEST_FACILITATED_PAYMENTS_NETWORK_INTERFACE_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_MULTIPLE_REQUEST_FACILITATED_PAYMENTS_NETWORK_INTERFACE_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/payments/multiple_request_payments_network_interface_base.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace autofill {
class AccountInfoGetter;
}  // namespace autofill

namespace signin {
class IdentityManager;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace payments::facilitated {

class FacilitatedPaymentsInitiatePaymentRequestDetails;
class FacilitatedPaymentsInitiatePaymentResponseDetails;

// Issues Payments RPCs and manages responses and failure conditions for
// Facilitated Payments. Multiple request may be active at the same time.
// Sending another request will not affect any pending requests.
class MultipleRequestFacilitatedPaymentsNetworkInterface
    : public autofill::payments::MultipleRequestPaymentsNetworkInterfaceBase {
 public:
  using InitiatePaymentResponseCallback = base::OnceCallback<void(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult,
      std::unique_ptr<FacilitatedPaymentsInitiatePaymentResponseDetails>)>;
  using RequestId =
      base::StrongAlias<struct autofill::payments::RequestIdTag, std::string>;

  // `identity_manager` and `account_info_getter` must outlive this.
  MultipleRequestFacilitatedPaymentsNetworkInterface(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager& identity_manager,
      autofill::AccountInfoGetter& account_info_getter,
      bool is_off_the_record = false);

  MultipleRequestFacilitatedPaymentsNetworkInterface(
      const MultipleRequestFacilitatedPaymentsNetworkInterface&) = delete;
  MultipleRequestFacilitatedPaymentsNetworkInterface& operator=(
      const MultipleRequestFacilitatedPaymentsNetworkInterface&) = delete;

  ~MultipleRequestFacilitatedPaymentsNetworkInterface() override;

  // Makes a `FacilitatedPaymentsInitiatePaymentRequest` to the Payments server.
  // This method is virtual so it can be overridden in tests.
  virtual RequestId InitiatePayment(
      std::unique_ptr<FacilitatedPaymentsInitiatePaymentRequestDetails>
          request_details,
      InitiatePaymentResponseCallback response_callback,
      const std::string& app_locale);

 private:
  raw_ref<autofill::AccountInfoGetter> account_info_getter_;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_MULTIPLE_REQUEST_FACILITATED_PAYMENTS_NETWORK_INTERFACE_H_
