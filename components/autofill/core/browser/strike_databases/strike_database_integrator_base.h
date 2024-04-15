// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_STRIKE_DATABASE_INTEGRATOR_BASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_STRIKE_DATABASE_INTEGRATOR_BASE_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/bind_internal.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/strike_databases/strike_database_base.h"

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
  // The StrikeDatabase's decision on whether the feature should be blocked or
  // not, and if so, why.
  enum StrikeDatabaseDecision {
    // The feature should not be blocked.
    kDoNotBlock = 0,
    // Block feature: The maximum strike limit has been reached.
    kMaxStrikeLimitReached = 1,
    // Block feature: Not enough time has passed since the last strike.
    kRequiredDelayNotPassed = 2,
  };

  explicit StrikeDatabaseIntegratorBase(StrikeDatabaseBase* strike_database);
  virtual ~StrikeDatabaseIntegratorBase();

  // Returns the StrikeDatabase's decision on whether a particular feature
  // should be blocked (not offered) for the given `id`.
  StrikeDatabaseDecision GetStrikeDatabaseDecision(const std::string& id) const;

  // Returns the StrikeDatabase's decision on whether a particular feature
  // should be blocked (not offered).
  StrikeDatabaseDecision GetStrikeDatabaseDecision() const;

  // Returns whether a particular feature should be blocked (not offered) for
  // the given `id`. Same as calling `GetStrikeDatabaseDecision`, where a result
  // of `kDoNotBlock` returns false.
  bool ShouldBlockFeature(const std::string& id) const;

  // Returns whether a particular feature should be blocked (not offered). Same
  // as calling `GetStrikeDatabaseDecision`, where a result of `kDoNotBlock`
  // returns false.
  bool ShouldBlockFeature() const;

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
  int GetStrikes(const std::string& id = kSharedId) const;

  // Removes all database entries from in-memory cache and underlying
  // ProtoDatabase.
  void ClearStrikes(const std::string& id = kSharedId);

  // Removes all database entries from in-memory cache and underlying
  // ProtoDatabase for the whole project.
  void ClearAllStrikes();

  // Count strike entries for this project.
  size_t CountEntries() const;

 protected:
  // Runs a cleanup routine to remove the stored strike elements with the oldest
  // update timestamps when `NumberOfEntriesExceedsLimits()`. The number of
  // elements should be reduced to `GetMaximumEntriesAfterCleanup()`.
  void LimitNumberOfStoredEntries();

  // Returns true if the number of stored entries exceeds the limit.
  bool NumberOfEntriesExceedsLimits() const;

  // Removes one strike for each key where it has been longer than
  // GetExpiryTimeMicros() since |last_update_timestamp|.
  void RemoveExpiredStrikes();

  // Removes all database entries for which `id_map(ID)` is in `ids_to_delete`.
  void ClearStrikesByIdMatching(
      const std::set<std::string>& ids_to_delete,
      base::FunctionRef<std::string(const std::string&)> id_map);

  // Removes all database entries from in-memory for which `id_map(ID)` is in
  // `ids_to_delete` and were added between `delete_begin` and `delete_end`.
  void ClearStrikesByIdMatchingAndTime(
      const std::set<std::string>& ids_to_delete,
      base::Time delete_begin,
      base::Time delete_end,
      base::FunctionRef<std::string(const std::string&)> id_map);

  // Removes all database entries from in-memory cache and underlying
  // ProtoDatabase for keys in `keys`.
  void ClearStrikesForKeys(const std::vector<std::string>& keys);

  // Get a readonly reference to the cache.
  const std::map<std::string, StrikeData>& GetStrikeCache() const {
    return strike_database_->GetStrikeCache();
  }

  // Returns the id the key was built from with `GetKey(id)`.
  std::string GetIdFromKey(const std::string& key) const;

  // Returns the age of a strike entry.
  static base::TimeDelta GetEntryAge(const StrikeData& strike_data);

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeBrowsingDataRemoverDelegateTest,
                           StrikeDatabaseEmptyOnAutofillRemoveEverything);
  FRIEND_TEST_ALL_PREFIXES(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
                           ClearStrikesForKeys);
  FRIEND_TEST_ALL_PREFIXES(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
                           GetKeyForStrikeDatabaseIntegratorUniqueIdTest);
  FRIEND_TEST_ALL_PREFIXES(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
                           IdFromKey);
  FRIEND_TEST_ALL_PREFIXES(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
                           NonExpiringStrikesDoNotExpire);
  FRIEND_TEST_ALL_PREFIXES(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
                           RemoveExpiredStrikesOnlyConsidersCurrentIntegrator);
  FRIEND_TEST_ALL_PREFIXES(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
                           RemoveExpiredStrikesTest);
  FRIEND_TEST_ALL_PREFIXES(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
                           RemoveExpiredStrikesTestLogsUMA);
  FRIEND_TEST_ALL_PREFIXES(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
                           RemoveExpiredStrikesUniqueIdTest);
  friend class FakeCreditCardServer;
  friend class StrikeDatabaseTest;
  friend class StrikeDatabaseTester;

  const raw_ptr<StrikeDatabaseBase> strike_database_;

  // For projects in which strikes don't have unique identifiers, the
  // id suffix is set to |kSharedId|. This makes sure that projects requiring
  // unique IDs always specify |id| instead of relying on the default shared
  // value, while projects where unique IDs are unnecessary always fall back to
  // the default shared value.
  void CheckIdUniqueness(const std::string& id) const {
    DCHECK(UniqueIdsRequired() == (id != kSharedId));
  }

  // Generates key based on project-specific string identifier.
  std::string GetKey(const std::string& id) const;

  // Returns the maximum number of entries that should be stored for this
  // project prefix. std::nullopt means that there is no limit.
  virtual std::optional<size_t> GetMaximumEntries() const;

  // Returns the maximum number of entries that should remain after a cleanup.
  // This number should be smaller then `GetMaximumEntries()` to create some
  // headroom. std::nullopt means that `GetMaximumEntries()` should be used.
  virtual std::optional<size_t> GetMaximumEntriesAfterCleanup() const;

  // Returns a prefix unique to each project, which will be used to create
  // database key.
  virtual std::string GetProjectPrefix() const = 0;

  // Returns the maximum number of strikes after which the project's Autofill
  // opportunity stops being offered.
  virtual int GetMaxStrikesLimit() const = 0;

  // Returns the time delta after which the most recent strike should expire.
  // If the Optional is empty, then strikes don't expire.
  virtual std::optional<base::TimeDelta> GetExpiryTimeDelta() const = 0;

  // Returns whether or not a unique string identifier is required for every
  // strike in this project.
  virtual bool UniqueIdsRequired() const = 0;

  // Returns the time delta to wait for before prompting the feature again. If
  // the Optional is empty, then there is no required delay during which the
  // feature is blocked.
  virtual std::optional<base::TimeDelta> GetRequiredDelaySinceLastStrike()
      const;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_STRIKE_DATABASE_INTEGRATOR_BASE_H_
