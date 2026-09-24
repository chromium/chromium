// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/payments/cardholder_name_fix_flow_strike_database.h"

#include <optional>

#include "base/time/time.h"

namespace autofill {

std::optional<base::TimeDelta>
CardholderNameFixFlowStrikeDatabase::GetRequiredDelaySinceLastStrike() const {
  return base::Days(7);
}

}  // namespace autofill
