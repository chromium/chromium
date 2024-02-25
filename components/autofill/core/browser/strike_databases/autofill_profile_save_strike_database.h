// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_AUTOFILL_PROFILE_SAVE_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_AUTOFILL_PROFILE_SAVE_STRIKE_DATABASE_H_

#include <set>
#include <string>

#include "components/autofill/core/browser/strike_databases/simple_autofill_strike_database.h"

namespace autofill {

struct AutofillProfileSaveStrikeDatabaseTraits {
  static constexpr std::string_view kName = "AutofillProfileSave";
  static constexpr size_t kMaxStrikeEntities = 200;
  static constexpr size_t kMaxStrikeEntitiesAfterCleanup = 150;
  static constexpr size_t kMaxStrikeLimit = 3;
  static constexpr base::TimeDelta kExpiryTimeDelta = base::Days(180);
  static constexpr bool kUniqueIdRequired = true;
};

// Records the number of times a user declines saving their Autofill profile and
// stops prompting the user to do so after reaching a strike limit.
class AutofillProfileSaveStrikeDatabase
    : public SimpleAutofillStrikeDatabase<
          AutofillProfileSaveStrikeDatabaseTraits> {
 public:
  using SimpleAutofillStrikeDatabase<
      AutofillProfileSaveStrikeDatabaseTraits>::SimpleAutofillStrikeDatabase;

  void ClearStrikesByOriginAndTimeInternal(
      const std::set<std::string>& hosts_to_delete,
      base::Time delete_begin,
      base::Time delete_end);

  void ClearStrikesByOrigin(const std::set<std::string>& hosts_to_delete);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_AUTOFILL_PROFILE_SAVE_STRIKE_DATABASE_H_
