// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/payments/cvc_storage_strike_database.h"

#include "base/time/time.h"

namespace autofill {

std::optional<base::TimeDelta>
CvcStorageStrikeDatabase::GetRequiredDelaySinceLastStrike() const {
  return base::Days(7);
}

}  // namespace autofill
