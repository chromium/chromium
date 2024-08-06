// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PREALLOCATOR_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PREALLOCATOR_H_

#include <string_view>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/plus_addresses/plus_address_allocator.h"
#include "components/plus_addresses/plus_address_http_client.h"
#include "components/plus_addresses/plus_address_types.h"
#include "url/origin.h"

class PrefService;

namespace url {
class Origin;
}  // namespace url

namespace plus_addresses {

class PlusAddressService;
class PlusAddressSettingService;

class PlusAddressPreallocator : public PlusAddressAllocator {
 public:
  // The delay before (potentially) making a server request for more
  // pre-allocated plus addresses after creation. Non-null to avoid regressing
  // startup times.
  static constexpr auto kDelayUntilServerRequestAfterStartup =
      base::Seconds(30);

  // Keys used to serialize pre-allocated plus addresses to a `base::Value`.
  static constexpr std::string_view kEndOfLifeKey = "eol";
  static constexpr std::string_view kPlusAddressKey = "plus_address";

  PlusAddressPreallocator(PrefService* pref_service,
                          PlusAddressSettingService* setting_service,
                          PlusAddressHttpClient* http_client);
  ~PlusAddressPreallocator() override;

  // PlusAddressAllocator:
  void AllocatePlusAddress(const url::Origin& origin,
                           AllocationMode mode,
                           PlusAddressRequestCallback callback) override;
  bool IsRefreshingSupported(const url::Origin& origin) const override;

 private:
  // Deletes pre-allocated plus addresses that have reached their EOL and
  // updates the index of the next plus preallocated plus address.
  void PrunePreallocatedPlusAddresses();

  // Requests new pre-allocated plus addresses if
  // - the global feature toggle for plus addresses is on, and
  // - there are less than `kPlusAddressPreallocationMinimumSize` pre-allocated
  //   addresses left.
  void MaybeRequestNewPreallocatedPlusAddresses();

  // Adds the preallocated plus addresses in `result` to the local store.
  void OnReceivePreallocatedPlusAddresses(
      PlusAddressHttpClient::PreallocatePlusAddressesResult result);

  // Owned by the `Profile` that (indirectly) owns `this`.
  const raw_ref<PrefService> pref_service_;

  // Used to check whether plus addresses are enabled globally and whether the
  // notice has been accepted.
  const raw_ref<PlusAddressSettingService> settings_;

  // Responsible for server communication. Owned by the `PlusAddressService` and
  // outlives `this`.
  const raw_ref<PlusAddressHttpClient> http_client_;

  // Whether a request for more pre-allocated plus addresses is ongoing.
  bool is_server_request_ongoing_ = false;

  base::WeakPtrFactory<PlusAddressPreallocator> weak_ptr_factory_{this};
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PREALLOCATOR_H_
