// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_API_CLIENT_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_API_CLIENT_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"

struct CoreAccountInfo;

namespace payments::facilitated {

// A cross-platform interface for invoking the facilitated payment API. Each
// platform provides its own implementation by providing a definition for the
// static CreateFacilitatedPaymentsApiClient() function, which is declared in
// `facilitated_payments_api_client_factory.h`.
//
// All methods provide results through callbacks. These callbacks can be either
// synchronous or asynchronous.
//
// For each method in the interface, only one method call per API client should
// be made at a time, because, if more than one call is made at a time, then
// only the last callback will be invoked.
//
// Example usage:
//  std::unique_ptr<FacilitatedPaymentsApiClient> apiClient =
//      CreateFacilitatedPaymentsApiClient();
//  apiClient->IsAvailable(base::BindOnce(&MyClass::OnIsAvailable,
//                                        weak_ptr_factory_.GetWeakPtr()));
class FacilitatedPaymentsApiClient {
 public:
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.facilitated_payments
  // The result of invoking the purchase manager with an action token.
  enum class PurchaseActionResult : int {
    // Could not invoke the purchase manager.
    kCouldNotInvoke,

    // The purchase manager was invoked successfully.
    kResultOk,

    // The user cancelled out of the purchase manager flow.
    kResultCanceled,
  };

  virtual ~FacilitatedPaymentsApiClient() = default;

  // Checks whether the facilitated payment API is available and invokes the
  // given `callback` with the result. (If the API is not available, there is no
  // need to show FOPs to the user.) Only one IsAvailable() call per API client
  // should be made at a time, because, if more than one IsAvailable() call is
  // made at a time, then only the last callback will be invoked.
  virtual void IsAvailable(base::OnceCallback<void(bool)> callback) = 0;

  // Retrieves the client token to be used to initiate a payment and invokes the
  // given `callback` with the result. Only one GetClientToken() call per API
  // client should be made a time, because, if more than one GetClientToken()
  // call is made at a time, then only the last callback will be invoked.
  virtual void GetClientToken(
      base::OnceCallback<void(std::vector<uint8_t>)> callback) = 0;

  // Invokes the purchase manager with the given action token and invokes the
  // given `callback` with the result. Only one InvokePurchaseAction() call per
  // API client should be made at a time, because, if more than one
  // InvokePurchaseAction() call is made at a time, then only the last callback
  // will be invoked.
  virtual void InvokePurchaseAction(
      CoreAccountInfo primary_account,
      base::span<const uint8_t> action_token,
      base::OnceCallback<void(PurchaseActionResult)> callback) = 0;
};

// A one time use factory for the facilitated payment API client.
using FacilitatedPaymentsApiClientCreator =
    base::OnceCallback<std::unique_ptr<FacilitatedPaymentsApiClient>()>;

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_API_CLIENT_H_
