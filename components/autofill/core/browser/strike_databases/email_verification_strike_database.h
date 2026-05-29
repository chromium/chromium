// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_EMAIL_VERIFICATION_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_EMAIL_VERIFICATION_STRIKE_DATABASE_H_

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/strike_database/history_clearable_strike_database.h"
#include "crypto/hash.h"

namespace autofill {

struct EmailVerificationStrikeDatabaseTraits {
  static constexpr std::string_view kName = "EmailVerification";
  static constexpr size_t kMaxStrikeLimit = 3;
  static constexpr size_t kMaxStrikeEntities = 300;
  static constexpr std::optional<size_t> kMaxStrikeEntitiesAfterCleanup =
      std::nullopt;
  static constexpr std::optional<base::TimeDelta> kExpiryTimeDelta =
      base::Days(180);
  static constexpr bool kUniqueIdRequired = true;
};

class EmailVerificationStrikeDatabase
    : public strike_database::HistoryClearableStrikeDatabase<
          EmailVerificationStrikeDatabaseTraits> {
 public:
  explicit EmailVerificationStrikeDatabase(
      strike_database::StrikeDatabaseBase* strike_db)
      : HistoryClearableStrikeDatabase(strike_db) {}

  static std::string GetId(const std::string& email) {
    // Use just the first two bytes of the hash for privacy reasons, so that the
    // original email can't be determined from the hash.
    const std::array<uint8_t, crypto::hash::kSha256Size> hash =
        crypto::hash::Sha256(email);
    return base::HexEncode(base::span(hash).first<2>());
  }
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_EMAIL_VERIFICATION_STRIKE_DATABASE_H_
