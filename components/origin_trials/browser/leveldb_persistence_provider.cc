// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_trials/browser/leveldb_persistence_provider.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
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
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "url/origin.h"

namespace origin_trials {

namespace {

const base::FilePath::StringPieceType kPersistentTrialTokenDbPath =
    FILE_PATH_LITERAL("PersistentOriginTrials");

std::unique_ptr<LevelDbPersistenceProvider::DbLoadResult> BuildMapFromDb(
    std::unique_ptr<std::vector<origin_trials_pb::TrialTokenDbEntries>>
        entries) {
  // Build a new map of values
  std::unique_ptr<OriginTrialMap> new_map = std::make_unique<OriginTrialMap>();

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
    url::Origin key = url::Origin::CreateFromNormalizedTuple(
        std::move(*source_origin->mutable_scheme()),
        std::move(*source_origin->mutable_host()), source_origin->port());
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
        new_tokens.emplace(std::move(*it->mutable_trial_name()), token_expiry,
                           usage_restriction,
                           std::move(*it->mutable_token_signature()));
      }
    }

    if (new_tokens.empty())
      keys_to_delete->push_back(key.Serialize());
    else if (new_tokens.size() < stored_count)
      entries_to_update->insert_or_assign(key, new_tokens);

    if (!new_tokens.empty())
      new_map->insert_or_assign(std::move(key), std::move(new_tokens));
  }
  return std::make_unique<LevelDbPersistenceProvider::DbLoadResult>(
      std::move(new_map), std::move(keys_to_delete),
      std::move(entries_to_update));
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

  // Add/update any keys that were inserted after the data load was triggered,
  // and persist them to the database.
  for (const auto& pair : *trial_status_cache_) {
    result->result_map->insert_or_assign(pair.first, pair.second);
    result->updated_entries->insert_or_assign(pair.first, pair.second);
  }
  trial_status_cache_.swap(result->result_map);

  // Update the database with any changes or deletions caused by expired
  // tokens.
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
  db_->UpdateEntriesWithRemoveFilter(
      std::make_unique<ProtoKeyEntryVector>(),
      base::BindRepeating([](const std::string& key) { return true; }),
      base::DoNothing());
}

LevelDbPersistenceProvider::DbLoadResult::DbLoadResult(
    std::unique_ptr<OriginTrialMap> new_map,
    std::unique_ptr<ProtoKeyVector> keys_to_delete,
    std::unique_ptr<OriginTrialMap> entries_to_update)
    : result_map(std::move(new_map)),
      expired_keys(std::move(keys_to_delete)),
      updated_entries(std::move(entries_to_update)) {}

LevelDbPersistenceProvider::DbLoadResult::~DbLoadResult() = default;

}  // namespace origin_trials
