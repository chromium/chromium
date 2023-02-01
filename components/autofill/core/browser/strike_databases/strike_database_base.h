// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_STRIKE_DATABASE_BASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_STRIKE_DATABASE_BASE_H_

#include <map>
#include <string>
#include <vector>

#include "components/keyed_service/core/keyed_service.h"

namespace autofill {

namespace {
const char kKeyDeliminator[] = "__";
}  // namespace

class StrikeData;

// Interface for the StrikeDatabase which is used by the
// StrikeDatabaseIntegratorBase that is oblivious to the underlying database
// specifics. This class is also used to allow for simpler testing.
class StrikeDatabaseBase : public KeyedService {
 public:
  StrikeDatabaseBase();
  ~StrikeDatabaseBase() override;

  // Increases in-memory cache by |strikes_increase| and updates underlying
  // ProtoDatabase.
  virtual int AddStrikes(int strikes_increase, const std::string& key) = 0;

  // Removes |strikes_decrease| in-memory cache strikes, updates
  // last_update_timestamp, and updates underlying ProtoDatabase.
  virtual int RemoveStrikes(int strikes_decrease, const std::string& key) = 0;

  // Returns strike count from in-memory cache.
  virtual int GetStrikes(const std::string& key) = 0;

  // Removes database entry for |key| from in-memory cache and underlying
  // ProtoDatabase.
  virtual void ClearStrikes(const std::string& key) = 0;

  // Returns all strike keys for |project_prefix|.
  // The returned keys still contain the |project_prefix|.
  virtual std::vector<std::string> GetAllStrikeKeysForProject(
      const std::string& project_prefix) = 0;

  // Removes database entry for keys in |keys_to_remove| from in-memory cache
  // and the underlying ProtoDatabase.
  virtual void ClearStrikesForKeys(
      const std::vector<std::string>& keys_to_remove) = 0;

  // Removes all database entries from in-memory cache and underlying
  // ProtoDatabase for the whole project.
  virtual void ClearAllStrikesForProject(const std::string& project_prefix) = 0;

  // Removes all database entries from in-memory cache and underlying
  // ProtoDatabase.
  virtual void ClearAllStrikes() = 0;

  // Extracts per-project prefix from |key|.
  virtual std::string GetPrefixFromKey(const std::string& key) const = 0;

  // Updates the StrikeData for |key| in the cache and ProtoDatabase to have
  // |num_strikes|, and the current time as timestamp.
  virtual void SetStrikeData(const std::string& key, int num_strikes) = 0;

  // Returns the timestamp when the records for |key| were last updated.
  virtual int64_t GetLastUpdatedTimestamp(const std::string& key) = 0;

 protected:
  friend class StrikeDatabaseIntegratorBase;

  // Returns a pointer to the internal cache.
  virtual std::map<std::string, StrikeData>& GetStrikeCache() = 0;

  // Returns the deliminator that separates the project identifier and the id in
  // the strike key.
  static std::string KeyDeliminator();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_STRIKE_DATABASE_BASE_H_
