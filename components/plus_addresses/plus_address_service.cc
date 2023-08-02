// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace plus_addresses {

namespace {
// Get the ETLD+1 of `origin`, which means any subdomain is treated
// equivalently.
std::string GetEtldPlusOne(const url::Origin origin) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}
}  // namespace

PlusAddressService::PlusAddressService() = default;
PlusAddressService::~PlusAddressService() = default;

bool PlusAddressService::SupportsPlusAddresses() {
  return base::FeatureList::IsEnabled(plus_addresses::kFeature);
}

absl::optional<std::string> PlusAddressService::GetPlusAddress(
    url::Origin origin) {
  std::string etld_plus_one = GetEtldPlusOne(origin);
  auto it = plus_profiles_.find(etld_plus_one);
  if (it == plus_profiles_.end()) {
    return absl::nullopt;
  }
  return absl::optional<std::string>(it->second.address);
}

void PlusAddressService::SavePlusAddress(url::Origin origin,
                                         std::string plus_address) {
  std::string etld_plus_one = GetEtldPlusOne(origin);
  PlusProfile profile;
  profile.address = plus_address;
  plus_profiles_[etld_plus_one] = profile;
  plus_addresses_.insert(profile.address);
}

bool PlusAddressService::IsPlusAddress(std::string potential_plus_address) {
  return plus_addresses_.contains(potential_plus_address);
}
}  // namespace plus_addresses
