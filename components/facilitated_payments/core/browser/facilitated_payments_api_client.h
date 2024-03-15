// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_API_CLIENT_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_API_CLIENT_H_

#include <cstdint>
#include <memory>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"

namespace payments::facilitated {

class FacilitatedPaymentsApiClientDelegate;

// A cross-platform interface for invoking the facilitated payment API. Each
// platform provides its own implementation by providing a definition for the
// static Create() method, which is declared in this header. All methods call
// back into the FacilitatedPaymentsApiClientDelegate. These calls can be either
// synchrous or asynchronous.
// Example usage:
//  std::unique_ptr<FacilitatedPaymentsApiClient> apiClient =
//      FacilitatedPaymentsApiClient::Create(delegate->GetWeakPtr());
//  apiClient->IsAvailable();  // Will call back into delegate->OnIsAvailable().
class FacilitatedPaymentsApiClient {
 public:
  virtual ~FacilitatedPaymentsApiClient() = default;

  // Creates a platform-specific instances of the API client. This method is
  // defined in platform specific implementation source files, e.g., in
  // facilitated_payments_api_client_android.cc.
  static std::unique_ptr<FacilitatedPaymentsApiClient> Create(
      base::WeakPtr<FacilitatedPaymentsApiClientDelegate> delegate);

  // Checks whether the facilitated payment API is available. The response is
  // received in the FacilitatedPaymentsApiClientDelegate.OnIsAvailable()
  // method. (If the API is not available, there is no need to show FOPs to the
  // user.)
  virtual void IsAvailable() = 0;

  // Retrieves the client token to be used to initiate a payment. The response
  // is received in the FacilitatedPaymentsApiClientDelegate.OnGetClientToken()
  // method.
  virtual void GetClientToken() = 0;

  // Invokes the purchase manager with the given action token. The result is
  // received in the
  // FacilitatedPaymentsApiClientDelegate.OnPurchaseActionResult() method.
  virtual void InvokePurchaseAction(base::span<const uint8_t> action_token) = 0;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_API_CLIENT_H_
