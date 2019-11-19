// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_STRIKE_DATABASE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace autofill {

namespace {
const char kKeyDeliminator[] = "__";
}  // namespace

class StrikeData;

// Manages data on whether different Autofill opportunities should be offered to
// the user. Projects can earn strikes in a number of ways; for instance, if a
// user ignores or declines a prompt, or if a user accepts a prompt but the task
// fails.
// This class is a Singleton which contains StrikeData information for all
// projects. It should not be used directly, but rather by implementing the
// StrikeDatabaseIntegratorBase (which contains a pointer to StrikeDatabase)
// for specific projects.
class StrikeDatabase : public KeyedService {
 public:
  using ClearStrikesCallback = base::RepeatingCallback<void(bool success)>;

  using GetValueCallback =
      base::RepeatingCallback<void(bool success,
                                   std::unique_ptr<StrikeData> data)>;

  using LoadKeysCallback =
      base::RepeatingCallback<void(bool success,
                                   std::unique_ptr<std::vector<std::string>>)>;

  using SetValueCallback = base::RepeatingCallback<void(bool success)>;

  using StrikesCallback = base::RepeatingCallback<void(int num_strikes)>;

  using StrikeDataProto = leveldb_proto::ProtoDatabase<StrikeData>;

  StrikeDatabase(leveldb_proto::ProtoDatabaseProvider* db_provider,
                 base::FilePath profile_path);
  ~StrikeDatabase() override;

  // Increases in-memory cache by |strikes_increase| and updates underlying
  // ProtoDatabase.
  int AddStrikes(int strikes_increase, const std::string& key);

  // Removes |strikes_decrease| in-memory cache strikes, updates
  // last_update_timestamp, and updates underlying ProtoDatabase.
  int RemoveStrikes(int strikes_decrease, const std::string& key);

  // Returns strike count from in-memory cache.
  int GetStrikes(const std::string& key);

  // Removes database entry for |key| from in-memory cache and underlying
  // ProtoDatabase.
  void ClearStrikes(const std::string& key);

  // Removes all database entries from in-memory cache and underlying
  // ProtoDatabase for the whole project.
  void ClearAllStrikesForProject(const std::string& project_prefix);

  // Removes all database entries from in-memory cache and underlying
  // ProtoDatabase.
  void ClearAllStrikes();

 protected:
  friend class StrikeDatabaseIntegratorBase;
  // Constructor for testing that does not initialize a ProtoDatabase.
  StrikeDatabase();

  // The persistent ProtoDatabase for storing strike information.
  std::unique_ptr<leveldb_proto::ProtoDatabase<StrikeData>> db_;

  // Cached StrikeDatabase entries.
  std::map<std::string, StrikeData> strike_map_cache_;

  // Whether or not the ProtoDatabase database has been initialized and entries
  // have been loaded.
  bool database_initialized_ = false;

  // Number of attempts at initializing the ProtoDatabase.
  int num_init_attempts_ = 0;

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeBrowsingDataRemoverDelegateTest,
                           StrikeDatabaseEmptyOnAutofillRemoveEverything);
  FRIEND_TEST_ALL_PREFIXES(CreditCardSaveStrikeDatabaseTest,
                           GetKeyForCreditCardSaveTest);
  FRIEND_TEST_ALL_PREFIXES(CreditCardSaveStrikeDatabaseTest,
                           GetIdForCreditCardSaveTest);
  FRIEND_TEST_ALL_PREFIXES(CreditCardSaveStrikeDatabaseTest,
                           RemoveExpiredStrikesOnLoadTest);
  friend class SaveCardInfobarEGTestHelper;
  friend class StrikeDatabaseTest;
  friend class StrikeDatabaseTester;

  void OnDatabaseInit(leveldb_proto::Enums::InitStatus status);

  void OnDatabaseLoadKeysAndEntries(
      bool success,
      std::unique_ptr<std::map<std::string, StrikeData>> entries);

  // Updates the StrikeData for |key| in the cache and ProtoDatabase to have
  // |num_strikes|, and the current time as timestamp.
  void SetStrikeData(const std::string& key, int num_strikes);

  // Passes the number of strikes for |key| to |outer_callback|. In the case
  // that the database fails to retrieve the strike update or if no entry is
  // found for |key|, 0 is passed.
  virtual void GetProtoStrikes(const std::string& key,
                               const StrikesCallback& outer_callback);

  // Removes all database entries, which ensures there will be no saved strikes
  // the next time the cache is recreated from the underlying ProtoDatabase.
  virtual void ClearAllProtoStrikes(const ClearStrikesCallback& outer_callback);

  // Removes database entry for |key|, which ensures there will be no saved
  // strikes the next time the cache is recreated from the underlying
  // ProtoDatabase.
  virtual void ClearAllProtoStrikesForKey(
      const std::string& key,
      const ClearStrikesCallback& outer_callback);

  // Passes success status and StrikeData entry for |key| to |inner_callback|.
  void GetProtoStrikeData(const std::string& key,
                          const GetValueCallback& inner_callback);

  // Sets the entry for |key| to |strike_data|. Success status is passed to the
  // callback.
  void SetProtoStrikeData(const std::string& key,
                          const StrikeData& strike_data,
                          const SetValueCallback& inner_callback);

  // Passes number of strikes to |outer_callback|.
  void OnGetProtoStrikes(StrikesCallback outer_callback,
                         bool success,
                         std::unique_ptr<StrikeData> strike_data);

  // Exposed for testing purposes.
  void LoadKeys(const LoadKeysCallback& callback);

  // Sets the entry for |key| in |strike_map_cache_| to |data|.
  void UpdateCache(const std::string& key, const StrikeData& data);

  // Extracts per-project prefix from |key|.
  std::string GetPrefixFromKey(const std::string& key);

  base::WeakPtrFactory<StrikeDatabase> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_STRIKE_DATABASE_H_
