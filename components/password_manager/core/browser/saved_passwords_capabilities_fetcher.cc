// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/saved_passwords_capabilities_fetcher.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <vector>

#include "base/containers/cxx20_erase.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#include "base/stl_util.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "url/gurl.h"

namespace password_manager {

SavedPasswordsCapabilitiesFetcher::SavedPasswordsCapabilitiesFetcher(
    std::unique_ptr<CapabilitiesService> fetcher,
    scoped_refptr<password_manager::PasswordStoreInterface> password_store)
    : fetcher_(std::move(fetcher)), saved_passwords_presenter_(password_store) {
  observed_saved_password_presenter_.Observe(&saved_passwords_presenter_);
  saved_passwords_presenter_.Init();
}

SavedPasswordsCapabilitiesFetcher::~SavedPasswordsCapabilitiesFetcher() =
    default;

void SavedPasswordsCapabilitiesFetcher::PrewarmCache() {
  if (!is_cache_initialized_) {
    should_refresh_cache_ = true;
    return;
  }

  if (GetCacheState() == CacheState::kStale) {
    FetchCapababilitiesForAllStoredOrigins();
  }
}

void SavedPasswordsCapabilitiesFetcher::RefreshScriptsIfNecessary(
    base::OnceClosure fetch_finished_callback) {
  CacheState state = GetCacheState();

  base::UmaHistogramEnumeration(
      "PasswordManager.SavedPasswordsCapabilitiesFetcher.CacheState", state);

  if (state == CacheState::kReady) {
    std::move(fetch_finished_callback).Run();
    return;
  }

  all_origins_callbacks_.emplace_back(std::move(fetch_finished_callback));

  switch (state) {
    case CacheState::kNeverSet:
      should_refresh_cache_ = true;
      break;
    case CacheState::kWaiting:
      // No new fetching.
      break;
    case CacheState::kStale:
      FetchCapababilitiesForAllStoredOrigins();
      break;
    case CacheState::kReady:
      NOTREACHED();
      break;
  }
}

void SavedPasswordsCapabilitiesFetcher::FetchScriptAvailability(
    const url::Origin& origin,
    ResponseCallback callback) {
  if (!is_cache_initialized_ || refresh_in_process_) {
    single_origin_callbacks_.emplace_back(origin, std::move(callback));
    return;
  }

  auto domains_it = cache_.find(origin);
  // Domain is present in the cache but stale.
  if (domains_it != cache_.end() && domains_it->second.IsResultStale()) {
    FetchCapababilitiesForSingleOrigin(origin, std::move(callback));
    return;
  }

  std::move(callback).Run(IsScriptAvailable(origin));
}

bool SavedPasswordsCapabilitiesFetcher::IsScriptAvailable(
    const url::Origin& origin) const {
  if (base::FeatureList::IsEnabled(
          password_manager::features::kForceEnablePasswordDomainCapabilities)) {
    return true;
  }

  auto domains_it = cache_.find(origin);
  // Domain not present.
  if (domains_it == cache_.end() || domains_it->second.IsResultStale()) {
    return false;
  }

  return domains_it->second.has_script;
}

void SavedPasswordsCapabilitiesFetcher::OnEdited(const PasswordForm& form) {
  // OnEdited() only gets called if a the password was edited via
  // |saved_passwords_presenter_|, so even if the password gets edited
  // elsewhere, we wouldn't end up here.
  NOTREACHED();
}

void SavedPasswordsCapabilitiesFetcher::OnSavedPasswordsChanged(
    SavedPasswordsPresenter::SavedPasswordsView passwords) {
  if (is_cache_initialized_) {
    // Request an update only if the updated set of passwords origins differ
    // from the ones in cache.
    std::vector<url::Origin> new_origins = GetOriginsOfStoredPasswords();
    std::vector<url::Origin> cached_origins;
    base::ranges::transform(cache_, std::back_inserter(cached_origins),
                            [](const auto& record) { return record.first; });

    DCHECK(std::is_sorted(new_origins.begin(), new_origins.end()));
    DCHECK(std::is_sorted(cached_origins.begin(), cached_origins.end()));
    if (!base::ranges::equal(new_origins, cached_origins)) {
      ReinitializeCache();
      should_refresh_cache_ = true;
    }
    return;
  }

  ReinitializeCache();
  is_cache_initialized_ = true;

  if (should_refresh_cache_) {
    // Fetch capabilities for all origins if refresh was requested. Once
    // refresh is finished, any pending callbacks will be resolved.
    FetchCapababilitiesForAllStoredOrigins();
  } else {
    // Answer any single-origin requests that might have come during
    // initialization.
    for (auto& [origin, callback] :
         std::exchange(single_origin_callbacks_, {})) {
      FetchCapababilitiesForSingleOrigin(std::move(origin),
                                         std::move(callback));
    }
  }
}

void SavedPasswordsCapabilitiesFetcher::
    FetchCapababilitiesForAllStoredOrigins() {
  DCHECK(!refresh_in_process_);
  // The request is being handled, so reset the boolean.
  should_refresh_cache_ = false;
  refresh_in_process_ = true;

  ReinitializeCache();

  fetcher_->QueryPasswordChangeScriptAvailability(
      GetOriginsOfStoredPasswords(),
      base::BindOnce(&SavedPasswordsCapabilitiesFetcher::
                         FetchCapababilitiesForAllStoredOriginsDone,
                     base::Unretained(this), base::TimeTicks::Now()));
}

void SavedPasswordsCapabilitiesFetcher::
    FetchCapababilitiesForAllStoredOriginsDone(
        base::TimeTicks request_start_timestamp,
        const std::set<url::Origin>& capabilities) {
  // Update |has_script| and |last_fetch_timestamp| for all origins.
  base::TimeTicks now = base::TimeTicks::Now();
  base::UmaHistogramMediumTimes(
      "PasswordManager.SavedPasswordsCapabilitiesFetcher."
      "AllOriginsResponseTime",
      now - request_start_timestamp);
  for (auto& [origin, c_result] : cache_) {
    c_result.last_fetch_timestamp = now;
    c_result.has_script = capabilities.find(origin) != capabilities.end();
  }

  refresh_in_process_ = false;

  if (should_refresh_cache_) {
    // If a new refresh request has come in the meantime, this starts another
    // refresh here instead of running all the callbacks.
    FetchCapababilitiesForAllStoredOrigins();
    return;
  }

  for (auto& callback : std::exchange(all_origins_callbacks_, {})) {
    std::move(callback).Run();
  }
  for (auto& [origin, callback] : std::exchange(single_origin_callbacks_, {})) {
    std::move(callback).Run(IsScriptAvailable(origin));
  }
}

void SavedPasswordsCapabilitiesFetcher::FetchCapababilitiesForSingleOrigin(
    const url::Origin& origin,
    ResponseCallback callback) {
  // Domain not present. Fetching is only supported for origins of stored
  // passwords.
  if (cache_.find(origin) == cache_.end()) {
    std::move(callback).Run(IsScriptAvailable(origin));
    return;
  }

  fetcher_->QueryPasswordChangeScriptAvailability(
      {origin}, base::BindOnce(&SavedPasswordsCapabilitiesFetcher::
                                   FetchCapababilitiesForSingleOriginDone,
                               base::Unretained(this), origin,
                               std::move(callback), base::TimeTicks::Now()));
}

void SavedPasswordsCapabilitiesFetcher::FetchCapababilitiesForSingleOriginDone(
    const url::Origin& origin,
    ResponseCallback callback,
    base::TimeTicks request_start_timestamp,
    const std::set<url::Origin>& capabilities) {
  base::UmaHistogramMediumTimes(
      "PasswordManager.SavedPasswordsCapabilitiesFetcher."
      "SingleOriginResponseTime",
      base::TimeTicks::Now() - request_start_timestamp);
  // Domain not present. The password might have gotten deleted since the fetch
  // was started.
  if (cache_.find(origin) == cache_.end()) {
    std::move(callback).Run(IsScriptAvailable(origin));
    return;
  }
  SavedPasswordsCapabilitiesFetcher::CapabilitiesFetchResult& c_result =
      cache_.find(origin)->second;
  c_result.last_fetch_timestamp = base::TimeTicks::Now();
  c_result.has_script = capabilities.find(origin) != capabilities.end();
  std::move(callback).Run(IsScriptAvailable(origin));
}

std::vector<url::Origin>
SavedPasswordsCapabilitiesFetcher::GetOriginsOfStoredPasswords() const {
  std::vector<url::Origin> origins;
  for (const auto& form : saved_passwords_presenter_.GetSavedPasswords()) {
    if (form.url.SchemeIs(url::kHttpScheme)) {
      // Http schemes are not supported.
      continue;
    }

    url::Origin origin = url::Origin::Create(form.url);
    if (!origin.opaque()) {
      origins.push_back(origin);
    }
  }
  std::sort(origins.begin(), origins.end());
  origins.erase(std::unique(origins.begin(), origins.end()), origins.end());
  return origins;
}

void SavedPasswordsCapabilitiesFetcher::ReinitializeCache() {
  // Restart the cache.
  cache_.clear();
  for (const auto& origin : GetOriginsOfStoredPasswords()) {
    cache_[origin] = CapabilitiesFetchResult();
  }
}

SavedPasswordsCapabilitiesFetcher::CacheState
SavedPasswordsCapabilitiesFetcher::GetCacheState() const {
  if (!is_cache_initialized_) {
    return CacheState::kNeverSet;
  }
  bool need_refresh = should_refresh_cache_ ||
                      base::ranges::any_of(cache_, [](const auto& record) {
                        return record.second.IsResultStale();
                      });
  return need_refresh
             ? (refresh_in_process_ ? CacheState::kWaiting : CacheState::kStale)
             : CacheState::kReady;
}

SavedPasswordsCapabilitiesFetcher::CapabilitiesFetchResult::
    CapabilitiesFetchResult() = default;
SavedPasswordsCapabilitiesFetcher::CapabilitiesFetchResult::
    ~CapabilitiesFetchResult() = default;

bool SavedPasswordsCapabilitiesFetcher::CapabilitiesFetchResult::IsResultStale()
    const {
  static const base::TimeDelta kCacheTimeout = base::Minutes(5);
  return last_fetch_timestamp.is_null() ||
         base::TimeTicks::Now() - last_fetch_timestamp >= kCacheTimeout;
}

base::Value::Dict
SavedPasswordsCapabilitiesFetcher::GetDebugInformationForInternals() const {
  base::Value::Dict result;

  result.Set("engine", "hash-prefix-based lookup");

  std::string cache_state;
  switch (GetCacheState()) {
    case CacheState::kReady:
      cache_state = "ready";
      break;
    case CacheState::kStale:
      cache_state = "stale";
      break;
    case CacheState::kNeverSet:
      cache_state = "never set";
      break;
    case CacheState::kWaiting:
      cache_state = "waiting";
      break;
  }
  result.Set("cache state", cache_state);

  return result;
}

base::Value::List SavedPasswordsCapabilitiesFetcher::GetCacheEntries() const {
  base::Value::List cache_entries;
  for (const auto& [origin, capabilities] : cache_) {
    base::Value::Dict entry;
    entry.Set("url", origin.Serialize());
    entry.Set("has_script", capabilities.has_script);
    cache_entries.Append(std::move(entry));
  }

  return cache_entries;
}

}  // namespace password_manager
