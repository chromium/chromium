// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_METRICS_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_METRICS_H_

#include <cstddef>
#include "base/time/time.h"

namespace security_interstitials::https_only_mode {

// The main histogram that records events about HTTPS-First Mode and HTTPS
// Upgrades.
extern const char kEventHistogram[];
// Same as kEventHistogram, but only recorded if the event happened on a
// navigation where HFM was enabled due to the site engagement heuristic.
extern const char kEventHistogramWithEngagementHeuristic[];

extern const char kNavigationRequestSecurityLevelHistogram[];

// Histogram that records enabled/disabled states for sites. If HFM gets enabled
// or disabled due to Site Engagement on a site, records an entry.
extern const char kSiteEngagementHeuristicStateHistogram[];

// Histogram that records the current number of host that have HFM enabled due
// to the site engagement heuristic. Includes hosts that have HTTP allowed.
extern const char kSiteEngagementHeuristicHostCountHistogram[];
// Histogram that records the accumulated number of host that have HFM enabled
// at some point due to the site engagement heuristic. Includes hosts that have
// HTTP allowed.
extern const char kSiteEngagementHeuristicAccumulatedHostCountHistogram[];

// Histogram that records the duration a host has HFM enabled due to the site
// engagement heuristic. Only recorded for hosts removed from the HFM list.
// Recorded at the time of navigation when HFM upgrades trigger.
extern const char kSiteEngagementHeuristicEnforcementDurationHistogram[];

// Histogram that records why HTTPS-First Mode interstitial was shown. Only one
// reason is recorded per interstitial.
extern const char kInterstitialReasonHistogram[];

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

  // kUpgradeCertError, kUpgradeNetError, kUpgradeTimedOut, and
  // kUpgradeRedirectLoop are subsets of kUpgradeFailed. kUpgradeFailed should
  // also be recorded whenever these events are recorded.

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

  // Upgrade failed due to encountering a redirect loop and failing early.
  kUpgradeRedirectLoop = 8,

  kMaxValue = kUpgradeRedirectLoop,
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

  // Request was explicitly allowlisted by content or enterprise settings
  // (NOT by clicking through the HFM interstitial / an upgrade failing).
  kAllowlisted = 7,

  // Request was insecure (HTTP), but was to a hostname that isn't globally
  // unique (e.g. a bare RFC1918 IP address, single-label or .local hostname).
  // This bucket is recorded IN ADDITION to kInsecure/kAllowlisted.
  kNonUniqueHostname = 8,

  // Request was insecure (HTTP), but was to a URL that was fully typed (as
  // opposed to autocompleted) that included an explicit http scheme.
  kExplicitHttpScheme = 9,

  // Request was for a captive portal login page.
  kCaptivePortalLogin = 10,

  kMaxValue = kCaptivePortalLogin,
};

// Recorded by the Site Engagement Heuristic logic, recording whether HFM should
// be enabled on a site due to its HTTP and HTTPS site engagement scores. Only
// recorded if the enabled/disabled state changes.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Values may be added to offer greater
// specificity in the future. Keep in sync with SiteEngagementHeuristicState
// in enums.xml.
enum class SiteEngagementHeuristicState {
  // HFM was not enabled and is now enabled on this site because its HTTPS score
  // is high and HTTP score is low.
  kEnabled = 0,
  // HFM was enabled and is now disabled on this site because its HTTPS score is
  // low or HTTP score is high.
  kDisabled = 1,

  kMaxValue = kDisabled,
};

// Stores the parameters to decide whether to show an interstitial for the
// current site.
// TODO(crbug.com/40937027): Consider making this a variant used to track which
// specific feature is being applied to simplify code reasoning elsewhere.
struct HttpInterstitialState {
  // Whether HTTPS-First Mode is enabled using the global UI toggle.
  bool enabled_by_pref = false;

  // Whether HTTPS-First Mode is enabled because the navigation is in Incognito
  // (when HFM-in-Incognito is enabled).
  bool enabled_by_incognito = false;

  // Whether HTTPS-First Mode is enabled for the current site due to the
  // site engagement heuristic.
  bool enabled_by_engagement_heuristic = false;

  // Whether HTTPS-First Mode is enabled because the user is in the Advanced
  // Protection program.
  bool enabled_by_advanced_protection = false;

  // Whether HTTPS-First Mode is enabled because the user's browsing pattern
  // is typically secure, i.e. they mainly visit HTTPS sites.
  bool enabled_by_typically_secure_browsing = false;

  // Whether HTTPS-First Mode is enabled in a balanced mode, which attempts to
  // warn when HTTPS can be expected to succeed, but not when it will likely
  // fail (e.g. to non-unique hostnames).
  bool enabled_in_balanced_mode = false;
};

// Helper to record an HTTPS-First Mode navigation event.
void RecordHttpsFirstModeNavigation(
    Event event,
    const HttpInterstitialState& interstitial_state);

// Helper to record a navigation request security level.
void RecordNavigationRequestSecurityLevel(NavigationRequestSecurityLevel level);

// Helper to record Site Engagement Heuristic enabled state.
void RecordSiteEngagementHeuristicState(SiteEngagementHeuristicState state);

// Helper to record metrics about the number of hosts affected by the Site
// Engagement Heuristic.
// `current_count` is the number of hosts that currently have HFM enabled.
// `accumulated_count` is the number of accumulated hosts that had HFM enabled
// at some point.
void RecordSiteEngagementHeuristicCurrentHostCounts(size_t current_count,
                                                    size_t accumulated_count);

void RecordSiteEngagementHeuristicEnforcementDuration(
    base::TimeDelta enforcement_duration);

// Recorded by the HTTPS-First Mode logic when showing the HTTPS-First Mode
// interstitial. Only one reason is recorded even though multiple flags may be
// true for the given navigation (e.g. Site Engagement + Advanced Protection).
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Values may be added to offer greater
// specificity in the future. Keep in sync with HttpsFirstModeInterstitialReason
// in security/enums.xml.
enum class InterstitialReason {
  kUnknown = 0,
  // The interstitial was shown because the user enabled the UI pref.
  kPref = 1,
  // The interstitial was shown because the user is enrolled in Advanced
  // Protection, which enables HTTPS-First Mode.
  kAdvancedProtection = 2,
  // The interstitial was shown because of the Site Engagement heuristic.
  kSiteEngagementHeuristic = 3,
  // The interstitial was shown because of the Typically Secure User heuristic.
  kTypicallySecureUserHeuristic = 4,
  // The interstitial was shown because of HTTPS-First Mode in Incognito.
  kIncognito = 5,
  // The interstitial was shown because of HTTPS-First Balance Mode.
  kBalanced = 6,

  kMaxValue = kBalanced,
};

void RecordInterstitialReason(const HttpInterstitialState& interstitial_state);

}  // namespace security_interstitials::https_only_mode

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_METRICS_H_
