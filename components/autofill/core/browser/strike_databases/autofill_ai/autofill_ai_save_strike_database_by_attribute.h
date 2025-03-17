// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_AUTOFILL_AI_AUTOFILL_AI_SAVE_STRIKE_DATABASE_BY_ATTRIBUTE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_AUTOFILL_AI_AUTOFILL_AI_SAVE_STRIKE_DATABASE_BY_ATTRIBUTE_H_

#include <string_view>

#include "components/autofill/core/browser/strike_databases/simple_autofill_strike_database.h"

namespace autofill {

// A strike database for AutofillAI save prompts that is keyed by a hash of
// (entity_type;attribute_type_1;attribute_1_value;attribute_type_2;...)
// for attribute tuples that are listed as strike keys in the entity schema.
struct AutofillAiSaveStrikeDatabaseByAttributeTraits {
  static constexpr std::string_view kName = "AutofillEntitySaveByAttribute";
  static constexpr size_t kMaxStrikeEntities = 200;
  static constexpr size_t kMaxStrikeEntitiesAfterCleanup = 150;
  static constexpr size_t kMaxStrikeLimit = 3;
  static constexpr base::TimeDelta kExpiryTimeDelta = base::Days(180);
  static constexpr bool kUniqueIdRequired = true;
};

using AutofillAiSaveStrikeDatabaseByAttribute =
    autofill::SimpleAutofillStrikeDatabase<
        AutofillAiSaveStrikeDatabaseByAttributeTraits>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_AUTOFILL_AI_AUTOFILL_AI_SAVE_STRIKE_DATABASE_BY_ATTRIBUTE_H_
