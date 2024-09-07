// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PREALLOCATOR_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PREALLOCATOR_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/queue.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/plus_addresses/plus_address_allocator.h"
#include "components/plus_addresses/plus_address_http_client.h"
#include "components/plus_addresses/plus_address_types.h"
#include "net/base/backoff_entry.h"
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

  // Used to check the `PlusAddressService` is in its "enabled" state - i.e.,
  // whether the user is signed in, the plus address feature is enabled for the
  // profile, etc.
  using IsEnabledCheck = base::RepeatingCallback<bool()>;

  PlusAddressPreallocator(PrefService* pref_service,
                          PlusAddressSettingService* setting_service,
                          PlusAddressHttpClient* http_client,
                          IsEnabledCheck);
  ~PlusAddressPreallocator() override;

  // PlusAddressAllocator:
  void AllocatePlusAddress(const url::Origin& origin,
                           AllocationMode mode,
                           PlusAddressRequestCallback callback) override;
  std::optional<PlusProfile> AllocatePlusAddressSynchronously(
      const url::Origin& origin,
      AllocationMode mode) override;
  bool IsRefreshingSupported(const url::Origin& origin) const override;
  void RemoveAllocatedPlusAddress(const PlusAddress& plus_address) override;

 private:
  // Ensures that the index of the next pre-allocated address is within the
  // bounds of the array of pre-allocated plus addresses if those are not empty.
  // If they are empty, it sets the index to zero.
  void FixIndexOfNextPreallocatedAddress();

  // Deletes pre-allocated plus addresses that have reached their EOL and
  // updates the index of the next plus preallocated plus address.
  void PrunePreallocatedPlusAddresses();

  // Requests new pre-allocated plus addresses if
  // 1) `IsEnabled()` is true,
  // 2) there are less than `kPlusAddressPreallocationMinimumSize` pre-allocated
  //   addresses left,
  // 3) there is no ongoing pre-allocation request,
  // 4) we are not in a cool-off period due to repeated, timed-out requests. If
  //   `is_user_triggered` is `true`, the cool-off period is ignored.
  void MaybeRequestNewPreallocatedPlusAddresses(bool is_user_triggered);

  // Returns whether the global feature toggle for plus addresses is on and
  // `IsEnabledCheck` returns `true`.
  bool IsEnabled() const;

  // Schedules a server request to pre-allocate more addresses in `time_delta`.
  // Overrides any previously scheduled requests.
  void SendRequestWithDelay(base::TimeDelta delay);

  // Sends a request to pre-allocate addresses.
  void SendRequest();

  // Adds the preallocated plus addresses in `result` to the local store.
  void OnReceivePreallocatedPlusAddresses(
      PlusAddressHttpClient::PreallocatePlusAddressesResult result);

  // Attempts to process the pending `requests_`. If there are not enough
  // pre-allocated addresses, it will request more and resume processing once
  // the request for more pre-allocated addresses has finished.
  void ProcessAllocationRequests(bool is_user_triggered);

  // Replies to all currently ongoing requests with `error`.
  void ReplyToRequestsWithError(const PlusAddressRequestError& error);

  // Returns the next available pre-allocated plus address or `std::nullopt` if
  // there is none. It does not attempt to pre-allocate more.
  std::optional<PlusAddress> GetNextPreallocatedPlusAddress();

  // Returns the pre-allocated plus addresses from pref-storage.
  const base::Value::List& GetPreallocatedAddresses() const;

  // Returns the index of the next pre-allocated plus address from pref-storage.
  int GetIndexOfNextPreallocatedAddress() const;

  // Owned by the `Profile` that (indirectly) owns `this`.
  const raw_ref<PrefService> pref_service_;

  // Used to check whether plus addresses are enabled globally and whether the
  // notice has been accepted.
  const raw_ref<PlusAddressSettingService> settings_;

  // Responsible for server communication. Owned by the `PlusAddressService` and
  // outlives `this`.
  const raw_ref<PlusAddressHttpClient> http_client_;

  const IsEnabledCheck is_enabled_check_;

  // Whether a request for more pre-allocated plus addresses is ongoing.
  bool is_server_request_ongoing_ = false;

  // A helper for dealing with retries if a server request failed due to a
  // timeout.
  net::BackoffEntry backoff_entry_;

  // A timer that governs when the next request will be made.
  base::OneShotTimer server_request_timer_;

  // A helper class to keep track of the arguments that were passed when
  // requesting a plus address allocation.
  struct Request final {
    Request(PlusAddressRequestCallback callback, affiliations::FacetURI facet);
    Request(Request&&);
    Request& operator=(Request&&);
    ~Request();

    PlusAddressRequestCallback callback;
    affiliations::FacetURI facet;
  };
  // The pre-allocation requests that have not yet been completed because there
  // are no pre-allocated plus addresses available. Handled in FIFO order.
  base::queue<Request> requests_;

  base::WeakPtrFactory<PlusAddressPreallocator> weak_ptr_factory_{this};
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PREALLOCATOR_H_
