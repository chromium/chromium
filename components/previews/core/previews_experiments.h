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

  // The user is shown an offline page as a preview. Deprecated, and should not
  // be used.
  // DEPRECATED_OFFLINE = 1,

  // Replace images with placeholders. Deprecated, and should not be used.
  // DEPRECATED_LOFI = 2,

  // The user is shown a server lite page. Deprecated, and should not
  // be used.
  // DEPRECATED_LITE_PAGE = 3,

  // AMP version of the page is shown as a preview. Deprecated, and should not
  // be used.
  // DEPRECATED_AMP_REDIRECTION = 4,

  // Preview that disables JavaScript for the navigation.
  // NOSCRIPT = 5,

  // Special value that indicates that no specific type is identified. This
  // might be used for checks or logging that applies to any type.
  UNSPECIFIED = 6,

  // Request that resource loading hints be used during pageload.
  // RESOURCE_LOADING_HINTS = 7,

  // Allows the browser to redirect navigations to a Lite Page server.
  // DEPRECATED_LITE_PAGE_REDIRECT = 8,

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

// The maximum number of recent previews navigations the block list looks at to
// determine if a host is blocklisted.
size_t MaxStoredHistoryLengthForPerHostBlockList();

// The maximum number of recent previews navigations the block list looks at to
// determine if all previews navigations are disallowed.
size_t MaxStoredHistoryLengthForHostIndifferentBlockList();

// The maximum number of hosts allowed in the in memory block list.
size_t MaxInMemoryHostsInBlockList();

// The number of recent navigations that were opted out of for a given host that
// would trigger that host to be blocklisted.
int PerHostBlockListOptOutThreshold();

// The number of recent navigations that were opted out of that would trigger
// all previews navigations to be disallowed.
int HostIndifferentBlockListOptOutThreshold();

// The amount of time a host remains blocklisted due to opt outs.
base::TimeDelta PerHostBlockListDuration();

// The amount of time all previews navigations are disallowed due to opt outs.
base::TimeDelta HostIndifferentBlockListPerHostDuration();

// The amount of time after any opt out that no previews should be shown.
base::TimeDelta SingleOptOutDuration();

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
bool IsDeferAllScriptPreviewsEnabled();

// The blocklist version for each preview type.
int DeferAllScriptPreviewsVersion();

// Forces the coin flip holdback, if enabled, to always come up "holdback".
bool ShouldOverrideNavigationCoinFlipToHoldback();

// Forces the coin flip holdback, if enabled, to always come up "allowed".
bool ShouldOverrideNavigationCoinFlipToAllowed();

// Returns true if the given url matches an excluded media suffix.
bool ShouldExcludeMediaSuffix(const GURL& url);

// Returns true if the logic to detect redirect loops with defer all script
// preview using a cache is enabled.
bool DetectDeferRedirectLoopsUsingCache();

// Returns true if the checks to show a preview for the navigation should be
// overridden.
bool OverrideShouldShowPreviewCheck();

// Returns true if DeferAllScript should be applied even if the optimization
// guide decision is unknown. This allows DeferAllScript to be applied if the
// optimization guide does not yet know if it can be or not.
bool ApplyDeferWhenOptimizationGuideDecisionUnknown();

}  // namespace params

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_PREVIEWS_EXPERIMENTS_H_
