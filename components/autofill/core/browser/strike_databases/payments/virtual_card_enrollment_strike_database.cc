// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/payments/virtual_card_enrollment_strike_database.h"

#include "base/feature_list.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill {

VirtualCardEnrollmentStrikeDatabase::VirtualCardEnrollmentStrikeDatabase(
    strike_database::StrikeDatabaseBase* strike_database)
    : strike_database::StrikeDatabaseIntegratorBase(strike_database) {
  RemoveExpiredStrikes();
}

VirtualCardEnrollmentStrikeDatabase::~VirtualCardEnrollmentStrikeDatabase() =
    default;

bool VirtualCardEnrollmentStrikeDatabase::IsLastOffer(
    const std::string& instrument_id) const {
  // This check should not be invoked for blocked bubble.
  DCHECK_LT(GetStrikes(instrument_id), GetMaxStrikesLimit());
  return GetStrikes(instrument_id) == GetMaxStrikesLimit() - 1;
}

std::optional<base::TimeDelta>
VirtualCardEnrollmentStrikeDatabase::GetRequiredDelaySinceLastStrike() const {
  return std::optional<base::TimeDelta>(
      base::Days(kEnrollmentEnforcedDelayInDays));
}

std::optional<size_t> VirtualCardEnrollmentStrikeDatabase::GetMaximumEntries()
    const {
  return Traits::kMaxStrikeEntities;
}

std::optional<size_t>
VirtualCardEnrollmentStrikeDatabase::GetMaximumEntriesAfterCleanup() const {
  return Traits::kMaxStrikeEntitiesAfterCleanup;
}

std::string VirtualCardEnrollmentStrikeDatabase::GetProjectPrefix() const {
  return std::string(Traits::kName);
}

int VirtualCardEnrollmentStrikeDatabase::GetMaxStrikesLimit() const {
  return Traits::kMaxStrikeLimit;
}

std::optional<base::TimeDelta>
VirtualCardEnrollmentStrikeDatabase::GetExpiryTimeDelta() const {
  // TODO(crbug.com/409407620): Make the class an alias of
  // strike_database::SimpleStrikeDatabase when
  // `kAutofillVcnEnrollStrikeExpiryTime` launches.
  return base::FeatureList::IsEnabled(
             features::kAutofillVcnEnrollStrikeExpiryTime)
             ? std::optional<base::TimeDelta>(base::Days(
                   features::kAutofillVcnEnrollStrikeExpiryTimeDays.Get()))
             : base::Days(180);
}

bool VirtualCardEnrollmentStrikeDatabase::UniqueIdsRequired() const {
  return Traits::kUniqueIdRequired;
}

}  // namespace autofill
