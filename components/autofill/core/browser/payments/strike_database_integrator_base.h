// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_STRIKE_DATABASE_INTEGRATOR_BASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_STRIKE_DATABASE_INTEGRATOR_BASE_H_

#include "components/autofill/core/browser/payments/strike_database.h"

namespace autofill {

namespace {
static const char kSharedId[] = "shared_id";
}  // namespace

// Contains virtual functions for per-project implementations of StrikeDatabase
// to interface from, as well as a pointer to StrikeDatabase. This class is
// seperated from StrikeDatabase since we only want StrikeDatabase's cache to
// be loaded once per browser session.
class StrikeDatabaseIntegratorBase {
 public:
  StrikeDatabaseIntegratorBase(StrikeDatabase* strike_database);
  virtual ~StrikeDatabaseIntegratorBase();

  // Returns whether or not strike count for |id| has reached the strike limit
  // set by GetMaxStrikesLimit().
  bool IsMaxStrikesLimitReached(const std::string& id = kSharedId);

  // Increments in-memory cache and updates underlying ProtoDatabase.
  int AddStrike(const std::string& id = kSharedId);

  // Increases in-memory cache by |strikes_increase| and updates underlying
  // ProtoDatabase.
  int AddStrikes(int strikes_increase, const std::string& id = kSharedId);

  // Removes an in-memory cache strike, updates last_update_timestamp, and
  // updates underlying ProtoDatabase.
  int RemoveStrike(const std::string& id = kSharedId);

  // Removes |strikes_decrease| in-memory cache strikes, updates
  // |last_update_timestamp|, and updates underlying ProtoDatabase.
  int RemoveStrikes(int strikes_decrease, const std::string& id = kSharedId);

  // Returns strike count from in-memory cache.
  int GetStrikes(const std::string& id = kSharedId);

  // Removes all database entries from in-memory cache and underlying
  // ProtoDatabase.
  void ClearStrikes(const std::string& id = kSharedId);

  // Removes all database entries from in-memory cache and underlying
  // ProtoDatabase for the whole project.
  void ClearAllStrikes();

 protected:
  // Removes all strikes in which it has been longer than GetExpiryTimeMicros()
  // past |last_update_timestamp|.
  void RemoveExpiredStrikes();

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeBrowsingDataRemoverDelegateTest,
                           StrikeDatabaseEmptyOnAutofillRemoveEverything);
  FRIEND_TEST_ALL_PREFIXES(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
                           RemoveExpiredStrikesTest);
  FRIEND_TEST_ALL_PREFIXES(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
                           GetKeyForStrikeDatabaseIntegratorUniqueIdTest);
  FRIEND_TEST_ALL_PREFIXES(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
                           RemoveExpiredStrikesUniqueIdTest);
  FRIEND_TEST_ALL_PREFIXES(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
                           RemoveExpiredStrikesTestLogsUMA);
  friend class SaveCardInfobarEGTestHelper;
  friend class StrikeDatabaseTest;
  friend class StrikeDatabaseTester;

  StrikeDatabase* strike_database_;

  // For projects in which strikes don't have unique identifiers, the
  // id suffix is set to |kSharedId|. This makes sure that projects requiring
  // unique IDs always specify |id| instead of relying on the default shared
  // value, while projects where unique IDs are unnecessary always fall back to
  // the default shared value.
  void CheckIdUniqueness(std::string id) {
    DCHECK(UniqueIdsRequired() == (id != kSharedId));
  }

  // Generates key based on project-specific string identifier.
  std::string GetKey(const std::string& id);

  // Returns a prefix unique to each project, which will be used to create
  // database key.
  virtual std::string GetProjectPrefix() = 0;

  // Returns the maximum number of strikes after which the project's Autofill
  // opportunity stops being offered.
  virtual int GetMaxStrikesLimit() = 0;

  // Returns the time after which the most recent strike should expire.
  virtual long long GetExpiryTimeMicros() = 0;

  // Returns whether or not a unique string identifier is required for every
  // strike in this project.
  virtual bool UniqueIdsRequired() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_STRIKE_DATABASE_INTEGRATOR_BASE_H_
