// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_TEST_INMEMORY_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_TEST_INMEMORY_STRIKE_DATABASE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "components/autofill/core/browser/strike_databases/strike_database_base.h"

namespace autofill {

class StrikeData;

// Simplified implementation of the StrikeDatabase that only uses a local
// cache for testing purposes.
class TestInMemoryStrikeDatabase : public StrikeDatabaseBase {
 public:
  TestInMemoryStrikeDatabase();
  ~TestInMemoryStrikeDatabase() override;

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

  // Cached StrikeDatabase entries.
  std::map<std::string, StrikeData> strike_map_cache_;

  // StrikeDatabaseBase:
  std::map<std::string, StrikeData>& GetStrikeCache() override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_TEST_INMEMORY_STRIKE_DATABASE_H_
