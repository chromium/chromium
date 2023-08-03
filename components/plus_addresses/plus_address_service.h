// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_H_

#include <unordered_map>
#include <unordered_set>
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace plus_addresses {

// Represents a psuedo-profile-like object for use on a given facet.
struct PlusProfile {
  std::string address;
};

typedef base::OnceCallback<void(const std::string&)> PlusAddressCallback;

// An experimental class for filling plus addresses (asdf+123@some-domain.com).
// Not intended for widespread use.
class PlusAddressService : public KeyedService {
 public:
  // Default constructor/destructor only, for now.
  PlusAddressService();
  ~PlusAddressService() override;

  // Returns `true` when plus addresses are supported. Currently requires only
  // that the `kPlusAddressesEnabled` base::Feature is enabled.
  // TODO(crbug.com/1467623): also take signin state into account.
  bool SupportsPlusAddresses();
  // Get a plus address, if one exists, for the passed-in origin. Note that all
  // plus address activity is scoped to eTLD+1. This class owns the conversion
  // of `origin` to its eTLD+1 form.
  absl::optional<std::string> GetPlusAddress(url::Origin origin);
  // Save a plus address for the given origin, which is converted to its eTLD+1
  // form prior to persistence.
  void SavePlusAddress(url::Origin origin, std::string plus_address);
  // Check whether the passed-in string is a known plus address.
  bool IsPlusAddress(std::string potential_plus_address);

  // Eventually, will orchestrate UI elements to inform the user of the plus
  // address being created on their behalf, calling `PlusAddressCallback` on
  // confirmation. For now, however, simply generates a fake plus address and
  // runs `callback` with it immediately.
  void OfferPlusAddressCreation(url::Origin origin,
                                PlusAddressCallback callback);

 private:
  // The user's existing set of plus addresses, scoped to facets.
  std::unordered_map<std::string, PlusProfile> plus_profiles_;

  // Used to drive the `IsPlusAddress` function, and derived from the values of
  // `plus_profiles`.
  std::unordered_set<std::string> plus_addresses_;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_H_
