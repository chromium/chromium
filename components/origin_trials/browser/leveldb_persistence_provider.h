// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_TRIALS_BROWSER_LEVELDB_PERSISTENCE_PROVIDER_H_
#define COMPONENTS_ORIGIN_TRIALS_BROWSER_LEVELDB_PERSISTENCE_PROVIDER_H_

#include "components/origin_trials/common/origin_trials_persistence_provider.h"

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace leveldb_proto {
class ProtoDatabaseProvider;
}

namespace origin_trials_pb {
class TrialTokenDbEntries;
}

namespace url {
class Origin;
}

namespace origin_trials {

using SiteOriginsMap = base::flat_map<SiteKey, base::flat_set<url::Origin>>;
using OriginTrialMap =
    base::flat_map<url::Origin, base::flat_set<PersistedTrialToken>>;

using ProtoKeyVector = std::vector<std::string>;
using ProtoEntryVector = std::vector<origin_trials_pb::TrialTokenDbEntries>;
using ProtoKeyEntryVector =
    std::vector<std::pair<std::string, origin_trials_pb::TrialTokenDbEntries>>;

// Persistence for Persistent Origin Trials based on LevelDB, with an in-memory
// cache to ensure quick and synchronous reads.
class LevelDbPersistenceProvider : public OriginTrialsPersistenceProvider {
 public:
  // Multiple value return type for async building of the cache.
  struct DbLoadResult {
    DbLoadResult(std::unique_ptr<OriginTrialMap> new_origin_trial_map,
                 std::unique_ptr<ProtoKeyVector> keys_to_delete,
                 std::unique_ptr<OriginTrialMap> entries_to_update,
                 std::unique_ptr<SiteOriginsMap> new_site_origins_map);
    ~DbLoadResult();
    std::unique_ptr<OriginTrialMap> result_origin_trial_map;
    std::unique_ptr<ProtoKeyVector> expired_keys;
    std::unique_ptr<OriginTrialMap> updated_entries;
    std::unique_ptr<SiteOriginsMap> result_site_origins_map;
  };

  LevelDbPersistenceProvider(
      const base::FilePath& profile_dir,
      leveldb_proto::ProtoDatabaseProvider* database_provider);
  ~LevelDbPersistenceProvider() override;

  // Create an instance for testing that uses the provided |db| instance
  static std::unique_ptr<LevelDbPersistenceProvider> CreateForTesting(
      std::unique_ptr<
          leveldb_proto::ProtoDatabase<origin_trials_pb::TrialTokenDbEntries>>
          db);

  // |origin_trials::OriginTrialsPersistenceProvider|
  base::flat_set<PersistedTrialToken> GetPersistentTrialTokens(
      const url::Origin& origin) override;
  SiteOriginTrialTokens GetPotentialPersistentTrialTokens(
      const url::Origin& origin) override;
  void SavePersistentTrialTokens(
      const url::Origin& origin,
      const base::flat_set<PersistedTrialToken>& tokens) override;
  void ClearPersistedTokens() override;

 private:
  bool db_loaded_;
  // Proto db for storing all the entries, keyed by serialized origin.
  std::unique_ptr<
      leveldb_proto::ProtoDatabase<origin_trials_pb::TrialTokenDbEntries>>
      db_;
  std::unique_ptr<OriginTrialMap> trial_status_cache_;
  std::unique_ptr<SiteOriginsMap> site_origins_map_;

  // Used to report total load time
  base::TimeTicks database_load_start_;
  // Used to report number of lookups before load
  uint32_t lookups_before_db_loaded_;

  base::WeakPtrFactory<LevelDbPersistenceProvider> weak_ptr_factory_{this};

  explicit LevelDbPersistenceProvider(
      std::unique_ptr<
          leveldb_proto::ProtoDatabase<origin_trials_pb::TrialTokenDbEntries>>
          db);

  // Callback for |db_| initialized.
  void OnDbInitialized(leveldb_proto::Enums::InitStatus status);

  // Callback for loading entries from |db_|. Builds the in-memory cache and
  // triggers clean-up of expired token values.
  void OnDbLoad(
      bool success,
      std::unique_ptr<std::vector<origin_trials_pb::TrialTokenDbEntries>>
          entries);

  // Callback to swap out the |trial_status_cache_| with one built from the
  // loaded database.
  void OnMapBuild(std::unique_ptr<DbLoadResult> result);

  // Merges the |trial_status_cache_| entries into |result|.
  // This merge ensures that any cached token and partition information is
  // preserved after the load result is applied.
  void MergeCacheIntoLoadResult(DbLoadResult& result);

  // Updates the in-memory map of SiteKeys to origins within the
  // `trial_status_cache_`.
  void UpdateSiteToOriginsMap(const url::Origin& origin, bool insert);
};

}  // namespace origin_trials

#endif  // COMPONENTS_ORIGIN_TRIALS_BROWSER_LEVELDB_PERSISTENCE_PROVIDER_H_
