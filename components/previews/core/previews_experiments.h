// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_PREVIEWS_EXPERIMENTS_H_
#define COMPONENTS_PREVIEWS_CORE_PREVIEWS_EXPERIMENTS_H_

#include <string>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "net/nqe/effective_connection_type.h"
#include "url/gurl.h"

namespace previews {

// Types of previews. This enum must remain synchronized with the enum
// |PreviewsType| in tools/metrics/histograms/enums.xml.
enum class PreviewsType {
  // Used to indicate that there is no preview type.
  NONE = 0,

  // The user is shown an offline page as a preview.
  OFFLINE = 1,

  // Replace images with placeholders. Deprecated, and should not be used.
  DEPRECATED_LOFI = 2,

  // The user is shown a server lite page.
  LITE_PAGE = 3,

  // AMP version of the page is shown as a preview. Deprecated, and should not
  // be used.
  DEPRECATED_AMP_REDIRECTION = 4,

  // Preview that disables JavaScript for the navigation.
  NOSCRIPT = 5,

  // Special value that indicates that no specific type is identified. This
  // might be used for checks or logging that applies to any type.
  UNSPECIFIED = 6,

  // Request that resource loading hints be used during pageload.
  RESOURCE_LOADING_HINTS = 7,

  // Allows the browser to redirect navigations to a Lite Page server.
  LITE_PAGE_REDIRECT = 8,

  // Preview that defers script execution until after parsing completes.
  DEFER_ALL_SCRIPT = 9,

  // Insert new enum values here. Keep values sequential to allow looping from
  // NONE+1 to LAST-1. Also add the enum to Previews.Types histogram suffix.
  LAST = 10,
};

enum class CoinFlipHoldbackResult {
  // Either the page load was not eligible for any previews, or the coin flip
  // holdback experiment was disabled.
  kNotSet = 0,

  // A preview was likely for the page load, and a random coin flip allowed the
  // preview to be shown to the user.
  kAllowed = 1,

