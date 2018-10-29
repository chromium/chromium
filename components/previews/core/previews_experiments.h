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

enum class PreviewsType {
  // Used to indicate that there is no preview type.
  NONE = 0,

  // The user is shown an offline page as a preview.
  OFFLINE = 1,

  // Replace images with placeholders.
  LOFI = 2,

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

  // Insert new enum values here. Keep values sequential to allow looping from
  // NONE+1 to LAST-1. Also add the enum to Previews.Types histogram suffix.
  LAST = 9,
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

// A list of all path suffixes to blacklist from Lite Page Server Previews.
// Primarily used to prohibit URLs that look like media requests.
std::vector<std::string> LitePagePreviewsBlacklistedPathSuffixes();

// Whether or not to trigger a preview for a navigation to localhost. Provided
// as an experiment for automated and manual testing.
bool LitePagePreviewsTriggerOnLocalhost();

// The maximum data byte size for the server-provided blacklist. This is
// a client-side safety limit for RAM use in case server sends too large of
// a blacklist.
int LitePageRedirectPreviewMaxServerBlacklistByteSize();

// The maximum number of seconds to loadshed the Previews server for.
int PreviewServerLoadshedMaxSeconds();

// The threshold of EffectiveConnectionType above which preview |type| will be
// triggered.
net::EffectiveConnectionType GetECTThresholdForPreview(
    previews::PreviewsType type);

// Whether any previews are allowed. Acts as a kill-switch or holdback check.
bool ArePreviewsAllowed();

// Whether the Previews UI is in the omnibox instead of an infobar.
bool IsPreviewsOmniboxUiEnabled();

// Whether the preview type is enabled.
bool IsOfflinePreviewsEnabled();
bool IsClientLoFiEnabled();
bool IsNoScriptPreviewsEnabled();
bool IsResourceLoadingHintsEnabled();
bool IsLitePageServerPreviewsEnabled();

// The blacklist version for each preview type.
int OfflinePreviewsVersion();
int ClientLoFiVersion();
int LitePageServerPreviewsVersion();
int NoScriptPreviewsVersion();
int ResourceLoadingHintsVersion();

// The maximum number of page hints that should be loaded to memory.
size_t GetMaxPageHintsInMemoryThreshhold();

// Whether server optimization hints are enabled.
bool IsOptimizationHintsEnabled();

// The threshold of EffectiveConnectionType above which Client Lo-Fi previews
// should not be served.
net::EffectiveConnectionType EffectiveConnectionTypeThresholdForClientLoFi();

// Returns the hosts that are blacklisted by the Client Lo-Fi field trial.
std::vector<std::string> GetBlackListedHostsForClientLoFiFieldTrial();

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

}  // namespace params

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_PREVIEWS_EXPERIMENTS_H_
