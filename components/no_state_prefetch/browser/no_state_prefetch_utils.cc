// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/browser/no_state_prefetch_utils.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/google/core/common/google_util.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace prerender {

bool IsGoogleOriginURL(const GURL& origin_url) {
  // ALLOW_NON_STANDARD_PORTS for integration tests with the embedded server.
  if (!google_util::IsGoogleDomainUrl(origin_url,
                                      google_util::DISALLOW_SUBDOMAIN,
                                      google_util::ALLOW_NON_STANDARD_PORTS)) {
    return false;
  }

  return (origin_url.path_piece() == "/") ||
         google_util::IsGoogleSearchUrl(origin_url);
}

void RecordNoStatePrefetchMetrics(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id,
    NoStatePrefetchManager* no_state_prefetch_manager) {
  DCHECK(no_state_prefetch_manager);

  const std::vector<GURL>& redirects = navigation_handle->GetRedirectChain();

  base::TimeDelta prefetch_age;
  FinalStatus final_status;
  Origin prefetch_origin;

  bool nostate_prefetch_entry_found =
      no_state_prefetch_manager->GetPrefetchInformation(
          navigation_handle->GetURL(), &prefetch_age, &final_status,
          &prefetch_origin);

  // Try the URLs from the redirect chain.
  if (!nostate_prefetch_entry_found) {
    for (const auto& url : redirects) {
      nostate_prefetch_entry_found =
          no_state_prefetch_manager->GetPrefetchInformation(
              url, &prefetch_age, &final_status, &prefetch_origin);
      if (nostate_prefetch_entry_found)
        break;
    }
  }

  if (!nostate_prefetch_entry_found)
    return;

  ukm::builders::NoStatePrefetch builder(source_id);
  builder.SetPrefetchedRecently_PrefetchAge(
      ukm::GetExponentialBucketMinForUserTiming(prefetch_age.InMilliseconds()));
  builder.SetPrefetchedRecently_FinalStatus(final_status);
  builder.SetPrefetchedRecently_Origin(prefetch_origin);
  builder.Record(ukm::UkmRecorder::Get());
}

}  // namespace prerender
