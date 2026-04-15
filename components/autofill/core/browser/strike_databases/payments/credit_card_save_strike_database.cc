// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/payments/credit_card_save_strike_database.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill {

std::optional<base::TimeDelta>
CreditCardSaveStrikeDatabase::GetRequiredDelaySinceLastStrike() const {
  return base::FeatureList::IsEnabled(
             features::kAutofillUpstreamEnforceStrikeDelay)
             ? std::optional<base::TimeDelta>(
                   CreditCardSaveStrikeDatabaseTraits::
                       kRequiredDelayBetweenStrikes)
             : std::nullopt;
}

}  // namespace autofill
