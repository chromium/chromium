// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/management_service.h"

#include <ostream>
#include <tuple>

#include "base/barrier_closure.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace policy {

ManagementStatusProvider::ManagementStatusProvider() = default;
ManagementStatusProvider::ManagementStatusProvider(
    const std::string& cache_pref_name)
    : cache_pref_name_(cache_pref_name) {}
ManagementStatusProvider::~ManagementStatusProvider() = default;

EnterpriseManagementAuthority ManagementStatusProvider::GetAuthority() {
  if (!RequiresCache())
    return FetchAuthority();

  if (absl::holds_alternative<PrefService*>(cache_) &&
      absl::get<PrefService*>(cache_) &&
      absl::get<PrefService*>(cache_)->HasPrefPath(cache_pref_name_))
    return static_cast<EnterpriseManagementAuthority>(
        absl::get<PrefService*>(cache_)->GetInteger(cache_pref_name_));

  if (absl::holds_alternative<scoped_refptr<PersistentPrefStore>>(cache_) &&
      absl::holds_alternative<scoped_refptr<PersistentPrefStore>>(cache_)) {
    const base::Value* value = nullptr;
    if (absl::get<scoped_refptr<PersistentPrefStore>>(cache_)->GetValue(
            cache_pref_name_, &value) &&
        value->is_int())
      return static_cast<EnterpriseManagementAuthority>(value->GetInt());
  }
  return EnterpriseManagementAuthority::NONE;
}

bool ManagementStatusProvider::RequiresCache() const {
  return !cache_pref_name_.empty();
}

void ManagementStatusProvider::UpdateCache(
    EnterpriseManagementAuthority authority) {
  DCHECK(absl::holds_alternative<PrefService*>(cache_))
      << "A PrefService is required to refresh the management "
         "status provider cache.";
  absl::get<PrefService*>(cache_)->SetInteger(cache_pref_name_, authority);
}

void ManagementStatusProvider::UsePrefStoreAsCache(
    scoped_refptr<PersistentPrefStore> pref_store) {
  DCHECK(!cache_pref_name_.empty())
      << "This management status provider does not support caching";
  cache_ = pref_store;
}

void ManagementStatusProvider::UsePrefServiceAsCache(PrefService* prefs) {
  DCHECK(!cache_pref_name_.empty())
      << "This management status provider does not support caching";
  cache_ = prefs;
}

ManagementService::ManagementService(
    std::vector<std::unique_ptr<ManagementStatusProvider>> providers)
    : management_status_providers_(std::move(providers)) {}

ManagementService::~ManagementService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ManagementService::UsePrefServiceAsCache(PrefService* prefs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& provider : management_status_providers_) {
    if (provider->RequiresCache())
      provider->UsePrefServiceAsCache(prefs);
  }
}

void ManagementService::UsePrefStoreAsCache(
    scoped_refptr<PersistentPrefStore> pref_store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& provider : management_status_providers_) {
    if (provider->RequiresCache())
      provider->UsePrefStoreAsCache(pref_store);
  }
}

void ManagementService::RefreshCache(CacheRefreshCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ManagementAuthorityTrustworthiness previous =
      GetManagementAuthorityTrustworthiness();
  for (const auto& provider : management_status_providers_) {
    if (provider->RequiresCache()) {
      provider->UpdateCache(provider->FetchAuthority());
    }

    ManagementAuthorityTrustworthiness next =
        GetManagementAuthorityTrustworthiness();
    if (callback)
      std::move(callback).Run(previous, next);
  }
}

ui::ImageModel* ManagementService::GetManagementIcon() {
  return nullptr;
}

bool ManagementService::HasManagementAuthority(
    EnterpriseManagementAuthority authority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetManagementAuthorities() & authority;
}

ManagementAuthorityTrustworthiness
ManagementService::GetManagementAuthorityTrustworthiness() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (HasManagementAuthority(EnterpriseManagementAuthority::CLOUD_DOMAIN))
    return ManagementAuthorityTrustworthiness::FULLY_TRUSTED;
  if (HasManagementAuthority(EnterpriseManagementAuthority::CLOUD))
    return ManagementAuthorityTrustworthiness::TRUSTED;
  if (HasManagementAuthority(EnterpriseManagementAuthority::DOMAIN_LOCAL))
    return ManagementAuthorityTrustworthiness::TRUSTED;
  if (HasManagementAuthority(EnterpriseManagementAuthority::COMPUTER_LOCAL))
    return ManagementAuthorityTrustworthiness::LOW;
  return ManagementAuthorityTrustworthiness::NONE;
}

int ManagementService::GetManagementAuthorities() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (management_authorities_for_testing_)
    return management_authorities_for_testing_.value();

  int result = 0;
  for (const auto& provider : management_status_providers_)
    result |= provider->GetAuthority();
  return result;
}

void ManagementService::SetManagementStatusProviderForTesting(
    std::vector<std::unique_ptr<ManagementStatusProvider>> providers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetManagementStatusProvider(std::move(providers));
}

void ManagementService::SetManagementAuthoritiesForTesting(
    int management_authorities) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  management_authorities_for_testing_ = management_authorities;
}

void ManagementService::ClearManagementAuthoritiesForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  management_authorities_for_testing_.reset();
}

// static
void ManagementService::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
#if BUILDFLAG(IS_WIN)
  registry->RegisterIntegerPref(policy_prefs::kAzureActiveDirectoryManagement,
                                NONE);
  registry->RegisterIntegerPref(policy_prefs::kEnterpriseMDMManagementWindows,
                                NONE);
#elif BUILDFLAG(IS_MAC)
  registry->RegisterIntegerPref(policy_prefs::kEnterpriseMDMManagementMac,
                                NONE);
#endif
}

bool ManagementService::IsManaged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetManagementAuthorityTrustworthiness() >
         ManagementAuthorityTrustworthiness::NONE;
}

bool ManagementService::IsAccountManaged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return HasManagementAuthority(policy::EnterpriseManagementAuthority::CLOUD);
}

bool ManagementService::IsBrowserManaged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return HasManagementAuthority(
             policy::EnterpriseManagementAuthority::CLOUD_DOMAIN) ||
         HasManagementAuthority(
             policy::EnterpriseManagementAuthority::DOMAIN_LOCAL) ||
         HasManagementAuthority(
             policy::EnterpriseManagementAuthority::COMPUTER_LOCAL);
}

void ManagementService::SetManagementStatusProvider(
    std::vector<std::unique_ptr<ManagementStatusProvider>> providers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  management_status_providers_ = std::move(providers);
}

void ManagementService::AddManagementStatusProvider(
    std::unique_ptr<ManagementStatusProvider> provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  management_status_providers_.push_back(std::move(provider));
}

}  // namespace policy
