// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_trials/browser/leveldb_persistence_provider.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"
#include "components/origin_trials/common/persisted_trial_token.h"
#include "components/origin_trials/proto/db_trial_token.pb.h"
#include "components/origin_trials/proto/proto_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/schemeful_site.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "url/origin.h"

namespace origin_trials {

namespace {

// Check to see if |token_origin_host| has any of the strings in
// |partition_sites| as a suffix.
bool HasFirstPartyPartition(
    const url::Origin token_origin_host,
    const google::protobuf::RepeatedPtrField<std::string>& partition_sites) {
  std::string host_key = net::SchemefulSite(token_origin_host).Serialize();
  for (const std::string& site : partition_sites) {
    if (site == host_key) {
      return true;
    }
  }
  return false;
}

const base::FilePath::StringPieceType kPersistentTrialTokenDbPath =
    FILE_PATH_LITERAL("PersistentOriginTrials");

std::unique_ptr<LevelDbPersistenceProvider::DbLoadResult> BuildMapFromDb(
    std::unique_ptr<std::vector<origin_trials_pb::TrialTokenDbEntries>>
        entries) {
  // Build new maps
  std::unique_ptr<OriginTrialMap> new_origin_trial_map =
      std::make_unique<OriginTrialMap>();
  std::unique_ptr<SiteOriginsMap> new_site_origins_map =
      std::make_unique<SiteOriginsMap>();

  // Keep track of values to clean up
  blink::TrialTokenValidator validator;
  std::unique_ptr<ProtoKeyVector> keys_to_delete =
      std::make_unique<ProtoKeyVector>();
  std::unique_ptr<OriginTrialMap> entries_to_update =
      std::make_unique<OriginTrialMap>();
  base::Time current_time = base::Time::Now();

  // Move values out of the protobuffer to avoid allocating new strings
  for (auto& entry : *entries) {
    origin_trials_pb::OriginMessage* source_origin = entry.mutable_origin();
    url::Origin origin = url::Origin::CreateFromNormalizedTuple(
        std::move(*source_origin->mutable_scheme()),
        std::move(*source_origin->mutable_host()), source_origin->port());
    SiteKey mapKey = LevelDbPersistenceProvider::GetSiteKey(origin);
    base::flat_set<PersistedTrialToken> new_tokens;
    auto* mutable_tokens = entry.mutable_tokens();
    size_t stored_count = entry.mutable_tokens()->size();

    for (auto it = mutable_tokens->begin(); it < mutable_tokens->end(); ++it) {
      base::Time token_expiry =
          origin_trials_pb::DeserializeTime(it->token_expiry());
      auto usage_restriction = static_cast<blink::TrialToken::UsageRestriction>(
          it->usage_restriction());
      bool valid = validator.RevalidateTokenAndTrial(
          it->trial_name(), token_expiry, usage_restriction,
          it->token_signature(), current_time);

      if (valid) {
        UMA_HISTOGRAM_COUNTS_100(
            "OriginTrials.PersistentOriginTrial.PartitionSetSize",
            it->partition_sites().size());
        UMA_HISTOGRAM_BOOLEAN(
            "OriginTrials.PersistentOriginTrial.TokenHasFirstPartyPartition",
            HasFirstPartyPartition(origin, it->partition_sites()));
        // Move the strings out of the protobuffer to avoid allocations
        base::flat_set<std::string> partition_sites;
        for (std::string& site : *it->mutable_partition_sites()) {
          partition_sites.insert(std::move(site));
        }
        new_tokens.emplace(it->match_subdomains(),
                           std::move(*it->mutable_trial_name()), token_expiry,
                           usage_restriction,
                           std::move(*it->mutable_token_signature()),
                           std::move(partition_sites));
      }
    }

    if (new_tokens.empty()) {
      keys_to_delete->push_back(origin.Serialize());
    } else if (new_tokens.size() < stored_count) {
      entries_to_update->insert_or_assign(origin, new_tokens);
    }

    if (!new_tokens.empty()) {
      new_origin_trial_map->insert_or_assign(origin, std::move(new_tokens));

      SiteKey site_key = LevelDbPersistenceProvider::GetSiteKey(origin);
      auto find_it = new_site_origins_map->find(site_key);
      if (find_it == new_site_origins_map->end()) {
        new_site_origins_map->insert_or_assign(
            std::move(site_key),
            base::flat_set<url::Origin>({std::move(origin)}));
      } else {
        find_it->second.insert(std::move(origin));
      }
    }
  }
  return std::make_unique<LevelDbPersistenceProvider::DbLoadResult>(
      std::move(new_origin_trial_map), std::move(keys_to_delete),
      std::move(entries_to_update), std::move(new_site_origins_map));
}

}  // namespace

// Public constructor
LevelDbPersistenceProvider::LevelDbPersistenceProvider(
    const base::FilePath& profile_dir,
    leveldb_proto::ProtoDatabaseProvider* db_provider)
    : LevelDbPersistenceProvider(
          db_provider->GetDB<origin_trials_pb::TrialTokenDbEntries>(
              leveldb_proto::ProtoDbType::PERSISTENT_ORIGIN_TRIALS,
              profile_dir.Append(kPersistentTrialTokenDbPath),
              base::ThreadPool::CreateSequencedTaskRunner(
                  {base::MayBlock(), base::TaskPriority::USER_BLOCKING}))) {}

// Private constructor
LevelDbPersistenceProvider::LevelDbPersistenceProvider(
    std::unique_ptr<
        leveldb_proto::ProtoDatabase<origin_trials_pb::TrialTokenDbEntries>>
        database)
    : db_loaded_(false),
      db_(std::move(database)),
      trial_status_cache_(std::make_unique<OriginTrialMap>()),
      site_origins_map_(std::make_unique<SiteOriginsMap>()),
      database_load_start_(base::TimeTicks::Now()),
      lookups_before_db_loaded_(0) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  db_->Init(base::BindOnce(&LevelDbPersistenceProvider::OnDbInitialized,
                           weak_ptr_factory_.GetWeakPtr()));
}

