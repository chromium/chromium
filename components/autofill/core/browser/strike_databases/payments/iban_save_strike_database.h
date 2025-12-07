// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_IBAN_SAVE_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_IBAN_SAVE_STRIKE_DATABASE_H_

#include <stddef.h>

#include <optional>
#include <string_view>

#include "base/time/time.h"
#include "components/strike_database/simple_strike_database.h"
#include "components/strike_database/strike_database.h"

namespace autofill {

struct IbanSaveStrikeDatabaseTraits {
  static constexpr std::string_view kName = "IBANSave";
  static constexpr std::optional<size_t> kMaxStrikeEntities;
  static constexpr std::optional<size_t> kMaxStrikeEntitiesAfterCleanup;
  static constexpr size_t kMaxStrikeLimit = 3;
  static constexpr base::TimeDelta kExpiryTimeDelta = base::Days(183);
  static constexpr bool kUniqueIdRequired = true;
};

using IbanSaveStrikeDatabase =
    strike_database::SimpleStrikeDatabase<IbanSaveStrikeDatabaseTraits>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_IBAN_SAVE_STRIKE_DATABASE_H_
