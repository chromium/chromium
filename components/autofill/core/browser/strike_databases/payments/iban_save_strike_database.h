// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_IBAN_SAVE_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_IBAN_SAVE_STRIKE_DATABASE_H_

#include "components/autofill/core/browser/strike_databases/simple_autofill_strike_database.h"
#include "components/autofill/core/browser/strike_databases/strike_database.h"

namespace autofill {

struct IbanSaveStrikeDatabaseTraits {
  static constexpr std::string_view kName = "IBANSave";
  static constexpr std::optional<size_t> kMaxStrikeEntities = std::nullopt;
  static constexpr std::optional<size_t> kMaxStrikeEntitiesAfterCleanup =
      std::nullopt;
  static constexpr size_t kMaxStrikeLimit = 3;
  static constexpr base::TimeDelta kExpiryTimeDelta = base::Days(183);
  static constexpr bool kUniqueIdRequired = true;
};

using IbanSaveStrikeDatabase =
    SimpleAutofillStrikeDatabase<IbanSaveStrikeDatabaseTraits>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_IBAN_SAVE_STRIKE_DATABASE_H_
