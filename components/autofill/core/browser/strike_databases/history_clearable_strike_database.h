// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_HISTORY_CLEARABLE_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_HISTORY_CLEARABLE_STRIKE_DATABASE_H_

#include <set>
#include <string>

#include "base/time/time.h"
#include "components/autofill/core/browser/strike_databases/simple_autofill_strike_database.h"
#include "components/autofill/core/browser/strike_databases/strike_database_integrator_base.h"
#include "components/history/core/browser/history_types.h"

namespace autofill {

// This class defines an interface for strike database whose keys should be
// cleared when the user clears his personal browsing history. The key of such
// strike databases doesn't necessarily need to be the origin of the website
// (Domain URL), but the origin needs to be retrievable from the key so that
// strikes could be cleared by origin.
//
// Simply define the traits of the strike database and define the strike
// database as an alias:
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
//
//   static std::string OriginFromId(const std::string& id) {
//     // Add logic here to retrieve the origin from the ID od the strike entry.
//   }
// };
//  using MyStrikeDatabase =
//                 HistoryClearableStrikeDatabase<MyStrikeDatabaseTraits>;
//
// If additional logic or overrides are needed, derive from this class.
template <typename Traits>
class HistoryClearableStrikeDatabase
    : public SimpleAutofillStrikeDatabase<Traits> {
 public:
  using SimpleAutofillStrikeDatabase<Traits>::SimpleAutofillStrikeDatabase;
  using StrikeDatabaseIntegratorBase::ClearAllStrikes;
  using StrikeDatabaseIntegratorBase::ClearStrikesByIdMatching;
  using StrikeDatabaseIntegratorBase::ClearStrikesByIdMatchingAndTime;

  void ClearStrikesWithHistory(const history::DeletionInfo& deletion_info) {
    if (deletion_info.IsAllHistory()) {
      // If the whole history is deleted, clear all strikes.
      ClearAllStrikes();
      return;
    }
    std::set<std::string> deleted_hosts;
    for (const auto& url_row : deletion_info.deleted_rows()) {
      deleted_hosts.insert(url_row.url().host());
    }
    if (!deletion_info.time_range().IsValid()) {
      ClearStrikesByOrigin(deleted_hosts);
      return;
    }
    ClearStrikesByOriginAndTime(deleted_hosts,
                                deletion_info.time_range().begin(),
                                deletion_info.time_range().end());
  }

 private:
  void ClearStrikesByOriginAndTime(const std::set<std::string>& hosts_to_delete,
                                   base::Time delete_begin,
                                   base::Time delete_end) {
    ClearStrikesByIdMatchingAndTime(hosts_to_delete, delete_begin, delete_end,
                                    &Traits::OriginFromId);
  }

  void ClearStrikesByOrigin(const std::set<std::string>& hosts_to_delete) {
    ClearStrikesByIdMatching(hosts_to_delete, &Traits::OriginFromId);
  }
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_HISTORY_CLEARABLE_STRIKE_DATABASE_H_
