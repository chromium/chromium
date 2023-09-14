// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_service.h"

#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_prefs.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/persistent_repeating_timer.h"
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
    : PlusAddressService(/*identity_manager=*/nullptr,
                         /*pref_service=*/nullptr,
                         /*url_loader_factory=*/nullptr) {}

PlusAddressService::~PlusAddressService() = default;

PlusAddressService::PlusAddressService(
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      repeating_timer_(CreateTimer(pref_service)),
      plus_address_client_(identity_manager, std::move(url_loader_factory)) {}

bool PlusAddressService::SupportsPlusAddresses(url::Origin origin) {
  // TODO(b/295187452): Also check `origin` here.
  return is_enabled();
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
  if (!is_enabled()) {
    return;
  }

  // TODO (kaklilu): Remove this once we can Mock the PlusAddressClient in tests
  if (use_url_based_plus_address_) {
    const std::string email =
        identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
            .email;
    absl::optional<std::string> result = MakePlusAddress(email, origin);
    if (!result.has_value()) {
      return;
    }
    SavePlusAddress(origin, result.value());
    std::move(callback).Run(result.value());
    return;
  }
  plus_address_client_.CreatePlusAddress(
      GetEtldPlusOne(origin),
      // On receiving the PlusAddress...
      base::BindOnce(
          // ... first send it back to Autofill
          [](PlusAddressCallback callback, const std::string& plus_address) {
            std::move(callback).Run(plus_address);
            return plus_address;
          },
          std::move(callback))
          // ... then save it in this service.
          .Then(base::BindOnce(
              &PlusAddressService::SavePlusAddress,
              // base::Unretained is safe here since PlusAddressService owns
              // the PlusAddressClient and they will have the same lifetime.
              base::Unretained(this), origin)));
}

std::u16string PlusAddressService::GetCreateSuggestionLabel() {
  // TODO(crbug.com/1467623): once ready, use standard
  // `l10n_util::GetStringUTF16` instead of using feature params.
  return base::UTF8ToUTF16(
      plus_addresses::kEnterprisePlusAddressLabelOverride.Get());
}

void PlusAddressService::set_use_url_based_plus_addresses_for_testing(
    bool enabled) {
  use_url_based_plus_address_ = enabled;
}

bool PlusAddressService::is_enabled() const {
  return base::FeatureList::IsEnabled(plus_addresses::kFeature) &&
         identity_manager_ != nullptr &&
         // Note that having a primary account implies that account's email will
         // be populated.
         identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin);
}

std::unique_ptr<signin::PersistentRepeatingTimer>
PlusAddressService::CreateTimer(PrefService* pref_service) {
  if (!is_enabled() || !pref_service ||
      !kSyncWithEnterprisePlusAddressServer.Get()) {
    return nullptr;
  }
  // TODO(b/297366364):
  // - Make delay configurable via a Finch parameter.
  // - Replace task with a RepeatingCallback to fetch plus addresses and update
  //   the structures in this class.
  return std::make_unique<signin::PersistentRepeatingTimer>(
      pref_service, prefs::kPlusAddressLastFetchedTime,
      /*delay=*/kEnterprisePlusAddressTimerDelay.Get(),
      /*task=*/base::DoNothing());
}
}  // namespace plus_addresses
