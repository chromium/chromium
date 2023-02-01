// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/payments/fido_authentication_strike_database.h"

#include "components/autofill/core/browser/proto/strike_data.pb.h"

namespace autofill {

const int
    FidoAuthenticationStrikeDatabase::kStrikesToAddWhenOptInOfferDeclined = 1;
const int FidoAuthenticationStrikeDatabase::
    kStrikesToAddWhenUserVerificationFailsOnOptInAttempt = 2;
const int FidoAuthenticationStrikeDatabase::kStrikesToAddWhenUserOptsOut = 3;

FidoAuthenticationStrikeDatabase::FidoAuthenticationStrikeDatabase(
    StrikeDatabase* strike_database)
    : StrikeDatabaseIntegratorBase(strike_database) {
  RemoveExpiredStrikes();
}

FidoAuthenticationStrikeDatabase::~FidoAuthenticationStrikeDatabase() = default;

std::string FidoAuthenticationStrikeDatabase::GetProjectPrefix() const {
  return "FidoAuthentication";
}

int FidoAuthenticationStrikeDatabase::GetMaxStrikesLimit() const {
  return 3;
}

absl::optional<base::TimeDelta>
FidoAuthenticationStrikeDatabase::GetExpiryTimeDelta() const {
  // Expiry time is six months.
  return base::Days(183);
}

bool FidoAuthenticationStrikeDatabase::UniqueIdsRequired() const {
  return false;
}

}  // namespace autofill
