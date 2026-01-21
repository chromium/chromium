// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_STRIKE_DATABASE_H_

#include <string>

#include "components/strike_database/simple_strike_database.h"

namespace autofill {

// The delay required since the last strike before offering another virtual card
// enrollment attempt.
constexpr int kEnrollmentEnforcedDelayInDays = 7;

struct VirtualCardEnrollmentStrikeDatabaseTraits {
  static constexpr std::string_view kName = "VirtualCardEnrollment";
  static constexpr size_t kMaxStrikeEntities = 50;
  static constexpr size_t kMaxStrikeEntitiesAfterCleanup = 30;
  static constexpr size_t kMaxStrikeLimit = 3;
  static constexpr bool kUniqueIdRequired = true;
};

// This is essentially a strike_database::SimpleStrikeDatabase except that
// GetExpiryTimeDelta() depends on a feature.
//
// TODO(crbug.com/409407620): Make the class an alias of
// strike_database::SimpleStrikeDatabase when
// `kAutofillVcnEnrollStrikeExpiryTime` launches.
class VirtualCardEnrollmentStrikeDatabase
    : public strike_database::StrikeDatabaseIntegratorBase {
 public:
  explicit VirtualCardEnrollmentStrikeDatabase(
      strike_database::StrikeDatabaseBase* strike_database);
  ~VirtualCardEnrollmentStrikeDatabase() override;

  std::optional<size_t> GetMaximumEntries() const final;

  std::optional<size_t> GetMaximumEntriesAfterCleanup() const final;

  std::string GetProjectPrefix() const final;

  int GetMaxStrikesLimit() const final;

  std::optional<base::TimeDelta> GetExpiryTimeDelta() const final;

  bool UniqueIdsRequired() const final;

  // Whether bubble to be shown is the last offer for the card with
  // `instrument_id`.
  bool IsLastOffer(const std::string& instrument_id) const;

  std::optional<base::TimeDelta> GetRequiredDelaySinceLastStrike()
      const override;

 private:
  using Traits = VirtualCardEnrollmentStrikeDatabaseTraits;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_STRIKE_DATABASE_H_
