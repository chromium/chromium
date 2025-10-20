// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_ADDRESSES_AUTOFILL_ON_TYPING_SUGGESTION_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_ADDRESSES_AUTOFILL_ON_TYPING_SUGGESTION_STRIKE_DATABASE_H_

#include "components/strike_database/simple_strike_database.h"

namespace autofill {

struct AutofillOnTypingSuggestionStrikeDatabaseTraits {
  static constexpr std::string_view kName = "AutofillOnTyping";
  static constexpr size_t kMaxStrikeEntities = 300;
  static constexpr size_t kMaxStrikeEntitiesAfterCleanup = 70;
  static constexpr size_t kMaxStrikeLimit = 8;
  static constexpr base::TimeDelta kExpiryTimeDelta = base::Days(60);
  static constexpr bool kUniqueIdRequired = true;
};

// Records the number of times a user declines Autofill on typing suggestions
// and stops showing suggestion after reaching the strike limit.
using AutofillOnTypingSuggestionStrikeDatabase =
    strike_database::SimpleStrikeDatabase<
        AutofillOnTypingSuggestionStrikeDatabaseTraits>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_ADDRESSES_AUTOFILL_ON_TYPING_SUGGESTION_STRIKE_DATABASE_H_
