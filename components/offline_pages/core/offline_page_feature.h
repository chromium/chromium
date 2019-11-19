// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_FEATURE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_FEATURE_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace offline_pages {

extern const base::Feature kOffliningRecentPagesFeature;
extern const base::Feature kOfflinePagesCTFeature;
extern const base::Feature kOfflinePagesLivePageSharingFeature;
extern const base::Feature kBackgroundLoaderForDownloadsFeature;
extern const base::Feature kPrefetchingOfflinePagesFeature;
extern const base::Feature kOfflinePagesLoadSignalCollectingFeature;
extern const base::Feature kOfflinePagesCTV2Feature;
extern const base::Feature kOfflinePagesRenovationsFeature;
extern const base::Feature kOfflinePagesResourceBasedSnapshotFeature;
extern const base::Feature kOfflinePagesDescriptivePendingStatusFeature;
extern const base::Feature kOfflinePagesInDownloadHomeOpenInCctFeature;
extern const base::Feature kOfflinePagesDescriptiveFailStatusFeature;
extern const base::Feature kOfflinePagesCTSuppressNotificationsFeature;
extern const base::Feature kOfflinePagesShowAlternateDinoPageFeature;
extern const base::Feature kOfflineIndicatorFeature;
extern const base::Feature kOfflineIndicatorAlwaysHttpProbeFeature;
extern const base::Feature kOnTheFlyMhtmlHashComputationFeature;

// The parameter name used to find the experiment tag for prefetching offline
// pages.
extern const char kPrefetchingOfflinePagesExperimentsOption[];

// Returns true if offlining of recent pages (aka 'Last N pages') is enabled.
bool IsOffliningRecentPagesEnabled();

// Returns true if offline CT features are enabled.  See crbug.com/620421.
bool IsOfflinePagesCTEnabled();

// Returns true if live page sharing of offline page is enabled.
bool IsOfflinePagesLivePageSharingEnabled();

// Returns true if prefetching offline pages is enabled.
bool IsPrefetchingOfflinePagesEnabled();

// Returns true if we enable load timing signals to be collected.
bool IsOfflinePagesLoadSignalCollectingEnabled();

// Returns true if we should use the "page renovation" framework in
// the BackgroundLoaderOffliner.
bool IsOfflinePagesRenovationsEnabled();

// Returns true if we should use the "Resource percentage signal" for taking
// snapshots instead of a time delay after the document is loaded in the main
// frame.
bool IsOfflinePagesResourceBasedSnapshotEnabled();

// Returns true if a command line for test has been set that shortens the
// snapshot delay.
bool ShouldUseTestingSnapshotDelay();

// Returns true if we should record request origin as part of custom tabs V2.
bool IsOfflinePagesCTV2Enabled();

// Returns true if descriptive failed download status texts should be used in
// notifications and Downloads Home.
bool IsOfflinePagesDescriptiveFailStatusEnabled();

// Returns true if descriptive pending download status texts should be used in
// notifications and Downloads Home.
bool IsOfflinePagesDescriptivePendingStatusEnabled();

// Controls whether offline pages opened from the Downloads Home should be
// opened in CCTs instead of new tabs.
bool ShouldOfflinePagesInDownloadHomeOpenInCct();

// Returns true if we should suppress completed notifications for certain custom
// tabs downloads.
bool IsOfflinePagesSuppressNotificationsEnabled();

// Controls whether we should show a dinosaur page with alternate UI.
bool ShouldShowAlternateDinoPage();

// Returns an experiment tag provided by the field trial. This experiment tag
// will be included in a custom header in all requests sent to Offline Prefetch
// Server. The server will use this this optional tag to decide how to process
// the request.
std::string GetPrefetchingOfflinePagesExperimentTag();

// Returns true if offline indicator UI is shown when the user is offline.
bool IsOfflineIndicatorFeatureEnabled();

// Returns true if we should always do http probes to detect network
// connectivity instead of retrieving it from the system. This enables the user
// to test our http probe detection on Android devices with Marshmallow and
// above.
bool IsOfflineIndicatorAlwaysHttpProbeEnabled();

// Returns true if we are saving MHTML files to the target location and
// calculating their content digests in one step.
bool IsOnTheFlyMhtmlHashComputationEnabled();

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_FEATURE_H_
