// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_FIDO_AUTHENTICATION_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_FIDO_AUTHENTICATION_STRIKE_DATABASE_H_

#include <string>

#include "components/autofill/core/browser/payments/strike_database.h"
#include "components/autofill/core/browser/payments/strike_database_integrator_base.h"

namespace autofill {

// Implementation of StrikeDatabaseIntegratorBase for offering FIDO
// authentication for card unmasking.
class FidoAuthenticationStrikeDatabase : public StrikeDatabaseIntegratorBase {
 public:
  FidoAuthenticationStrikeDatabase(StrikeDatabase* strike_database);
  ~FidoAuthenticationStrikeDatabase() override;

  // Strikes to add when user declines opt-in offer.
  static const int kStrikesToAddWhenOptInOfferDeclined;
  // Strikes to add when user fails to complete user-verification for an opt-in
  // attempt.
  static const int kStrikesToAddWhenUserVerificationFailsOnOptInAttempt;
  // Strikes to add when user opts-out from settings page.
  static const int kStrikesToAddWhenUserOptsOut;

  std::string GetProjectPrefix() override;
  int GetMaxStrikesLimit() override;
  long long GetExpiryTimeMicros() override;
  bool UniqueIdsRequired() override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_FIDO_AUTHENTICATION_STRIKE_DATABASE_H_
