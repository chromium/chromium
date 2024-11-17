// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_ADDRESS_SUGGESTION_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_ADDRESS_SUGGESTION_STRIKE_DATABASE_H_

#include <optional>
#include <set>
#include <string>

#include "components/autofill/core/browser/strike_databases/history_clearable_strike_database.h"
#include "components/autofill/core/browser/strike_databases/strike_database_integrator_base.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/signatures.h"
#include "url/gurl.h"

namespace autofill {

struct AddressSuggestionStrikeDatabaseTraits {
  static constexpr std::string_view kName = "AddressSuggestion";
  // This value is now meaningless, since precedence is taken by the
  // parameterized `AddressSuggestionStrikeDatabase::GetMaxStrikesLimit`.
  static constexpr size_t kMaxStrikeLimit = 3;
  static constexpr size_t kMaxStrikeEntities = 300;
  // Strikes in this database do not expire.
  static constexpr std::optional<size_t> kMaxStrikeEntitiesAfterCleanup =
      std::nullopt;
  static constexpr std::optional<base::TimeDelta> kExpiryTimeDelta =
      std::nullopt;
  static constexpr bool kUniqueIdRequired = true;

  static std::string OriginFromId(const std::string& id) {
    // AddressSuggestionStrikeDatabase keys have the following format:
    // form_signature | field_signature | '-' | origin (| is concatenation).
    // Since the signatures cannot contain dashes, we just return everything
    // that comes after the first dash, which is the separator we added.
    size_t first_dash = id.find_first_of("-");
    CHECK_NE(first_dash, std::string::npos);
    return id.substr(first_dash + 1);
  }
};

// Records the number of times a user ignores address suggestions shown via the
// Autofill popup and stops automatically showing address suggestions to the
// user after reaching a strike limit.
class AddressSuggestionStrikeDatabase
    : public HistoryClearableStrikeDatabase<
          AddressSuggestionStrikeDatabaseTraits> {
 public:
  using HistoryClearableStrikeDatabase<
      AddressSuggestionStrikeDatabaseTraits>::HistoryClearableStrikeDatabase;

  static std::string GetId(FormSignature form_signature,
                           FieldSignature field_signature,
                           const GURL& url);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_ADDRESS_SUGGESTION_STRIKE_DATABASE_H_
