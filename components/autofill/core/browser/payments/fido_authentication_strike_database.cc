// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/fido_authentication_strike_database.h"

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

FidoAuthenticationStrikeDatabase::~FidoAuthenticationStrikeDatabase() {}

std::string FidoAuthenticationStrikeDatabase::GetProjectPrefix() {
  return "FidoAuthentication";
}

int FidoAuthenticationStrikeDatabase::GetMaxStrikesLimit() {
  return 3;
}

long long FidoAuthenticationStrikeDatabase::GetExpiryTimeMicros() {
  // Expiry time is six months.
  return 1000000LL * 60 * 60 * 24 * 30 * 6;
}

bool FidoAuthenticationStrikeDatabase::UniqueIdsRequired() {
  return false;
}

}  // namespace autofill
