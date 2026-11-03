// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/management_service.h"

#include <algorithm>
#include <optional>
#include <ostream>
#include <tuple>
#include <variant>

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
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

  return GetCachedAuthority().value_or(EnterpriseManagementAuthority::NONE);
}

EnterpriseManagementAuthority
ManagementStatusProvider::GetAuthorityOrFetchBlocking() {
  if (!RequiresCache()) {
    return FetchAuthority();
  }
  if (std::optional<EnterpriseManagementAuthority> cached_authority =
          GetCachedAuthority()) {
    return *cached_authority;
  }

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  EnterpriseManagementAuthority authority = FetchAuthority();
  if (auto* pref_service =
          std::get_if<raw_ptr<PrefService, DanglingUntriaged>>(&cache_);
      pref_service && *pref_service) {
    UpdateCache(authority);
  } else {
    // Cached by `UsePrefServiceAsCache()`.
    fetched_authority_ = authority;
  }
  return authority;
}

std::optional<EnterpriseManagementAuthority>
ManagementStatusProvider::GetCachedAuthority() const {
  if (fetched_authority_) {
    return fetched_authority_;
  }

  if (auto* pref_service =
          std::get_if<raw_ptr<PrefService, DanglingUntriaged>>(&cache_);
      pref_service && *pref_service &&
      (*pref_service)->HasPrefPath(cache_pref_name_)) {
    return static_cast<EnterpriseManagementAuthority>(
        (*pref_service)->GetInteger(cache_pref_name_));
  }

  if (auto* pref_store =
          std::get_if<scoped_refptr<PersistentPrefStore>>(&cache_);
      pref_store && *pref_store) {
    const base::Value* value = nullptr;
    if ((*pref_store)->GetValue(cache_pref_name_, &value) && value->is_int()) {
      return static_cast<EnterpriseManagementAuthority>(value->GetInt());
    }
  }
  return std::nullopt;
}

bool ManagementStatusProvider::RequiresCache() const {
  return !cache_pref_name_.empty();
}

void ManagementStatusProvider::UpdateCache(
    EnterpriseManagementAuthority authority) {
  DCHECK(
      (std::holds_alternative<raw_ptr<PrefService, DanglingUntriaged>>(cache_)))
      << "A PrefService is required to refresh the management "
         "status provider cache.";
  std::get<raw_ptr<PrefService, DanglingUntriaged>>(cache_)->SetInteger(
      cache_pref_name_, authority);
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
  if (prefs && fetched_authority_) {
    UpdateCache(*fetched_authority_);
    fetched_authority_.reset();
  } else if (!prefs) {
    fetched_authority_.reset();
  }
}

void ManagementStatusProvider::FetchAuthorityAsync(
    base::OnceCallback<void(std::pair<ManagementStatusProvider*,
                                      EnterpriseManagementAuthority>)>
        callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ManagementStatusProvider::FetchAuthority,
                     base::Unretained(this)),
      base::BindOnce(
          [](base::OnceCallback<void(std::pair<ManagementStatusProvider*,
                                               EnterpriseManagementAuthority>)>
                 callback,
             base::WeakPtr<ManagementStatusProvider> provider,
             EnterpriseManagementAuthority authority) {
            if (provider) {
              std::move(callback).Run({provider.get(), authority});
            }
          },
          std::move(callback), weak_factory_.GetWeakPtr()));
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
  size_t caching_providers_count = std::count_if(
      management_status_providers_.begin(), management_status_providers_.end(),
      [](const auto& provider) { return provider->RequiresCache(); });

  if (caching_providers_count == 0) {
    if (callback) {
      // If no providers require a cache, the state remains unchanged.
      ManagementAuthorityTrustworthiness previous =
          GetManagementAuthorityTrustworthiness();
      std::move(callback).Run(previous, previous);
    }
    return;
  }

  ManagementAuthorityTrustworthiness previous =
      GetManagementAuthorityTrustworthiness();
  auto barrier = base::BarrierCallback<
      std::pair<ManagementStatusProvider*, EnterpriseManagementAuthority>>(
      caching_providers_count,
      base::BindOnce(&ManagementService::OnAuthFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     previous));

  for (const auto& provider : management_status_providers_) {
    if (provider->RequiresCache()) {
      provider->FetchAuthorityAsync(barrier);
    }
  }
}

