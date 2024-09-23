// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_cache.h"

#include <optional>
#include <vector>

#include "components/plus_addresses/plus_address_types.h"

namespace plus_addresses {

PlusAddressCache::PlusAddressCache() = default;
PlusAddressCache::~PlusAddressCache() = default;

bool PlusAddressCache::InsertProfile(const PlusProfile& profile) {
  const auto [it, inserted] = plus_profiles_.insert(profile);
  if (inserted) {
    plus_addresses_.insert(*profile.plus_address);
  }
  return inserted;
}

bool PlusAddressCache::EraseProfile(const PlusProfile& profile) {
  bool erased = plus_profiles_.erase(profile);
  if (erased) {
    plus_addresses_.erase(*profile.plus_address);
  }
  return erased;
}

std::optional<PlusProfile> PlusAddressCache::FindByFacet(
    const affiliations::FacetURI& facet) const {
  // `facet` is used as the comparator, so the other fields don't matter.
  if (auto it =
          plus_profiles_.find(PlusProfile("", facet, PlusAddress(), false));
      it != plus_profiles_.end()) {
    return *it;
  }
  return std::nullopt;
}

void PlusAddressCache::Clear() {
  plus_profiles_.clear();
  plus_addresses_.clear();
}

bool PlusAddressCache::IsPlusAddress(
    const std::string& potential_plus_address) const {
  return plus_addresses_.contains(potential_plus_address);
}

base::span<const PlusProfile> PlusAddressCache::GetPlusProfiles() const {
  return plus_profiles_;
}

bool PlusAddressCache::IsEmpty() const {
  return plus_profiles_.empty();
}

size_t PlusAddressCache::Size() const {
  return plus_profiles_.size();
}

}  // namespace plus_addresses
