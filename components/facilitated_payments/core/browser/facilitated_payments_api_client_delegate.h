// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_API_CLIENT_DELEGATE_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_API_CLIENT_DELEGATE_H_

#include <cstdint>
#include <vector>

namespace payments::facilitated {

// The interface for receiving back the responses from the facilitated payment
// API. These methods can be invoked either synchronously or asynchronously.
class FacilitatedPaymentsApiClientDelegate {
 public:
  virtual ~FacilitatedPaymentsApiClientDelegate() = default;

  // Called in response to the FacilitatedPaymentsApiClient.IsAvailable()
  // method. If the `is_available` boolean is false, then no FOP (form of
  // payment) selector should be shown.
  virtual void OnIsAvailable(bool is_available) = 0;

  // Called in response to the FacilitatedPaymentsApiClient.GetClientToken()
  // method. If not empty, the `client_token` can be used to initiate a payment.
  virtual void OnGetClientToken(std::vector<uint8_t> client_token) = 0;

  // Called in response to the
  // FacilitatedPaymentsApiClient.InvokePurchaseAction() method, i.e., the
  // method that initiated the payment flow UI.
  virtual void OnPurchaseActionResult(bool is_purchase_action_successful) = 0;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_API_CLIENT_DELEGATE_H_
