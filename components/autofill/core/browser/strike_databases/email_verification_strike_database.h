// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_EMAIL_VERIFICATION_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_EMAIL_VERIFICATION_STRIKE_DATABASE_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/time/time.h"
#include "components/strike_database/history_clearable_strike_database.h"

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

  static std::string HostFromId(const std::string& id) {
    size_t at_pos = id.find_last_of("@");
    if (at_pos == std::string::npos) {
      return "";
    }
    return id.substr(at_pos + 1);
  }
};

class EmailVerificationStrikeDatabase
    : public strike_database::HistoryClearableStrikeDatabase<
          EmailVerificationStrikeDatabaseTraits> {
 public:
  explicit EmailVerificationStrikeDatabase(
      strike_database::StrikeDatabaseBase* strike_db)
      : HistoryClearableStrikeDatabase(strike_db) {}

  static std::string GetId(const std::string& email) { return email; }
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_EMAIL_VERIFICATION_STRIKE_DATABASE_H_
