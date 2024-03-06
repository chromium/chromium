// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_FEATURE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_FEATURE_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace offline_pages {

BASE_DECLARE_FEATURE(kBackgroundLoaderForDownloadsFeature);
BASE_DECLARE_FEATURE(kOfflinePagesInDownloadHomeOpenInCctFeature);
BASE_DECLARE_FEATURE(kOnTheFlyMhtmlHashComputationFeature);
BASE_DECLARE_FEATURE(kOfflinePagesNetworkStateLikelyUnknown);

// The parameter name used to find the experiment tag for prefetching offline
// pages.
extern const char kPrefetchingOfflinePagesExperimentsOption[];

// Returns true if a command line for test has been set that shortens the
// snapshot delay.
bool ShouldUseTestingSnapshotDelay();

// Controls whether offline pages opened from the Downloads Home should be
// opened in CCTs instead of new tabs.
bool ShouldOfflinePagesInDownloadHomeOpenInCct();

// Returns an experiment tag provided by the field trial. This experiment tag
// will be included in a custom header in all requests sent to Offline Prefetch
// Server. The server will use this this optional tag to decide how to process
// the request.
std::string GetPrefetchingOfflinePagesExperimentTag();

// Returns true if we are saving MHTML files to the target location and
// calculating their content digests in one step.
bool IsOnTheFlyMhtmlHashComputationEnabled();

// Returns true if the network state returned by Android device APIs can't be
// trusted. See https://crbug.com/1120144. In such cases, we want to disable
// offline pages to avoid showing them even when the device is online.
bool IsOfflinePagesNetworkStateLikelyUnknown();

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_FEATURE_H_
