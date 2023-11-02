// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_UTILS_H_
#define COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_UTILS_H_

#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;

namespace content {
class NavigationHandle;
}

namespace prerender {

class NoStatePrefetchManager;

// Indicates whether the URL provided is a GWS origin.
bool IsGoogleOriginURL(const GURL& origin_url);

// Records the metrics for the nostate prefetch to an event with UKM source ID
// |source_id|.
void RecordNoStatePrefetchMetrics(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id,
    NoStatePrefetchManager* no_state_prefetch_manager);

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_UTILS_H_
