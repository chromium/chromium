// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_FIDO_AUTHENTICATION_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_FIDO_AUTHENTICATION_STRIKE_DATABASE_H_

#include <stdint.h>
#include <string>

#include "components/autofill/core/browser/strike_databases/strike_database.h"
#include "components/autofill/core/browser/strike_databases/strike_database_integrator_base.h"

namespace autofill {

// Implementation of StrikeDatabaseIntegratorBase for offering FIDO
// authentication for card unmasking.
class FidoAuthenticationStrikeDatabase : public StrikeDatabaseIntegratorBase {
 public:
  explicit FidoAuthenticationStrikeDatabase(StrikeDatabase* strike_database);
  ~FidoAuthenticationStrikeDatabase() override;

  // Strikes to add when user declines opt-in offer.
  static const int kStrikesToAddWhenOptInOfferDeclined;
  // Strikes to add when user fails to complete user-verification for an opt-in
  // attempt.
  static const int kStrikesToAddWhenUserVerificationFailsOnOptInAttempt;
  // Strikes to add when user opts-out from settings page.
  static const int kStrikesToAddWhenUserOptsOut;

  std::string GetProjectPrefix() const override;
  int GetMaxStrikesLimit() const override;
  absl::optional<base::TimeDelta> GetExpiryTimeDelta() const override;
  bool UniqueIdsRequired() const override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_FIDO_AUTHENTICATION_STRIKE_DATABASE_H_
