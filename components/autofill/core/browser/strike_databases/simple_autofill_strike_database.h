// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_SIMPLE_AUTOFILL_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_SIMPLE_AUTOFILL_STRIKE_DATABASE_H_

#include <stddef.h>

#include <optional>
#include <string>
#include <string_view>

#include "base/time/time.h"
#include "components/autofill/core/browser/strike_databases/strike_database_base.h"
#include "components/autofill/core/browser/strike_databases/strike_database_integrator_base.h"

namespace autofill {

// Most strike database in Autofill don't incorporate any special logic and
// simple want to store strikes up to a given (constexpr) limit. This class
// reduces the boilerplate in such cases. Simply define the traits of the strike
// database and define the strike database as an alias:
//
// struct MyStrikeDatabaseTraits {
//   static constexpr std::string_view kName = "MyStrikeDatabase";
//   static constexpr std::optional<size_t> kMaxStrikeEntities = 100;
//   static constexpr std::optional<size_t>
//                                kMaxStrikeEntitiesAfterCleanup = 70;
//   static constexpr size_t kMaxStrikeLimit = 3;
//   static constexpr std::optional<base::TimeDelta> kExpiryTimeDelta =
//       base::Days(180);
//   static constexpr bool kUniqueIdRequired = true;
// };
//
// using MyStrikeDatabase =
//                 SimpleAutofillStrikeDatabase<MyStrikeDatabaseTraits>;
//
// If additional logic or overrides are needed, derive from this class.
template <typename Traits>
class SimpleAutofillStrikeDatabase : public StrikeDatabaseIntegratorBase {
 public:
  explicit SimpleAutofillStrikeDatabase(StrikeDatabaseBase* strike_database)
      : StrikeDatabaseIntegratorBase(strike_database) {
    RemoveExpiredStrikes();
  }
  ~SimpleAutofillStrikeDatabase() override = default;

  std::optional<size_t> GetMaximumEntries() const override {
    return Traits::kMaxStrikeEntities;
  }
  std::optional<size_t> GetMaximumEntriesAfterCleanup() const override {
    return Traits::kMaxStrikeEntitiesAfterCleanup;
  }
  std::string GetProjectPrefix() const override {
    return std::string(Traits::kName);
  }
  int GetMaxStrikesLimit() const override { return Traits::kMaxStrikeLimit; }
  std::optional<base::TimeDelta> GetExpiryTimeDelta() const override {
    return Traits::kExpiryTimeDelta;
  }
  bool UniqueIdsRequired() const override { return Traits::kUniqueIdRequired; }
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_SIMPLE_AUTOFILL_STRIKE_DATABASE_H_
