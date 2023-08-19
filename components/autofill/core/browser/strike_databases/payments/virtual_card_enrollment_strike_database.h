// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_STRIKE_DATABASE_H_

#include <stdint.h>
#include <string>

#include "components/autofill/core/browser/strike_databases/strike_database_base.h"
#include "components/autofill/core/browser/strike_databases/strike_database_integrator_base.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

// The delay required since the last strike before offering another virtual card
// enrollment attempt.
constexpr int kEnrollmentEnforcedDelayInDays = 7;

// Implementation of StrikeDatabaseIntegratorBase for virtual card enrollment
// dialogs.
class VirtualCardEnrollmentStrikeDatabase
    : public StrikeDatabaseIntegratorBase {
 public:
  explicit VirtualCardEnrollmentStrikeDatabase(
      StrikeDatabaseBase* strike_database);
  ~VirtualCardEnrollmentStrikeDatabase() override;

  // Whether bubble to be shown is the last offer for the card with
  // |instrument_id|.
  bool IsLastOffer(const std::string& instrument_id) const;

  absl::optional<size_t> GetMaximumEntries() const override;
  absl::optional<size_t> GetMaximumEntriesAfterCleanup() const override;

  std::string GetProjectPrefix() const override;
  int GetMaxStrikesLimit() const override;
  absl::optional<base::TimeDelta> GetExpiryTimeDelta() const override;
  bool UniqueIdsRequired() const override;
  absl::optional<base::TimeDelta> GetRequiredDelaySinceLastStrike()
      const override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_STRIKE_DATABASE_H_
