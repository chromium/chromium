// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/metrics/wallet_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace wallet::metrics {

void LogOptInEvent(PassCategory pass_category,
                   WalletablePassOptInFunnelEvents event) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Wallet.WalletablePass.OptIn.Funnel.",
                    PassCategoryToString(pass_category)}),
      event);
}

}  // namespace wallet::metrics
