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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto [it, inserted] = plus_profiles_.insert(profile);
  if (inserted) {
    plus_addresses_.insert(profile.plus_address);
  }
  return inserted;
}

bool PlusAddressCache::EraseProfile(const PlusProfile& profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool erased = plus_profiles_.erase(profile);
  if (erased) {
    plus_addresses_.erase(profile.plus_address);
  }
  return erased;
}

std::optional<PlusProfile> PlusAddressCache::FindByFacet(
    const PlusProfile::facet_t& facet) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // `facet` is used as the comparator, so the other fields don't matter.
  if (auto it = plus_profiles_.find(PlusProfile("", facet, "", false));
      it != plus_profiles_.end()) {
    return *it;
  }
  return std::nullopt;
}

void PlusAddressCache::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  plus_profiles_.clear();
  plus_addresses_.clear();
}

bool PlusAddressCache::IsPlusAddress(
    const std::string& potential_plus_address) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return plus_addresses_.contains(potential_plus_address);
}

base::span<const PlusProfile> PlusAddressCache::GetPlusProfiles() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return plus_profiles_;
}

bool PlusAddressCache::IsEmpty() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return plus_profiles_.empty();
}

size_t PlusAddressCache::Size() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return plus_profiles_.size();
}

}  // namespace plus_addresses
