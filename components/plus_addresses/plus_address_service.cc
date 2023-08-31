// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_service.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/plus_addresses/features.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
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

// A dummy, temporary function to generate a domain-specific plus address. This
// will be replaced with a service integration.
absl::optional<std::string> MakePlusAddress(std::string email,
                                            url::Origin origin) {
  std::string etld_plus_one = GetEtldPlusOne(origin);
  std::vector<std::string> email_parts = base::SplitString(
      email, "@", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (email_parts.size() != 2) {
    return absl::nullopt;
  }

  // It's possible there would already be a plus in the plus address, so use
  // only the part before that point. Note that this function is temporary, so
  // further effort is not made to, e.g., ensure the domain actually supports
  // plus addresses, or to preserve it somehow.
  std::vector<std::string> maybe_preexisting_plus = base::SplitString(
      email_parts[0], "+", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (maybe_preexisting_plus.size() == 0) {
    return absl::nullopt;
  }
  return base::StrCat({maybe_preexisting_plus[0], "+",
                       GetPlusAddressSuffixForEtldPlusOne(etld_plus_one), "@",
                       email_parts[1]});
}
}  // namespace

PlusAddressService::PlusAddressService()
    : PlusAddressService(/*identity_manager=*/nullptr) {}

PlusAddressService::~PlusAddressService() = default;

PlusAddressService::PlusAddressService(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {}

bool PlusAddressService::SupportsPlusAddresses(url::Origin origin) {
  return base::FeatureList::IsEnabled(plus_addresses::kFeature) &&
         identity_manager_ != nullptr &&
         // Note that having a primary account implies that account's email will
         // be populated.
         identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin);
}

absl::optional<std::string> PlusAddressService::GetPlusAddress(
    url::Origin origin) {
  std::string etld_plus_one = GetEtldPlusOne(origin);
  auto it = plus_address_by_site_.find(etld_plus_one);
  if (it == plus_address_by_site_.end()) {
    return absl::nullopt;
  }
  return absl::optional<std::string>(it->second);
}

void PlusAddressService::SavePlusAddress(url::Origin origin,
                                         std::string plus_address) {
  std::string etld_plus_one = GetEtldPlusOne(origin);
  plus_address_by_site_[etld_plus_one] = plus_address;
  plus_addresses_.insert(plus_address);
}

bool PlusAddressService::IsPlusAddress(std::string potential_plus_address) {
  return plus_addresses_.contains(potential_plus_address);
}

void PlusAddressService::OfferPlusAddressCreation(
    const url::Origin& origin,
    PlusAddressCallback callback) {
  if (!identity_manager_ ||
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return;
  }
  const std::string email =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email;
  absl::optional<std::string> result = MakePlusAddress(email, origin);
  if (!result.has_value()) {
    return;
  }
  SavePlusAddress(origin, result.value());
  std::move(callback).Run(result.value());
}

std::u16string PlusAddressService::GetCreateSuggestionLabel() {
  // TODO(crbug.com/1467623): once ready, use standard
  // `l10n_util::GetStringUTF16` instead of using feature params.
  return base::UTF8ToUTF16(
      plus_addresses::kEnterprisePlusAddressLabelOverride.Get());
}
}  // namespace plus_addresses