// static
std::unique_ptr<LevelDbPersistenceProvider>
LevelDbPersistenceProvider::CreateForTesting(
    std::unique_ptr<
        leveldb_proto::ProtoDatabase<origin_trials_pb::TrialTokenDbEntries>>
        db) {
  return base::WrapUnique(new LevelDbPersistenceProvider(std::move(db)));
}

LevelDbPersistenceProvider::~LevelDbPersistenceProvider() = default;

void LevelDbPersistenceProvider::OnDbInitialized(
    leveldb_proto::Enums::InitStatus status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::UmaHistogramBoolean(
      "OriginTrials.PersistentOriginTrial.LevelDbInitSuccess",
      status == leveldb_proto::Enums::kOK);
  if (status != leveldb_proto::Enums::kOK) {
    DLOG(ERROR)
        << "PersistentTrialToken database initialization failed with status: "
        << status;
    return;
  }
  // Trigger a cache load from the DB
  db_->LoadEntries(base::BindOnce(&LevelDbPersistenceProvider::OnDbLoad,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void LevelDbPersistenceProvider::OnDbLoad(
    bool success,
    std::unique_ptr<std::vector<origin_trials_pb::TrialTokenDbEntries>>
        entries) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::UmaHistogramBoolean(
      "OriginTrials.PersistentOriginTrial.LevelDbLoadSuccess", success);
  if (!success)
    return;

  base::UmaHistogramCounts1000(
      "OriginTrials.PersistentOriginTrial.LevelDbLoadSize", entries->size());

  // Converting the proto data to a flat_map is potentially expensive, so do it
  // asynchronously.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&BuildMapFromDb, std::move(entries)),
      base::BindOnce(&LevelDbPersistenceProvider::OnMapBuild,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LevelDbPersistenceProvider::OnMapBuild(
    std::unique_ptr<DbLoadResult> result) {
  base::UmaHistogramCounts100(
      "OriginTrials.PersistentOriginTrial.OriginsAddedBeforeDbLoad",
      trial_status_cache_->size());

  // Add/update any keys that were inserted after the data load was triggered.
  MergeCacheIntoLoadResult(*result);

  // Activate the new cache.
  trial_status_cache_.swap(result->result_origin_trial_map);
  site_origins_map_.swap(result->result_site_origins_map);

  // Update the database with any changes or deletions caused by expired
  // tokens or pre-load writes.
  if (!result->updated_entries->empty() || !result->expired_keys->empty()) {
    std::unique_ptr<ProtoKeyEntryVector> update_vector =
        std::make_unique<ProtoKeyEntryVector>();
    for (auto& update : *(result->updated_entries)) {
      update_vector->emplace_back(
          update.first.Serialize(),
          origin_trials_pb::ProtoFromTokens(update.first, update.second));
    }
    db_->UpdateEntries(std::move(update_vector),
                       std::move(result->expired_keys), base::DoNothing());
  }

  // Done loading, record how long it took.
  base::UmaHistogramTimes("OriginTrials.PersistentOriginTrial.LevelDbLoadTime",
                          base::TimeTicks::Now() - database_load_start_);
  base::UmaHistogramCounts100(
      "OriginTrials.PersistentOriginTrial.OriginLookupsBeforeDbLoad",
      lookups_before_db_loaded_);
  db_loaded_ = true;  // Stop counting
}

void LevelDbPersistenceProvider::MergeCacheIntoLoadResult(
    DbLoadResult& result) {
  // Merge the `site_origins_map_` into the `result.result_site_origins_map`
  // to update the mappings created loading the token entries from the database.
  for (const auto& [cache_site, cache_origins] : *site_origins_map_) {
    auto find_iter = result.result_site_origins_map->find(cache_site);
    if (find_iter == result.result_site_origins_map->end()) {
      // This is a completely new site, add it directly.
      (*result.result_site_origins_map)[cache_site] = cache_origins;

    } else {
      // The `result.result_site_origins_map` already has a set of origins for
      // the `cache_site`, merge the `cache_origins` into it.
      find_iter->second.insert(cache_origins.begin(), cache_origins.end());
    }
  }

  // Merge the |trial_status_cache_| into the |result.result_origin_trial_map|
  // to update the entries loaded from the database.
  for (const auto& [cache_origin, cache_tokens] : *trial_status_cache_) {
    auto find_iter = result.result_origin_trial_map->find(cache_origin);
    if (find_iter == result.result_origin_trial_map->end()) {
      // This is a completely new origin, add it directly.
      (*result.result_origin_trial_map)[cache_origin] = cache_tokens;

    } else {
      // The |result.result_origin_trial_map| already has a set of tokens for
      // the |cache_origin|, merge the |cache_tokens| into the
      // |result.result_origin_trial_map|.
      base::flat_set<PersistedTrialToken>& destination_set = find_iter->second;
      for (const PersistedTrialToken& token : cache_tokens) {
        auto token_iter = destination_set.find(token);
        if (token_iter == destination_set.end()) {
          // No comparable token exists in the set, insert the new one
          destination_set.insert(token);
        } else {
          // The token already exists, insert the cached partition sites.
          token_iter->partition_sites.insert(token.partition_sites.begin(),
                                             token.partition_sites.end());
        }
      }
    }
  }

  // Ensure changes are written back to the database as well by updating
  // modified entries in |result.updated_entries|.
  for (const auto& [cache_origin, ignored] : *trial_status_cache_) {
    (*result.updated_entries)[cache_origin] =
        (*result.result_origin_trial_map)[cache_origin];
  }
}

void LevelDbPersistenceProvider::UpdateSiteToOriginsMap(
    const url::Origin& origin,
    bool insert) {
  // If `insert` is false, removes `origin` from the set of origins for its
  // SiteKey. If `insert` is true, adds `origin` to the set of origins for its
  // SiteKey.
  SiteKey site_key = GetSiteKey(origin);
  auto find_it = site_origins_map_->find(site_key);
  bool site_mapped = (find_it != site_origins_map_->end());
  bool origin_mapped = (site_mapped && find_it->second.contains(origin));

  bool needs_create = insert && !site_mapped;
  bool needs_update = insert && (site_mapped && !origin_mapped);
  bool needs_delete = !insert && origin_mapped;

  if (needs_delete) {
    // There exists an origin set for `site_key` and `origin` needs to be
    // removed from it.
    find_it->second.erase(origin);

    if (find_it->second.empty()) {
      site_origins_map_->erase(find_it);
    }
  } else if (needs_update) {
    // There exists an origin set for `site_key` and `origin` needs to be
    // added to it.
    find_it->second.insert(origin);
  } else if (needs_create) {
    // There is not an existing origin set for `site_key`, but one needs to be
    // created (containing `origin`).
    site_origins_map_->insert_or_assign(find_it, site_key,
                                        base::flat_set<url::Origin>({origin}));
  }
}

base::flat_set<PersistedTrialToken>
LevelDbPersistenceProvider::GetPersistentTrialTokens(
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!db_loaded_)
    lookups_before_db_loaded_++;

  auto find_it = trial_status_cache_->find(origin);
  if (find_it != trial_status_cache_->end())
    return find_it->second;
  return base::flat_set<PersistedTrialToken>();
}

SiteOriginTrialTokens
LevelDbPersistenceProvider::GetPotentialPersistentTrialTokens(
    const url::Origin& origin) {
  SiteOriginTrialTokens site_tokens;
  auto site_iter = site_origins_map_->find(GetSiteKey(origin));

  if (site_iter == site_origins_map_->end()) {
    return site_tokens;
  }

  for (const auto& token_origin : site_iter->second) {
    auto token_origin_iter = trial_status_cache_->find(token_origin);
    if (token_origin_iter != trial_status_cache_->end()) {
      site_tokens.emplace_back(token_origin, token_origin_iter->second);
    }
  }

  return site_tokens;
}

void LevelDbPersistenceProvider::SavePersistentTrialTokens(
    const url::Origin& origin,
    const base::flat_set<PersistedTrialToken>& tokens) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Check if we need to update or delete the entry
  auto find_it = trial_status_cache_->find(origin);
  bool needs_delete = tokens.empty() && find_it != trial_status_cache_->end();
  bool needs_update =
      !tokens.empty() &&
      (find_it == trial_status_cache_->end() || find_it->second != tokens);

  if (!(needs_delete || needs_update))
    return;

  if (needs_update)
    trial_status_cache_->insert_or_assign(find_it, origin, tokens);
  else
    trial_status_cache_->erase(find_it);

  UpdateSiteToOriginsMap(origin, !needs_delete);

  if (db_loaded_) {
    std::string origin_key = origin.Serialize();
    std::unique_ptr<ProtoKeyEntryVector> entries_to_save =
        std::make_unique<ProtoKeyEntryVector>();
    std::unique_ptr<ProtoKeyVector> keys_to_remove =
        std::make_unique<ProtoKeyVector>();

    if (needs_update) {
      entries_to_save->emplace_back(
          origin_key, origin_trials_pb::ProtoFromTokens(origin, tokens));
    } else
      keys_to_remove->push_back(origin_key);

    db_->UpdateEntries(std::move(entries_to_save), std::move(keys_to_remove),
                       base::DoNothing());
  }
}

void LevelDbPersistenceProvider::ClearPersistedTokens() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  trial_status_cache_ = std::make_unique<OriginTrialMap>();
  site_origins_map_ = std::make_unique<SiteOriginsMap>();
  db_->UpdateEntriesWithRemoveFilter(
      std::make_unique<ProtoKeyEntryVector>(),
      base::BindRepeating([](const std::string& key) { return true; }),
      base::DoNothing());
}

LevelDbPersistenceProvider::DbLoadResult::DbLoadResult(
    std::unique_ptr<OriginTrialMap> new_origin_trial_map,
    std::unique_ptr<ProtoKeyVector> keys_to_delete,
    std::unique_ptr<OriginTrialMap> entries_to_update,
    std::unique_ptr<SiteOriginsMap> new_site_origins_map)
    : result_origin_trial_map(std::move(new_origin_trial_map)),
      expired_keys(std::move(keys_to_delete)),
      updated_entries(std::move(entries_to_update)),
      result_site_origins_map(std::move(new_site_origins_map)) {}

LevelDbPersistenceProvider::DbLoadResult::~DbLoadResult() = default;

}  // namespace origin_trials
