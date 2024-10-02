// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_H_

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/plus_addresses/plus_address_types.h"

namespace url {
class Origin;
}  // namespace url

namespace plus_addresses {

// This interface defines the public API for a service that manages plus
// addresses.
// Not intended for widespread use.
class PlusAddressService : public KeyedService,
                           public autofill::AutofillPlusAddressDelegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called whenever the set of plus addresses cached in the service
    // gets modified (e.g. `SavePlusProfile` calls, sync updates, etc).
    // `changes` represents a sequence of addition or removal operations
    // triggered on the local cache. Notably, update operations are emulated as
    // a remove operation of the old value followed by an addition of the
    // updated value.
    virtual void OnPlusAddressesChanged(
        const std::vector<PlusAddressDataChange>& changes) = 0;

    // Called when the observed PlusAddressService is being destroyed.
    virtual void OnPlusAddressServiceShutdown() = 0;
  };

  // Callback to return the list of plus profiles.
  using GetPlusProfilesCallback =
      base::OnceCallback<void(std::vector<PlusProfile>)>;

  virtual void AddObserver(Observer* o) = 0;
  virtual void RemoveObserver(Observer* o) = 0;

  // Returns whether plus address creation is supported for the given `origin`.
  // This is true iff:
  // - the plus address filling is enabled,
  // - the `origin` scheme is https,
  // - `is_off_the_record` is `false`, and
  // - plus address global toggle is on.
  virtual bool IsPlusAddressCreationEnabled(const url::Origin& origin,
                                            bool is_off_the_record) const = 0;

  // Returns a list of plus profiles for the `origin` and all affiliated
  // domains.
  virtual void GetAffiliatedPlusProfiles(const url::Origin& origin,
                                         GetPlusProfilesCallback callback) = 0;

  // Returns all the cached plus profiles. There are no server requests
  // triggered by this method, only the cached responses are returned.
  virtual base::span<const PlusProfile> GetPlusProfiles() const = 0;

  // Asks the PlusAddressHttpClient to reserve a plus address for use on
  // `origin` and returns the plus address via `on_completed`.
  //
  // Virtual to allow overriding the behavior in tests.
  virtual void ReservePlusAddress(const url::Origin& origin,
                                  PlusAddressRequestCallback on_completed) = 0;

  // Asks the PlusAddressHttpClient to refresh the plus address for `origin` and
  // calls `on_completed` with the result.
  virtual void RefreshPlusAddress(const url::Origin& origin,
                                  PlusAddressRequestCallback on_completed) = 0;

  // Asks the PlusAddressHttpClient to confirm `plus_address` for use on
  // `origin` and returns the plus address via `on_completed`.
  //
  // Virtual to allow overriding the behavior in tests.
  virtual void ConfirmPlusAddress(const url::Origin& origin,
                                  const PlusAddress& plus_address,
                                  PlusAddressRequestCallback on_completed) = 0;

  // Returns whether refreshing a plus address on `origin` is supported.
  virtual bool IsRefreshingSupported(const url::Origin& origin) = 0;

  // Gets a plus address, if one exists, for the passed-in facet.
  virtual std::optional<PlusAddress> GetPlusAddress(
      const affiliations::FacetURI& facet) const = 0;

  // Same as `GetPlusAddress()`, but returns the entire profile.
  virtual std::optional<PlusProfile> GetPlusProfile(
      const affiliations::FacetURI& facet) const = 0;

  // Used for displaying the user's email address in the UI modal.
  // virtual to allow mocking in tests that don't want to do identity setup.
  virtual std::optional<std::string> GetPrimaryEmail() = 0;

  // Returns whether a manual fallback suggestion should be shown on `origin`.
  // This is true iff
  // - plus address creation is supported or
  // - `is_off_the_record` is `true`  and the user has at least 1 plus address
  // for the given `origin`.
  virtual bool ShouldShowManualFallback(const url::Origin& origin,
                                        bool is_off_the_record) const = 0;

  // Saves a confirmed plus profile for its facet.
  virtual void SavePlusProfile(const PlusProfile& profile) = 0;

  // Returns true if the feature is supported for the user.
  virtual bool IsEnabled() const = 0;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_H_
