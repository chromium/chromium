// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_FACILITATED_PAYMENTS_NETWORK_INTERFACE_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_FACILITATED_PAYMENTS_NETWORK_INTERFACE_H_

#include <memory>

#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface_base.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace payments::facilitated {

class FacilitatedPaymentsInitiatePaymentRequestDetails;
class FacilitatedPaymentsInitiatePaymentResponseDetails;

// Billable service number is defined in Payments server to distinguish
// different requests.
inline constexpr int kFacilitatedPaymentsBillableServiceNumber = 70154;

// Issues Payments RPCs and manages responses and failure conditions for
// Facilitated Payments. Only one request may be active at a time. Initiating a
// new request will cancel a pending request.
class FacilitatedPaymentsNetworkInterface
    : public autofill::payments::PaymentsNetworkInterfaceBase {
 public:
  using InitiatePaymentResponseCallback = base::OnceCallback<void(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult,
      std::unique_ptr<FacilitatedPaymentsInitiatePaymentResponseDetails>)>;

  FacilitatedPaymentsNetworkInterface(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      autofill::AccountInfoGetter* account_info_getter,
      bool is_off_the_record = false);

  FacilitatedPaymentsNetworkInterface(
      const FacilitatedPaymentsNetworkInterface&) = delete;
  FacilitatedPaymentsNetworkInterface& operator=(
      const FacilitatedPaymentsNetworkInterface&) = delete;

  ~FacilitatedPaymentsNetworkInterface() override;

  // Makes a `FacilitatedPaymentsInitiatePaymentRequest` to the Payments server.
  // This method is virtual so it can be overridden in tests.
  virtual void InitiatePayment(
      std::unique_ptr<FacilitatedPaymentsInitiatePaymentRequestDetails>
          request_details,
      InitiatePaymentResponseCallback response_callback,
      const std::string& app_locale);
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_FACILITATED_PAYMENTS_NETWORK_INTERFACE_H_
