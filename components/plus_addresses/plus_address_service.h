// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_H_

#include <unordered_map>
#include <unordered_set>
#include "base/functional/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/plus_addresses/plus_address_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace signin {
class IdentityManager;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace plus_addresses {

// An experimental class for filling plus addresses (asdf+123@some-domain.com).
// Not intended for widespread use.
class PlusAddressService : public KeyedService {
 public:
  // Used to simplify testing in cases where calls depending on the
  // identity manager can be mocked out.
  PlusAddressService();
  ~PlusAddressService() override;

  // Initialize the PlusAddressService with the `IdentityManager` and a
  // `SharedURLLoaderFactory`.
  PlusAddressService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Returns `true` when plus addresses are supported. Currently requires only
  // that the `kPlusAddressesEnabled` base::Feature is enabled.
  // Virtual to allow overriding the behavior in tests. This allows external
  // tests (e.g., those in autofill that depend on this class) to substitute
  // their own behavior.
  // TODO(crbug.com/1467623): react to `origin` parameter.
  virtual bool SupportsPlusAddresses(url::Origin origin);
  // Get a plus address, if one exists, for the passed-in origin. Note that all
  // plus address activity is scoped to eTLD+1. This class owns the conversion
  // of `origin` to its eTLD+1 form.
  absl::optional<std::string> GetPlusAddress(url::Origin origin);
  // Save a plus address for the given origin, which is converted to its eTLD+1
  // form prior to persistence.
  void SavePlusAddress(url::Origin origin, std::string plus_address);
  // Check whether the passed-in string is a known plus address.
  bool IsPlusAddress(std::string potential_plus_address);

  // Asks the PlusAddressClient to get a plus address for use on `origin` and on
  // completion: runs`callback` with the created plus address, and stores the
  // plus address in this service.
  void OfferPlusAddressCreation(const url::Origin& origin,
                                PlusAddressCallback callback);

  // The label for an autofill suggestion offering to create a new plus address.
  // While only debatably relevant to this class, this function allows for
  // further decoupling of PlusAddress generation and autofill.
  std::u16string GetCreateSuggestionLabel();

  // Helper to prevent using the service integration to keep tests simpler..
  void set_use_url_based_plus_addresses_for_testing(bool enabled);

 private:
  // The user's existing set of plus addresses, scoped to sites.
  std::unordered_map<std::string, std::string> plus_address_by_site_;

  // Used to drive the `IsPlusAddress` function, and derived from the values of
  // `plus_profiles`.
  std::unordered_set<std::string> plus_addresses_;

  // Stores pointer to IdentityManager instance. It must outlive the
  // PlusAddressService and can be null during tests.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  // Controls whether this service uses the url-based approach or  the remote
  // service integration to create a plus-address.
  bool use_url_based_plus_address_ = false;

  // Handles requests to a remote server that this service uses.
  PlusAddressClient plus_address_client_;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_H_
