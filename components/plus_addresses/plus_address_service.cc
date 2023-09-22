// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_service.h"

#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_client.h"
#include "components/plus_addresses/plus_address_prefs.h"
#include "components/plus_addresses/plus_address_types.h"
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
}  // namespace

PlusAddressService::PlusAddressService(
    signin::IdentityManager* identity_manager)
    : PlusAddressService(
          identity_manager,
          /*pref_service=*/nullptr,
          PlusAddressClient(identity_manager, /*url_loader_factory=*/nullptr)) {
}

PlusAddressService::PlusAddressService()
    : PlusAddressService(/*identity_manager=*/nullptr,
                         /*pref_service=*/nullptr,
                         PlusAddressClient(/*identity_manager=*/nullptr,
                                           /*url_loader_factory=*/nullptr)) {}

PlusAddressService::~PlusAddressService() = default;

PlusAddressService::PlusAddressService(
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    PlusAddressClient plus_address_client)
    : identity_manager_(identity_manager),
      repeating_timer_(CreateTimer(pref_service)),
      plus_address_client_(std::move(plus_address_client)) {
  // Begin PlusAddress periodic actions at construction.
  if (repeating_timer_) {
    repeating_timer_->Start();
  }
}

bool PlusAddressService::SupportsPlusAddresses(url::Origin origin) {
  // TODO(b/295187452): Also check `origin` here.
  return is_enabled();
}

absl::optional<std::string> PlusAddressService::GetPlusAddress(
    url::Origin origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string etld_plus_one = GetEtldPlusOne(origin);
  auto it = plus_address_by_site_.find(etld_plus_one);
  if (it == plus_address_by_site_.end()) {
    return absl::nullopt;
  }
  return absl::optional<std::string>(it->second);
}

void PlusAddressService::SavePlusAddress(url::Origin origin,
                                         std::string plus_address) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string etld_plus_one = GetEtldPlusOne(origin);
  plus_address_by_site_[etld_plus_one] = plus_address;
  plus_addresses_.insert(plus_address);
}

bool PlusAddressService::IsPlusAddress(std::string potential_plus_address) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return plus_addresses_.contains(potential_plus_address);
}

void PlusAddressService::OfferPlusAddressCreation(
    const url::Origin& origin,
    PlusAddressCallback callback) {
  if (!is_enabled()) {
    return;
  }
  // Check the local mapping before issuing a network request.
  if (absl::optional<std::string> plus_address = GetPlusAddress(origin);
      plus_address) {
    std::move(callback).Run(plus_address.value());
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

absl::optional<std::string> PlusAddressService::GetPrimaryEmail() {
  if (!identity_manager_ ||
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return absl::nullopt;
  }
  // TODO(crbug.com/1467623): This is fine for prototyping, but eventually we
  // must also take `AccountInfo::CanHaveEmailAddressDisplayed` into account
  // here and elsewhere in this file.
  return identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
      .email;
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
  return std::make_unique<signin::PersistentRepeatingTimer>(
      pref_service, prefs::kPlusAddressLastFetchedTime,
      /*delay=*/kEnterprisePlusAddressTimerDelay.Get(),
      /*task=*/
      base::BindRepeating(&PlusAddressService::SyncPlusAddressMapping,
                          // base::Unretained(this) is safe here since the timer
                          // that is created has same lifetime as this service.
                          base::Unretained(this)));
}

void PlusAddressService::SyncPlusAddressMapping() {
  plus_address_client_.GetAllPlusAddresses(base::BindOnce(
      &PlusAddressService::UpdatePlusAddressMap,
      // base::Unretained is safe here since PlusAddressService owns
      // the PlusAddressClient and they have the same lifetime.
      base::Unretained(this)));
}

void PlusAddressService::UpdatePlusAddressMap(const PlusAddressMap& map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_enabled()) {
    return;
  }
  plus_address_by_site_ = map;
  for (const auto& [_, value] : map) {
    plus_addresses_.insert(value);
  }
}

}  // namespace plus_addresses
