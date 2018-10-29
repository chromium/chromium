// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/leveldb_proto/proto_database.h"

namespace autofill {
class StrikeData;

// Manages data on whether different Autofill opportunities should be offered to
// the user. Projects can earn strikes in a number of ways; for instance, if a
// user ignores or declines a prompt, or if a user accepts a prompt but the task
// fails.

// Here's how to create a new project type:
// 1) The keys used for this database are in form
// <ProjectTypePrefixName>__<SomeIdentifierSuffix>. In strike_database.cc, add a
// char[] variable called kKeyPrefixFor<ProjectType>.
// 2) In strike_database.h/cc, create the functions
//   GetKeyFor<ProjectType>(const std::string& identifier) and
//   GetKeyPrefixFor<ProjectType>().
// 3) Add new project type to the if block in
// StrikeDatabase::OnAddStrikeComplete(~).

class StrikeDatabase : public KeyedService {
 public:
  using ClearStrikesCallback = base::RepeatingCallback<void(bool success)>;

  using StrikesCallback = base::RepeatingCallback<void(int num_strikes)>;

  using GetValueCallback =
      base::RepeatingCallback<void(bool success,
                                   std::unique_ptr<StrikeData> data)>;

  using SetValueCallback = base::RepeatingCallback<void(bool success)>;

  using LoadKeysCallback =
      base::RepeatingCallback<void(bool success,
                                   std::unique_ptr<std::vector<std::string>>)>;

  using StrikeDataProto = leveldb_proto::ProtoDatabase<StrikeData>;

  explicit StrikeDatabase(const base::FilePath& database_dir);
  ~StrikeDatabase() override;

  // Passes the number of strikes for |key| to |outer_callback|. In the case
  // that the database fails to retrieve the strike update or if no entry is
  // found for |key|, 0 is passed.
  virtual void GetStrikes(const std::string key,
                          const StrikesCallback& outer_callback);

  // Increments strike count by 1 and passes the updated strike count to the
  // callback. In the case of |key| has no entry, a StrikeData entry with strike
  // count of 1 is added to the database. If the database fails to save or
  // retrieve the strike update, 0 is passed to |outer_callback|.
  virtual void AddStrike(const std::string key,
                         const StrikesCallback& outer_callback);

  // Removes all database entries, which implicitly resets all strike counts to
  // 0.
  virtual void ClearAllStrikes(const ClearStrikesCallback& outer_callback);

  // Removes database entry for |key|, which implicitly sets strike count to 0.
  virtual void ClearAllStrikesForKey(
      const std::string& key,
      const ClearStrikesCallback& outer_callback);

  // Returns concatenation of prefix + |card_last_four_digits| to be used as key
  // for credit card save. Expiration date is not included for privacy reasons,
  // as conflicting last-four should be a rare event, and it's not a huge issue
  // if we stop showing save bubbles a little earlier than usual in rare cases.
  std::string GetKeyForCreditCardSave(const std::string& card_last_four_digits);

 protected:
  // Constructor for testing that does not initialize a ProtoDatabase.
  StrikeDatabase();

  std::unique_ptr<leveldb_proto::ProtoDatabase<StrikeData>> db_;

 private:
  FRIEND_TEST_ALL_PREFIXES(StrikeDatabaseTest, GetPrefixFromKey);
  FRIEND_TEST_ALL_PREFIXES(ChromeBrowsingDataRemoverDelegateTest,
                           StrikeDatabaseEmptyOnAutofillRemoveEverything);
  friend class StrikeDatabaseTest;
  friend class StrikeDatabaseTester;

  void OnDatabaseInit(bool success);

  // Passes success status and StrikeData entry for |key| to |inner_callback|.
  void GetStrikeData(const std::string key,
                     const GetValueCallback& inner_callback);

  // Sets the entry for |key| to |strike_data|. Success status is passed to the
  // callback.
  void SetStrikeData(const std::string& key,
                     const StrikeData& strike_data,
                     const SetValueCallback& inner_callback);

  // Passes number of strikes to |outer_callback|.
  void OnGetStrikes(StrikesCallback outer_callback,
                    bool success,
                    std::unique_ptr<StrikeData> strike_data);

  // Updates database entry for |key| to increment num_strikes by 1, then passes
  // the updated strike count to |outer_callback|.
  void OnAddStrike(StrikesCallback outer_callback,
                   std::string key,
                   bool success,
                   std::unique_ptr<StrikeData> strike_data);

  void OnAddStrikeComplete(StrikesCallback outer_callback,
                           int num_strikes,
                           std::string key,
                           bool success);

  void OnClearAllStrikes(ClearStrikesCallback outer_callback, bool success);

  void OnClearAllStrikesForKey(ClearStrikesCallback outer_callback,
                               bool success);

  // Exposed for testing purposes.
  void LoadKeys(const LoadKeysCallback& callback);

  // Concatenates type prefix and identifier suffix to create a key.
  std::string CreateKey(const std::string& type_prefix,
                        const std::string& identifier_suffix);

  std::string GetKeyPrefixForCreditCardSave();

  std::string GetPrefixFromKey(const std::string& key);

  base::WeakPtrFactory<StrikeDatabase> weak_ptr_factory_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASE_H_
