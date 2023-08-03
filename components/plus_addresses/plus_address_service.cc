// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_service.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
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

// A dummy, temporary function to generate a domain-specific string to be the
// part after the plus in a plus address. This will be replaced with a service
// integration.
std::string GetPlusAddressSuffixForEtldPlusOne(std::string etld_plus_one) {
  int total = 0;
  for (const char& character : etld_plus_one) {
    total += character;
  }
  return base::NumberToString(total % 10000);
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

void PlusAddressService::OfferPlusAddressCreation(
    url::Origin origin,
    PlusAddressCallback callback) {
  std::string etld_plus_one = GetEtldPlusOne(origin);
  // TODO(crbug.com/1467623): use user sign-in state to get their actual email
  // address instead of hard-coded strings.
  std::string result =
      base::StrCat({"test+", GetPlusAddressSuffixForEtldPlusOne(etld_plus_one),
                    "@test.example"});
  SavePlusAddress(origin, result);
  std::move(callback).Run(result);
}
}  // namespace plus_addresses
