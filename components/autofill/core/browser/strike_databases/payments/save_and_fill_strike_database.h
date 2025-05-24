// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_SAVE_AND_FILL_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_SAVE_AND_FILL_STRIKE_DATABASE_H_

#include <cstddef>
#include <string_view>

#include "base/time/time.h"
#include "components/autofill/core/browser/strike_databases/simple_autofill_strike_database.h"

namespace autofill {

// The delay required since the last strike before offering another attempt.
inline constexpr int kEnforcedDelayInDays = 7;

struct SaveAndFillStrikeDatabaseTraits {
  static constexpr std::string_view kName = "SaveAndFill";
  static constexpr size_t kMaxStrikeEntities = 50;
  static constexpr size_t kMaxStrikeEntitiesAfterCleanup = 30;
  static constexpr size_t kMaxStrikeLimit = 3;
  static constexpr base::TimeDelta kExpiryTimeDelta = base::Days(180);
  static constexpr bool kUniqueIdRequired = true;
};

class SaveAndFillStrikeDatabase
    : public SimpleAutofillStrikeDatabase<SaveAndFillStrikeDatabaseTraits> {
 public:
  using SimpleAutofillStrikeDatabase<
      SaveAndFillStrikeDatabaseTraits>::SimpleAutofillStrikeDatabase;

  std::optional<base::TimeDelta> GetRequiredDelaySinceLastStrike()
      const override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_SAVE_AND_FILL_STRIKE_DATABASE_H_
