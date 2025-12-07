// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_FIDO_AUTHENTICATION_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_FIDO_AUTHENTICATION_STRIKE_DATABASE_H_

#include <stddef.h>

#include <optional>
#include <string_view>

#include "base/time/time.h"
#include "components/strike_database/simple_strike_database.h"
#include "components/strike_database/strike_database.h"

namespace autofill {

struct FidoAuthenticationStrikeDatabaseTraits {
  static constexpr std::string_view kName = "FidoAuthentication";
  static constexpr std::optional<size_t> kMaxStrikeEntities;
  static constexpr std::optional<size_t> kMaxStrikeEntitiesAfterCleanup;
  static constexpr size_t kMaxStrikeLimit = 3;
  static constexpr base::TimeDelta kExpiryTimeDelta = base::Days(183);
  static constexpr bool kUniqueIdRequired = false;
};

// Strike database for offering FIDO authentication for card unmasking.
class FidoAuthenticationStrikeDatabase
    : public strike_database::SimpleStrikeDatabase<
          FidoAuthenticationStrikeDatabaseTraits> {
 public:
  using strike_database::SimpleStrikeDatabase<
      FidoAuthenticationStrikeDatabaseTraits>::SimpleStrikeDatabase;

  // Strikes to add when user declines opt-in offer.
  static constexpr int kStrikesToAddWhenOptInOfferDeclined = 1;
  // Strikes to add when user fails to complete user-verification for an opt-in
  // attempt.
  static constexpr int kStrikesToAddWhenUserVerificationFailsOnOptInAttempt = 2;
  // Strikes to add when user opts-out from settings page.
  static constexpr int kStrikesToAddWhenUserOptsOut = 3;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_FIDO_AUTHENTICATION_STRIKE_DATABASE_H_
