// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_STRIKE_DATABASE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/strike_databases/strike_database_base.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace autofill {

extern const base::FilePath::StringPieceType kStrikeDatabaseFileName;

// Manages data on whether different Autofill opportunities should be offered to
// the user. Projects can earn strikes in a number of ways; for instance, if a
// user ignores or declines a prompt, or if a user accepts a prompt but the task
// fails.
// This class is a Singleton which contains StrikeData information for all
// projects. It should not be used directly, but rather by implementing the
// StrikeDatabaseIntegratorBase (which contains a pointer to StrikeDatabase)
// for specific projects.
class StrikeDatabase : public StrikeDatabaseBase {
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

  // StrikeDatabaseBase:
  int AddStrikes(int strikes_increase, const std::string& key) override;
  int RemoveStrikes(int strikes_decrease, const std::string& key) override;
  int GetStrikes(const std::string& key) override;
  void ClearStrikes(const std::string& key) override;
  std::vector<std::string> GetAllStrikeKeysForProject(
      const std::string& project_prefix) override;
  void ClearStrikesForKeys(
      const std::vector<std::string>& keys_to_remove) override;
  void ClearAllStrikesForProject(const std::string& project_prefix) override;
  void ClearAllStrikes() override;
  std::string GetPrefixFromKey(const std::string& key) const override;
  void SetStrikeData(const std::string& key, int num_strikes) override;
  int64_t GetLastUpdatedTimestamp(const std::string& key) override;

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

  // StrikeDatabaseBase:
  std::map<std::string, StrikeData>& GetStrikeCache() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeBrowsingDataRemoverDelegateTest,
                           StrikeDatabaseEmptyOnAutofillRemoveEverything);
  FRIEND_TEST_ALL_PREFIXES(CreditCardSaveStrikeDatabaseTest,
                           GetKeyForCreditCardSaveTest);
  FRIEND_TEST_ALL_PREFIXES(CreditCardSaveStrikeDatabaseTest,
                           GetIdForCreditCardSaveTest);
  FRIEND_TEST_ALL_PREFIXES(CreditCardSaveStrikeDatabaseTest,
                           RemoveExpiredStrikesOnLoadTest);
  friend class FakeCreditCardServer;
  friend class StrikeDatabaseTest;
  friend class StrikeDatabaseTester;

  void OnDatabaseInit(leveldb_proto::Enums::InitStatus status);

  void OnDatabaseLoadKeysAndEntries(
      bool success,
      std::unique_ptr<std::map<std::string, StrikeData>> entries);

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

  // Same as |ClearAllProtoStrikesForKey()| but for a vector of |keys|.
  virtual void ClearAllProtoStrikesForKeys(
      const std::vector<std::string>& keys,
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
  static void OnGetProtoStrikes(StrikesCallback outer_callback,
                                bool success,
                                std::unique_ptr<StrikeData> strike_data);

  // Exposed for testing purposes.
  void LoadKeys(const LoadKeysCallback& callback);

  // Sets the entry for |key| in |strike_map_cache_| to |data|.
  void UpdateCache(const std::string& key, const StrikeData& data);

  base::WeakPtrFactory<StrikeDatabase> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_STRIKE_DATABASE_H_
