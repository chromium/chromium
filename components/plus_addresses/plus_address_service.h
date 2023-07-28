// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_H_

#include <unordered_map>
#include <unordered_set>
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace plus_addresses {

// Represents a psuedo-profile-like object for use on a given facet.
struct PlusProfile {
  std::string address;
};

// Represents a pre-allocated plus address, which can expire (note that the
// expiration concept is best-effort, as clock skew can't be ruled out).
struct PreAllocatedPlusAddress {
  std::string address;
  base::Time expiry;
};

// An experimental class for filling plus addresses (asdf+123@some-domain.com).
// Not intended for widespread use.
class PlusAddressService {
 public:
  // Default constructor/destructor only, for now.
  PlusAddressService();
  ~PlusAddressService();

  // Returns `true` when plus addresses are supported. Currently requires only
  // that the `kPlusAddressesEnabled` base::Feature is enabled.
  // TODO(crbug.com/1467623): also take signin state into account.
  bool SupportsPlusAddresses();
  // Get a plus address, if one exists, for the passed-in facet.
  absl::optional<std::string> GetPlusAddress(std::string facet);
  // Save a plus address for the given facet.
  void SavePlusAddress(std::string facet, PreAllocatedPlusAddress plus_address);
  // Check whether the passed-in string is a known plus address.
  bool IsPlusAddress(std::string potential_plus_address);

 private:
  // The user's existing set of plus addresses, scoped to facets.
  std::unordered_map<std::string, PlusProfile> plus_profiles_;

  // Used to drive the `IsPlusAddress` function, and derived from the values of
  // `plus_profiles`.
  std::unordered_set<std::string> plus_addresses_;

  // Pre-allocated plus addresses.
  std::vector<PreAllocatedPlusAddress> pre_allocated_plus_addresses_;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_H_
