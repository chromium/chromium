// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_METRICS_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_METRICS_H_

namespace security_interstitials::https_only_mode {

extern const char kEventHistogram[];

extern const char kNavigationRequestSecurityLevelHistogram[];

// Recorded by HTTPS-First Mode and HTTPS-Upgrade logic when a navigation is
// upgraded, or is eligible to be upgraded but wasn't.
//
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

  // An upgrade would have been attempted but wasn't because neither HTTPS-First
  // Mode nor HTTPS Upgrading were enabled.
  kUpgradeNotAttempted = 7,

  kMaxValue = kUpgradeNotAttempted,
};

// Recorded by HTTPS-Upgrade logic when each step in a navigation request is
// observed, recording information about the protocol used. For a request with
// two redirects, this will be recorded three times (once for each redirect,
// then for the final URL).
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Values may be added to offer greater
// specificity in the future. Keep in sync with NavigationRequestSecurityLevel
// in enums.xml.
enum class NavigationRequestSecurityLevel {
  // Request was ignored because not all prerequisites were met.
  kUnknown = 0,

  // Request was for a secure (HTTPS) resource.
  kSecure = 1,

  // Request was for an insecure (HTTP) resource.
  kInsecure = 2,

  // Request was for an insecure (HTTP) resource, but was internally redirected
  // due to HSTS.
  kHstsUpgraded = 3,

  // Request was for localhost, and thus no network
  // due to HSTS.
  kLocalhost = 4,

  // Request was for an insecure (HTTP) resource, but was internally redirected
  // by the HTTPS-First Mode/HTTP Upgrading logic.
  kUpgraded = 5,

  // Request was for a URL with a scheme other than HTTP or HTTPS.
  kOtherScheme = 6,

  kMaxValue = kOtherScheme,
};

// Helper to record an HTTPS-First Mode navigation event.
void RecordHttpsFirstModeNavigation(Event event);

// Helper to record a navigation request security level.
void RecordNavigationRequestSecurityLevel(NavigationRequestSecurityLevel level);

}  // namespace security_interstitials::https_only_mode

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_METRICS_H_