  // A preview was likely for the page load, and a random coin flip did not
  // allow the preview to be shown to the user.
  kHoldback = 2,
};

typedef std::vector<std::pair<PreviewsType, int>> PreviewsTypeList;

// Gets the string representation of |type|.
std::string GetStringNameForType(PreviewsType type);

namespace params {

// The maximum number of recent previews navigations the black list looks at to
// determine if a host is blacklisted.
size_t MaxStoredHistoryLengthForPerHostBlackList();

// The maximum number of recent previews navigations the black list looks at to
// determine if all previews navigations are disallowed.
size_t MaxStoredHistoryLengthForHostIndifferentBlackList();

// The maximum number of hosts allowed in the in memory black list.
size_t MaxInMemoryHostsInBlackList();

// The number of recent navigations that were opted out of for a given host that
// would trigger that host to be blacklisted.
int PerHostBlackListOptOutThreshold();

// The number of recent navigations that were opted out of that would trigger
// all previews navigations to be disallowed.
int HostIndifferentBlackListOptOutThreshold();

// The amount of time a host remains blacklisted due to opt outs.
base::TimeDelta PerHostBlackListDuration();

// The amount of time all previews navigations are disallowed due to opt outs.
base::TimeDelta HostIndifferentBlackListPerHostDuration();

// The amount of time after any opt out that no previews should be shown.
base::TimeDelta SingleOptOutDuration();

// The amount of time that an offline page is considered fresh enough to be
// shown as a preview.
base::TimeDelta OfflinePreviewFreshnessDuration();

// The amount of time that a Server Lite Page Preview navigation can take before
// it is killed and the original page is loaded.
base::TimeDelta LitePagePreviewsNavigationTimeoutDuration();

// The host for Lite Page server previews.
GURL GetLitePagePreviewsDomainURL();

// The duration of a single bypass for Lite Page Server Previews.
base::TimeDelta LitePagePreviewsSingleBypassDuration();

// Whether or not to trigger a preview for a navigation to localhost. Provided
// as an experiment for automated and manual testing.
bool LitePagePreviewsTriggerOnLocalhost();

// Whether to request a Lite Page Server Preview even if there are optimization
// page hints for the host.
bool LitePagePreviewsOverridePageHints();

// Whether we should preconnect to the lite page redirect server or the origin.
bool LitePageRedirectPreviewShouldPreconnect();

// Whether we should preresolve the lite page redirect server or the origin.
bool LitePageRedirectPreviewShouldPresolve();

// Whether the Optimization Guide logic should be ignored for lite page redirect
// previews.
bool LitePageRedirectPreviewIgnoresOptimizationGuideFilter();

// Whether to only trigger a lite page preview if there has been a successful
// probe to the server. This is returns true, lite page redirect previews should
// only been attempted when a probe to the previews server has completed
// successfully.
bool LitePageRedirectOnlyTriggerOnSuccessfulProbe();

// Whether the preview should trigger on API page transitions.
bool LitePageRedirectTriggerOnAPITransition();

// Whether the preview should trigger on forward/back page transitions.
bool LitePageRedirectValidateForwardBackTransition();

// The URL to probe on the lite pages server.
GURL LitePageRedirectProbeURL();

// The duration in between preresolving or preconnecting the lite page redirect
// server or the origin.
base::TimeDelta LitePageRedirectPreviewPreresolvePreconnectInterval();

// The ect threshold at which, or below, we should preresolve or preconnect for
// lite page redirect previews.
net::EffectiveConnectionType
LitePageRedirectPreviewPreresolvePreconnectECTThreshold();

// The duration in between probes to the lite page redirect server.
base::TimeDelta LitePageRedirectPreviewProbeInterval();

// Whether the origin should be successfully probed before showing a preview.
bool LitePageRedirectShouldProbeOrigin();

// The timeout for the origin probe on lite page redirect previews.
base::TimeDelta LitePageRedirectPreviewOriginProbeTimeout();

// The maximum number of seconds to loadshed the Previews server for.
int PreviewServerLoadshedMaxSeconds();

// Returns true if we should only report metrics and not trigger when the Lite
// Page Redirect preview is enabled.
bool IsInLitePageRedirectControl();

// The default EffectiveConnectionType threshold where preview |type| will be
// triggered.
net::EffectiveConnectionType GetECTThresholdForPreview(
    previews::PreviewsType type);

// The maximum EffectiveConnectionType threshold where this client session is
// allowed to trigger previews (for slow page triggered previews). This may be
// Finch configured on a session basis to limit the proportion of previews
// triggered at faster connections.
net::EffectiveConnectionType GetSessionMaxECTThreshold();

// Whether any previews are allowed. Acts as a kill-switch or holdback check.
bool ArePreviewsAllowed();

// Whether the preview type is enabled.
bool IsOfflinePreviewsEnabled();
bool IsNoScriptPreviewsEnabled();
bool IsResourceLoadingHintsEnabled();
bool IsLitePageServerPreviewsEnabled();
bool IsDeferAllScriptPreviewsEnabled();

// The blacklist version for each preview type.
int OfflinePreviewsVersion();
int LitePageServerPreviewsVersion();
int NoScriptPreviewsVersion();
int ResourceLoadingHintsVersion();
int DeferAllScriptPreviewsVersion();

// For estimating NoScript data savings, this is the percentage factor to
// multiple by the network bytes for inflating the original_bytes count.
int NoScriptPreviewsInflationPercent();

// For estimating NoScript data savings, this is the number of bytes to
// for inflating the original_bytes count.
int NoScriptPreviewsInflationBytes();

// For estimating ResourceLoadingHints data savings, this is the percentage
// factor to multiple by the network bytes for inflating the original_bytes
// count.
int ResourceLoadingHintsPreviewsInflationPercent();

// For estimating ResourceLoadingHints data savings, this is the number of
// bytes to for inflating the original_bytes count.
int ResourceLoadingHintsPreviewsInflationBytes();

// The maximum number of pref entries that should be kept by
// PreviewsOfflineHelper.
size_t OfflinePreviewsHelperMaxPrefSize();

// Forces the coin flip holdback, if enabled, to always come up "holdback".
bool ShouldOverrideNavigationCoinFlipToHoldback();

// Forces the coin flip holdback, if enabled, to always come up "allowed".
bool ShouldOverrideNavigationCoinFlipToAllowed();

// Returns true if the given url matches an excluded media suffix.
bool ShouldExcludeMediaSuffix(const GURL& url);

// Returns true if the logic to detect redirect loops with defer all script
// preview using a cache is enabled.
bool DetectDeferRedirectLoopsUsingCache();

}  // namespace params

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_PREVIEWS_EXPERIMENTS_H_
