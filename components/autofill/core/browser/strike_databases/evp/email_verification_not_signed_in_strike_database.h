// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_EVP_EMAIL_VERIFICATION_NOT_SIGNED_IN_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_EVP_EMAIL_VERIFICATION_NOT_SIGNED_IN_STRIKE_DATABASE_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/time/time.h"
#include "components/autofill/core/browser/strike_databases/evp/email_verification_strike_database.h"
#include "components/strike_database/history_clearable_strike_database.h"

namespace autofill {

struct EmailVerificationNotSignedInStrikeDatabaseTraits {
  static constexpr std::string_view kName = "EmailVerificationNotSignedIn";
  static constexpr size_t kMaxStrikeLimit = 3;
  // We have a low limit here because we only use a one-byte hash below,
  // so to avoid getting too many collisions we only store 20 emails.
  static constexpr size_t kMaxStrikeEntities = 20;
  static constexpr std::optional<size_t> kMaxStrikeEntitiesAfterCleanup =
      std::nullopt;
  static constexpr std::optional<base::TimeDelta> kExpiryTimeDelta =
      base::Days(180);
  static constexpr bool kUniqueIdRequired = true;
};

class EmailVerificationNotSignedInStrikeDatabase
    : public strike_database::HistoryClearableStrikeDatabase<
          EmailVerificationNotSignedInStrikeDatabaseTraits> {
 public:
  explicit EmailVerificationNotSignedInStrikeDatabase(
      strike_database::StrikeDatabaseBase* strike_db)
      : HistoryClearableStrikeDatabase(strike_db) {}

  static std::string GetId(std::string_view email) {
    return GetEmailVerificationStrikeDatabaseId(email);
  }
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_EVP_EMAIL_VERIFICATION_NOT_SIGNED_IN_STRIKE_DATABASE_H_
