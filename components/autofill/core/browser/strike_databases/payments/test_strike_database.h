// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_TEST_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_TEST_STRIKE_DATABASE_H_

#include <string>
#include <unordered_map>

#include "components/strike_database/strike_data.pb.h"
#include "components/strike_database/strike_database.h"

namespace autofill {

// An in-memory-only test version of StrikeDatabase.
class TestStrikeDatabase : public strike_database::StrikeDatabase {
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

 private:
  // In-memory database of StrikeData.
  std::unordered_map<std::string, strike_database::StrikeData> db_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_TEST_STRIKE_DATABASE_H_
