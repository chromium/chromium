// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/strike_databases/pix_account_linking_strike_database.h"

namespace payments::facilitated {

std::optional<base::TimeDelta>
PixAccountLinkingStrikeDatabase::GetRequiredDelaySinceLastStrike() const {
  return kDelaySinceLastStrike;
}

}  // namespace payments::facilitated
