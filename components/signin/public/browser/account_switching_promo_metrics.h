// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BROWSER_ACCOUNT_SWITCHING_PROMO_METRICS_H_
#define COMPONENTS_SIGNIN_PUBLIC_BROWSER_ACCOUNT_SWITCHING_PROMO_METRICS_H_

namespace signin {

// Histogram recording lifecycle events for the Smart Account Switching startup
// promo.
inline constexpr char kAccountSwitchingPromoEventsHistogram[] =
    "Signin.AccountSwitchingPromo.Events";

// Events recorded to the `Signin.AccountSwitchingPromo.Events` histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(AccountSwitchingPromoEvent)
enum class AccountSwitchingPromoEvent {
  // Emitted at startup when the promo is eligible for display (on iOS, when
  // registered with `PromosManager`).
  kEligibleOnStartup = 0,
  // Emitted by `AccountSwitchingPromoLogger` when presentation of the account
  // switching promo is triggered.
  kPromoTriggered = 1,
  // Emitted by `AccountSwitchingPromoLogger` when the user accepts the
  // promo and completes switching to a different primary account.
  kAccepted = 2,
  // Emitted by `AccountSwitchingPromoLogger` when the user explicitly
  // dismisses the promo ("Not now" button or swipe-to-dismiss).
  kExplicitlyDismissed = 3,
  // Emitted by `AccountSwitchingPromoLogger` when the promo is
  // dismissed without an explicit accept or cancel action (e.g., interrupted by
  // external navigation, profile switch, or app dismissal).
  kIgnored = 4,

  kMaxValue = kIgnored,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:AccountSwitchingPromoEvent)

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BROWSER_ACCOUNT_SWITCHING_PROMO_METRICS_H_
