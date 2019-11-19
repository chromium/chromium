// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_STRIKE_DATABASE_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "components/autofill/core/browser/payments/strike_database.h"

namespace autofill {

// An in-memory-only test version of StrikeDatabase.
class TestStrikeDatabase : public StrikeDatabase {
 public:
  TestStrikeDatabase();
  ~TestStrikeDatabase() override;

  // StrikeDatabase:
  void GetProtoStrikes(const std::string& key,
                       const StrikesCallback& outer_callback) override;
  void ClearAllProtoStrikes(
      const ClearStrikesCallback& outer_callback) override;
  void ClearAllProtoStrikesForKey(
      const std::string& key,
      const ClearStrikesCallback& outer_callback) override;

  // TestStrikeDatabase:
  void AddEntryWithNumStrikes(const std::string& key, int num_strikes);
  int GetStrikesForTesting(const std::string& key);

 private:
  // In-memory database of StrikeData.
  std::unordered_map<std::string, StrikeData> db_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_STRIKE_DATABASE_H_
