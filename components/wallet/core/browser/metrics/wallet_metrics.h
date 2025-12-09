// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_METRICS_WALLET_METRICS_H_
#define COMPONENTS_WALLET_CORE_BROWSER_METRICS_WALLET_METRICS_H_

#include "components/wallet/core/browser/data_models/walletable_pass.h"

namespace wallet::metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(WalletablePassOptInFunnelEvents)
enum class WalletablePassOptInFunnelEvents {
  kConsentBubbleWasBlockedByStrike = 0,
  kUserAlreadyOptedIn = 1,
  kConsentBubbleWasShown = 2,
  kLearnMoreButtonClicked = 3,
  kConsentBubbleWasAccepted = 4,
  kConsentBubbleWasRejected = 5,
  kConsentBubbleWasClosed = 6,
  kConsentBubbleLostFocus = 7,
  kMaxValue = kConsentBubbleLostFocus,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/wallet/enums.xml:WalletablePassOptInFunnelEvents)

void LogOptInEvent(PassCategory pass_category,
                   WalletablePassOptInFunnelEvents event);

}  // namespace wallet::metrics

#endif  // COMPONENTS_WALLET_CORE_BROWSER_METRICS_WALLET_METRICS_H_
