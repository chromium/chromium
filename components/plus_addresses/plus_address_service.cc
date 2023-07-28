// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/features.h"

namespace plus_addresses {

PlusAddressService::PlusAddressService() = default;
PlusAddressService::~PlusAddressService() = default;

bool PlusAddressService::SupportsPlusAddresses() {
  return base::FeatureList::IsEnabled(plus_addresses::kFeature);
}

absl::optional<std::string> PlusAddressService::GetPlusAddress(
    std::string facet) {
  auto it = plus_profiles_.find(facet);
  if (it == plus_profiles_.end()) {
    return absl::nullopt;
  }
  return absl::optional<std::string>(it->second.address);
}

void PlusAddressService::SavePlusAddress(std::string facet,
                                         PreAllocatedPlusAddress plus_address) {
  PlusProfile profile;
  profile.address = plus_address.address;
  plus_profiles_[facet] = profile;
  plus_addresses_.insert(profile.address);
}

bool PlusAddressService::IsPlusAddress(std::string potential_plus_address) {
  return plus_addresses_.contains(potential_plus_address);
}
}  // namespace plus_addresses
