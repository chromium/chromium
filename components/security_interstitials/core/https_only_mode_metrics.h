// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_METRICS_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_METRICS_H_

namespace security_interstitials {

namespace https_only_mode {

extern const char kEventHistogram[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class Event {
  // Navigation was upgraded from HTTP to HTTPS at some point (either the
  // initial request or after a redirect).
  kUpgradeAttempted = 0,

  // Navigation succeeded after being upgraded to HTTPS.
  kUpgradeSucceeded = 1,
  // Navigation failed after being upgraded to HTTPS.
  kUpgradeFailed = 2,

  // kUpgradeCertError, kUpgradeNetError, and kUpgradeTimedOut are subsets of
  // kUpgradeFailed. kUpgradeFailed should also be recorded whenever these
  // events are recorded.

  // Navigation failed due to a cert error.
  kUpgradeCertError = 3,
  // Navigation failed due to a net error.
  kUpgradeNetError = 4,
  // Navigation failed due to timing out.
  kUpgradeTimedOut = 5,

  // A prerendered HTTP navigation was cancelled.
  kPrerenderCancelled = 6,

  kMaxValue = kPrerenderCancelled,
};

}  // namespace https_only_mode
}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_METRICS_H_
