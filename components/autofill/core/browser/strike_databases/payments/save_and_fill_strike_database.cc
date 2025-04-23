// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/payments/save_and_fill_strike_database.h"

namespace autofill {

std::optional<base::TimeDelta>
SaveAndFillStrikeDatabase::GetRequiredDelaySinceLastStrike() const {
  return std::optional<base::TimeDelta>(base::Days(kEnforcedDelayInDays));
}

}  // namespace autofill
