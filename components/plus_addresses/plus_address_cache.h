// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_CACHE_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_CACHE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "components/plus_addresses/plus_address_types.h"

namespace plus_addresses {

// An in-memory cache of PlusProfile(s) utilized by the PlusAddressService to
// store copies of existing plus profiles.
class PlusAddressCache {
 public:
  PlusAddressCache();
  ~PlusAddressCache();

  // Inserts `profile` into the cache. Returns true if a new element was
  // inserted or false if an equivalent element already existed.
  bool InsertProfile(const PlusProfile& profile);

  // Erases `profile` from the cache. Returns true if a new element was erase or
  // false if no equivalent element existed.
  bool EraseProfile(const PlusProfile& profile);

  // Searches the cache for an element with an equivalent facet and returns a
  // profile if found, otherwise it returns `std::nullopt`.
  std::optional<PlusProfile> FindByFacet(
      const affiliations::FacetURI& facet) const;

  // Clears the cache.
  void Clear();

  // Checks whether the passed-in string is a known plus address.
  bool IsPlusAddress(const std::string& plus_address) const;

  // Returns all the cached plus profiles.
  base::span<const PlusProfile> GetPlusProfiles() const;

  // Returns true if the cache is empty, false otherwise.
  bool IsEmpty() const;

  // Returns the number of elements in the cache.
  size_t Size() const;

 private:
  // The user's existing set of `PlusProfile`s, ordered by facet. Since only a
  // single address per facet is supported, this can be used as the comparator.
  // The facets related to stored profiles are always valid web or Android
  // `FacetURI`s. This is guaranteed by the PlusAddress server. Notably, this
  // excludes facets related to http domains.
  base::flat_set<PlusProfile, PlusProfileFacetComparator> plus_profiles_;

  // Used to drive the `IsPlusAddress` function, and derived from the values of
  // `plus_profiles_`.
  base::flat_set<std::string> plus_addresses_;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_CACHE_H_
