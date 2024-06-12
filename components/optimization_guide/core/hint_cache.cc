// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/hint_cache.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/time/default_clock.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/store_update_data.h"
#include "url/gurl.h"

namespace optimization_guide {

HintCache::HintCache(
    base::WeakPtr<OptimizationGuideStore> optimization_guide_store,
    int max_memory_cache_host_keyed_hints)
    : optimization_guide_store_(optimization_guide_store),
      host_keyed_cache_(max_memory_cache_host_keyed_hints),
      url_keyed_hint_cache_(features::MaxURLKeyedHintCacheSize()),
      clock_(base::DefaultClock::GetInstance()) {}

HintCache::~HintCache() = default;

void HintCache::Initialize(bool purge_existing_data,
                           base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (optimization_guide_store_) {
    optimization_guide_store_->Initialize(
        purge_existing_data,
        base::BindOnce(&HintCache::OnStoreInitialized,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
  std::move(callback).Run();
}

std::unique_ptr<StoreUpdateData>
HintCache::MaybeCreateUpdateDataForComponentHints(
    const base::Version& version) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (optimization_guide_store_) {
    return optimization_guide_store_->MaybeCreateUpdateDataForComponentHints(
        version);
  }
  return nullptr;
}

std::unique_ptr<StoreUpdateData> HintCache::CreateUpdateDataForFetchedHints(
    base::Time update_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (optimization_guide_store_) {
    return optimization_guide_store_->CreateUpdateDataForFetchedHints(
        update_time);
  }
  return nullptr;
}

void HintCache::UpdateComponentHints(
    std::unique_ptr<StoreUpdateData> component_data,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (optimization_guide_store_) {
    DCHECK(component_data);
    optimization_guide_store_->UpdateComponentHints(std::move(component_data),
                                                    std::move(callback));
    return;
  }
  std::move(callback).Run();
}

void HintCache::UpdateFetchedHints(
    std::unique_ptr<proto::GetHintsResponse> get_hints_response,
    base::Time update_time,
    const base::flat_set<std::string>& hosts_fetched,
    const base::flat_set<GURL>& urls_fetched,
    base::OnceClosure callback) {
  std::unique_ptr<StoreUpdateData> fetched_hints_update_data =
      CreateUpdateDataForFetchedHints(update_time);

  for (const GURL& url : urls_fetched) {
    if (IsValidURLForURLKeyedHint(url)) {
      url_keyed_hint_cache_.Put(GetURLKeyedHintCacheKey(url), nullptr);
    }
  }

  if (!optimization_guide_store_) {
    // If there's not a store, add a null entry for each of the hosts that we
    // didn't have already, so we don't refetch if we didn't get a hint back
    // for it.
    for (const std::string& host : hosts_fetched) {
      if (host_keyed_cache_.Peek(host) == host_keyed_cache_.end())
        host_keyed_cache_.Put(host, nullptr);
    }
  }

  ProcessAndCacheHints(get_hints_response.get()->mutable_hints(),
                       fetched_hints_update_data.get());

  if (optimization_guide_store_) {
    optimization_guide_store_->UpdateFetchedHints(
        std::move(fetched_hints_update_data), std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void HintCache::RemoveHintsForURLs(const base::flat_set<GURL>& urls) {
  for (const GURL& url : urls) {
    auto it = url_keyed_hint_cache_.Get(GetURLKeyedHintCacheKey(url));
    if (it != url_keyed_hint_cache_.end()) {
      url_keyed_hint_cache_.Erase(it);
    }
  }
}

void HintCache::RemoveHintsForHosts(base::OnceClosure on_success,
                                    const base::flat_set<std::string>& hosts) {
  for (const std::string& host : hosts) {
    auto it = host_keyed_cache_.Get(host);
    if (it != host_keyed_cache_.end()) {
      host_keyed_cache_.Erase(it);
    }
  }

  if (optimization_guide_store_) {
    optimization_guide_store_->RemoveFetchedHintsByKey(std::move(on_success),
                                                       hosts);
    return;
  }
  std::move(on_success).Run();
}

void HintCache::PurgeExpiredFetchedHints() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (optimization_guide_store_)
    optimization_guide_store_->PurgeExpiredFetchedHints();
}

void HintCache::ClearFetchedHints() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url_keyed_hint_cache_.Clear();
  ClearHostKeyedHints();
}

void HintCache::ClearHostKeyedHints() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_keyed_cache_.Clear();

  if (optimization_guide_store_)
    optimization_guide_store_->ClearFetchedHintsFromDatabase();
}

bool HintCache::HasHint(const std::string& host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto hint_it = host_keyed_cache_.Get(host);
  if (hint_it == host_keyed_cache_.end()) {
    if (optimization_guide_store_) {
      // If not in-memory, check database.
      OptimizationGuideStore::EntryKey hint_entry_key;
      return optimization_guide_store_->FindHintEntryKey(host, &hint_entry_key);
    }
    return false;
  }

  // The hint for |host| was requested but no hint was returned.
  if (!hint_it->second)
    return true;

  MemoryHint* hint = hint_it->second.get();
  if (!hint->expiry_time() || *hint->expiry_time() > clock_->Now())
    return true;

  // The hint is expired so remove it from the cache.
  host_keyed_cache_.Erase(hint_it);
  return false;
}

void HintCache::LoadHint(const std::string& host, HintLoadedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Search for the entry key in the host-keyed cache; if it is not already
  // there, then asynchronously load it from the store and return.
  auto hint_it = host_keyed_cache_.Get(host);
  if (hint_it == host_keyed_cache_.end()) {
    if (!optimization_guide_store_) {
      std::move(callback).Run(nullptr);
      return;
    }

    OptimizationGuideStore::EntryKey hint_entry_key;
    if (!optimization_guide_store_->FindHintEntryKey(host, &hint_entry_key)) {
      std::move(callback).Run(nullptr);
      return;
    }
    optimization_guide_store_->LoadHint(
        hint_entry_key, base::BindOnce(&HintCache::OnLoadStoreHint,
                                       weak_ptr_factory_.GetWeakPtr(), host,
                                       std::move(callback)));
    return;
  }

  if (!hint_it->second) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Run the callback with the hint from the host-keyed cache.
  std::move(callback).Run(hint_it->second.get()->hint());
}

const proto::Hint* HintCache::GetHostKeyedHintIfLoaded(
    const std::string& host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Find the hint within the host-keyed cache. It will only be available here
  // if it has been loaded recently enough to be retained within the MRU cache.
  auto hint_it = host_keyed_cache_.Get(host);
  if (hint_it == host_keyed_cache_.end() || !hint_it->second)
    return nullptr;

  MemoryHint* hint = hint_it->second.get();
  if (!hint)
    return nullptr;

  // Make sure the hint is not expired before propagating it up.
  if (hint->expiry_time().has_value() && *hint->expiry_time() < clock_->Now()) {
    // The hint is expired so remove it from the cache.
    host_keyed_cache_.Erase(hint_it);
    return nullptr;
  }

  return hint->hint();
}

proto::Hint* HintCache::GetURLKeyedHint(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto hint_it = url_keyed_hint_cache_.Get(GetURLKeyedHintCacheKey(url));
  if (hint_it == url_keyed_hint_cache_.end())
    return nullptr;

  if (!hint_it->second)
    return nullptr;

  MemoryHint* hint = hint_it->second.get();
  DCHECK(hint->expiry_time().has_value());
  if (*hint->expiry_time() > clock_->Now())
    return hint->hint();

  // The hint is expired so remove it from the cache.
  url_keyed_hint_cache_.Erase(hint_it);
  return nullptr;
}

bool HintCache::HasURLKeyedEntryForURL(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsValidURLForURLKeyedHint(url)) {
    return false;
  }

  auto hint_it = url_keyed_hint_cache_.Get(GetURLKeyedHintCacheKey(url));
  if (hint_it == url_keyed_hint_cache_.end()) {
    return false;
  }

  // The url-keyed hint for the URL was requested but no hint was returned so
  // return true.
  if (!hint_it->second) {
    return true;
  }

  MemoryHint* hint = hint_it->second.get();
  DCHECK(hint->expiry_time().has_value());
  if (*hint->expiry_time() > clock_->Now())
    return true;

  // The hint is expired so remove it from the cache.
  url_keyed_hint_cache_.Erase(hint_it);
  return false;
}

base::Time HintCache::GetFetchedHintsUpdateTime() const {
  if (optimization_guide_store_)
    return optimization_guide_store_->GetFetchedHintsUpdateTime();
  return base::Time();
}

std::string HintCache::GetURLKeyedHintCacheKey(const GURL& url) const {
  return url.GetWithoutRef().spec();
}

void HintCache::OnStoreInitialized(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(optimization_guide_store_);

  std::move(callback).Run();
}

void HintCache::OnLoadStoreHint(
    const std::string& host,
    HintLoadedCallback callback,
    const OptimizationGuideStore::EntryKey& hint_entry_key,
    std::unique_ptr<MemoryHint> hint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!hint) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Check if the hint was cached in the host-keyed cache after the load was
  // requested from the store. This can occur if multiple loads for the same
  // entry key occur consecutively prior to any returning.
  auto hint_it = host_keyed_cache_.Get(host);
  if (hint_it == host_keyed_cache_.end())
    hint_it = host_keyed_cache_.Put(host, std::move(hint));

  if (!hint_it->second) {
    std::move(callback).Run(nullptr);
    return;
  }
  std::move(callback).Run(hint_it->second.get()->hint());
}

bool HintCache::ProcessAndCacheHints(
    google::protobuf::RepeatedPtrField<proto::Hint>* hints,
    optimization_guide::StoreUpdateData* update_data) {
  if (optimization_guide_store_ && !update_data) {
    // If there's no update data, then there's nothing to do.
    return false;
  }

  bool processed_hints_to_store = false;
  // Process each hint in the the hint configuration. The hints are mutable
  // because once processing is completed on each individual hint, it is moved
  // into the component update data. This eliminates the need to make any
  // additional copies of the hints.
  for (auto& hint : *hints) {
    const std::string& hint_key = hint.key();
    // Validate configuration keys.
    DCHECK(!hint_key.empty());
    if (hint_key.empty())
      continue;

    if (hint.page_hints().empty() && hint.allowlisted_optimizations().empty())
      continue;

    base::Time expiry_time =
        hint.has_max_cache_duration()
            ? clock_->Now() + base::Seconds(hint.max_cache_duration().seconds())
            : clock_->Now() + features::URLKeyedHintValidCacheDuration();

    switch (hint.key_representation()) {
      case proto::HOST:
        host_keyed_cache_.Put(
            hint_key,
            std::make_unique<MemoryHint>(
                expiry_time,
                std::make_unique<optimization_guide::proto::Hint>(hint)));
        if (update_data)
          update_data->MoveHintIntoUpdateData(std::move(hint));
        processed_hints_to_store = true;
        break;
      case proto::FULL_URL:
        if (IsValidURLForURLKeyedHint(GURL(hint_key))) {
          url_keyed_hint_cache_.Put(
              GetURLKeyedHintCacheKey(GURL(hint_key)),
              std::make_unique<MemoryHint>(expiry_time, std::move(hint)));
        }
        break;
      case proto::HOST_SUFFIX:
        // Old component versions if not updated could potentially have
        // HOST_SUFFIX hints. Just skip over them.
        break;
      case proto::HASHED_HOST:
        // The server should not send hints with hashed host key.
        NOTREACHED_IN_MIGRATION();
        break;
      case proto::REPRESENTATION_UNSPECIFIED:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
  return processed_hints_to_store;
}

void HintCache::SetClockForTesting(const base::Clock* clock) {
  clock_ = clock;
}

void HintCache::AddHintForTesting(const GURL& url,
                                  std::unique_ptr<proto::Hint> hint) {
    url_keyed_hint_cache_.Put(
        GetURLKeyedHintCacheKey(url),
        std::make_unique<MemoryHint>(base::Time::Now() + base::Days(7),
                                     std::move(hint)));
}

bool HintCache::IsHintStoreAvailable() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (optimization_guide_store_)
    return optimization_guide_store_->IsAvailable();

  return false;
}

}  // namespace optimization_guide
