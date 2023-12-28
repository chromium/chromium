// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_STRIKE_DATABASE_H_

#include <string>

#include "components/autofill/core/browser/strike_databases/simple_autofill_strike_database.h"

namespace autofill {

// The delay required since the last strike before offering another virtual card
// enrollment attempt.
constexpr int kEnrollmentEnforcedDelayInDays = 7;

struct VirtualCardEnrollmentStrikeDatabaseTraits {
  static constexpr std::string_view kName = "VirtualCardEnrollment";
  static constexpr size_t kMaxStrikeEntities = 50;
  static constexpr size_t kMaxStrikeEntitiesAfterCleanup = 30;
  static constexpr size_t kMaxStrikeLimit = 3;
  static constexpr base::TimeDelta kExpiryTimeDelta = base::Days(180);
  static constexpr bool kUniqueIdRequired = true;
};

class VirtualCardEnrollmentStrikeDatabase
    : public SimpleAutofillStrikeDatabase<
          VirtualCardEnrollmentStrikeDatabaseTraits> {
 public:
  using SimpleAutofillStrikeDatabase<
      VirtualCardEnrollmentStrikeDatabaseTraits>::SimpleAutofillStrikeDatabase;

  // Whether bubble to be shown is the last offer for the card with
  // |instrument_id|.
  bool IsLastOffer(const std::string& instrument_id) const;

  std::optional<base::TimeDelta> GetRequiredDelaySinceLastStrike()
      const override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_STRIKE_DATABASE_H_