void ManagementService::OnAuthFetched(
    CacheRefreshCallback callback,
    ManagementAuthorityTrustworthiness previous,
    std::vector<std::pair<ManagementStatusProvider*,
                          EnterpriseManagementAuthority>> results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& [provider, authority] : results) {
    provider->UpdateCache(authority);
  }

  ManagementAuthorityTrustworthiness next =
      GetManagementAuthorityTrustworthiness();
  if (callback) {
    std::move(callback).Run(previous, next);
  }
}

ui::ImageModel* ManagementService::GetManagementIconForProfile() {
  return nullptr;
}

gfx::Image* ManagementService::GetManagementIconForBrowser() {
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

ManagementAuthorityTrustworthiness
ManagementService::GetManagementAuthorityTrustworthinessForPolicyLoading() {
  return GetManagementAuthorityTrustworthinessForPolicyLoadingImpl(
      /*fetch_if_not_cached=*/false);
}

ManagementAuthorityTrustworthiness ManagementService::
    GetOrFetchManagementAuthorityTrustworthinessForPolicyLoading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AssertBlockingAllowed();
  return GetManagementAuthorityTrustworthinessForPolicyLoadingImpl(
      /*fetch_if_not_cached=*/true);
}

ManagementAuthorityTrustworthiness
ManagementService::GetManagementAuthorityTrustworthinessForPolicyLoadingImpl(
    bool fetch_if_not_cached) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_WIN)
  if (!management_authorities_for_testing_) {
    int result = 0;
    for (const auto& provider : management_status_providers_) {
      if (provider->RequiresCache() &&
          provider->cache_pref_name() ==
              policy_prefs::kAzureActiveDirectoryManagement) {
        continue;
      }
      result |= fetch_if_not_cached ? provider->GetAuthorityOrFetchBlocking()
                                    : provider->GetAuthority();
    }

    if (result & EnterpriseManagementAuthority::CLOUD_DOMAIN) {
      return ManagementAuthorityTrustworthiness::FULLY_TRUSTED;
    }
    if (result & EnterpriseManagementAuthority::CLOUD) {
      return ManagementAuthorityTrustworthiness::TRUSTED;
    }
    if (result & EnterpriseManagementAuthority::DOMAIN_LOCAL) {
      return ManagementAuthorityTrustworthiness::TRUSTED;
    }
    if (result & EnterpriseManagementAuthority::COMPUTER_LOCAL) {
      return ManagementAuthorityTrustworthiness::LOW;
    }
    return ManagementAuthorityTrustworthiness::NONE;
  }
#endif
  return GetManagementAuthorityTrustworthiness();
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
  registry->RegisterIntegerPref(
      policy_prefs::kAzureActiveDirectoryDeviceManagement, NONE);
  registry->RegisterIntegerPref(policy_prefs::kEnterpriseMDMManagementWindows,
                                NONE);
#elif BUILDFLAG(IS_MAC)
  registry->RegisterIntegerPref(policy_prefs::kEnterpriseMDMManagementMac,
                                NONE);
#elif BUILDFLAG(IS_ANDROID)
  registry->RegisterIntegerPref(policy_prefs::kEnterpriseMDMManagementAndroid,
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

void ManagementService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ManagementService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
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

void ManagementService::NotifyEnterpriseLabelUpdated() {
  for (auto& observer : observers_) {
    observer.OnEnterpriseLabelUpdated();
  }
}

void ManagementService::NotifyEnterpriseLogoForBrowserUpdated() {
  for (auto& observer : observers_) {
    observer.OnEnterpriseLogoUpdatedForBrowser();
  }
}

}  // namespace policy
