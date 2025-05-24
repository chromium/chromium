// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_AUTOFILL_AI_AUTOFILL_AI_UPDATE_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_AUTOFILL_AI_AUTOFILL_AI_UPDATE_STRIKE_DATABASE_H_

#include <string_view>

#include "components/autofill/core/browser/strike_databases/simple_autofill_strike_database.h"

namespace autofill {

// A strike database for AutofillAI update prompts that is keyed by the unique
// id of the entity that is to be updated.
struct AutofillAiUpdateStrikeDatabaseTraits {
  static constexpr std::string_view kName = "AutofillEntityUpdate";
  static constexpr size_t kMaxStrikeEntities = 200;
  static constexpr size_t kMaxStrikeEntitiesAfterCleanup = 150;
  static constexpr size_t kMaxStrikeLimit = 3;
  static constexpr base::TimeDelta kExpiryTimeDelta = base::Days(180);
  static constexpr bool kUniqueIdRequired = true;
};

using AutofillAiUpdateStrikeDatabase = autofill::SimpleAutofillStrikeDatabase<
    AutofillAiUpdateStrikeDatabaseTraits>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_AUTOFILL_AI_AUTOFILL_AI_UPDATE_STRIKE_DATABASE_H_
