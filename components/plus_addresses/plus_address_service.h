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

namespace plus_addresses {

typedef base::OnceCallback<void(const std::string&)> PlusAddressCallback;

// An experimental class for filling plus addresses (asdf+123@some-domain.com).
// Not intended for widespread use.
class PlusAddressService : public KeyedService {
 public:
  // Used to simplify testing in cases where calls depending on the
  // identity manager can be mocked out.
  PlusAddressService();
  ~PlusAddressService() override;

  // Initialize the PlusAddressService with the `IdentityManager`.
  explicit PlusAddressService(signin::IdentityManager* identity_manager);

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

  // For now, simply generates a fake plus address and runs `callback` with it
  // immediately.
  void OfferPlusAddressCreation(const url::Origin& origin,
                                PlusAddressCallback callback);

  // The label for an autofill suggestion offering to create a new plus address.
  // While only debatably relevant to this class, this function allows for
  // further decoupling of PlusAddress generation and autofill.
  std::u16string GetCreateSuggestionLabel();

 private:
  // The user's existing set of plus addresses, scoped to sites.
  std::unordered_map<std::string, std::string> plus_address_by_site_;

  // Used to drive the `IsPlusAddress` function, and derived from the values of
  // `plus_profiles`.
  std::unordered_set<std::string> plus_addresses_;

  // Stores pointer to IdentityManager instance. It must outlive the
  // PlusAddressService and can be null during tests.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  // Handles requests to a remote server that this service uses.
  const PlusAddressClient plus_address_client_;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_H_
