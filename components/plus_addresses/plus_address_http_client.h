// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_HTTP_CLIENT_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_HTTP_CLIENT_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/plus_addresses/plus_address_types.h"

namespace url {
class Origin;
}  // namespace url

namespace plus_addresses {

// An interface for communicating with the remote plus address server.
class PlusAddressHttpClient {
 public:
  virtual ~PlusAddressHttpClient() = default;

  // Initiates a request to get a plus address for use on `origin` and runs
  // `on_completed` when the request is completed. If `refresh` is true, then
  // the server receives a flag that indicates that the address should be a new
  // one and not one that the user has previously seen.
  // On success, the facet of the returned `PlusProfile` is set to the `origin`
  // by the server.
  virtual void ReservePlusAddress(const url::Origin& origin,
                                  bool refresh,
                                  PlusAddressRequestCallback on_completed) = 0;

  // Initiates a request to confirm `plus_address` for use on `origin` and runs
  // `on_completed` when the request is completed.
  // On success, the facet of the returned `PlusProfile` is set to the `origin`
  // by the server.
  virtual void ConfirmPlusAddress(const url::Origin& origin,
                                  const PlusAddress& plus_address,
                                  PlusAddressRequestCallback on_completed) = 0;

  using PreallocatePlusAddressesResult =
      base::expected<std::vector<PreallocatedPlusAddress>,
                     PlusAddressRequestError>;
  using PreallocatePlusAddressesCallback =
      base::OnceCallback<void(PreallocatePlusAddressesResult)>;

  // Initiates a request for preallocated plus addresses and runs `callback`
  // with the response.
  virtual void PreallocatePlusAddresses(
      PreallocatePlusAddressesCallback callback) = 0;

  // Resets state in the client, e.g., by cancelling ongoing network requests.
  virtual void Reset() = 0;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_HTTP_CLIENT_H_
