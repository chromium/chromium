// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/browser/prerender_histograms.h"

#include <string>

#include "base/check_op.h"
#include "base/format_macros.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "components/google/core/common/google_util.h"
#include "components/no_state_prefetch/common/no_state_prefetch_utils.h"
#include "net/http/http_cache.h"

namespace prerender {

namespace {

std::string GetHistogramName(Origin origin, const std::string& name) {
  return ComposeHistogramName(PrerenderHistograms::GetHistogramPrefix(origin),
                              name);
}

}  // namespace

PrerenderHistograms::PrerenderHistograms() {}

std::string PrerenderHistograms::GetHistogramPrefix(Origin origin) {
  switch (origin) {
    case ORIGIN_OMNIBOX:
      return "omnibox";
    case ORIGIN_NONE:
      return "none";
    case ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN:
      return "websame";
    case ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN:
      return "webcross";
    case ORIGIN_EXTERNAL_REQUEST:
      return "externalrequest";
    case ORIGIN_LINK_REL_NEXT:
      return "webnext";
    case ORIGIN_GWS_PRERENDER:
      return "gws";
    case ORIGIN_EXTERNAL_REQUEST_FORCED_PRERENDER:
      return "externalrequestforced";
    case ORIGIN_NAVIGATION_PREDICTOR:
      return "navigationpredictor";
    case ORIGIN_ISOLATED_PRERENDER:
      return "isolatedprerender";
    case ORIGIN_SAME_ORIGIN_SPECULATION:
      return "sameoriginspeculation";
    case ORIGIN_MAX:
      NOTREACHED();
      break;
  }

  // Dummy return value to make the compiler happy.
  return "none";
}

void PrerenderHistograms::RecordFinalStatus(Origin origin,
                                            FinalStatus final_status) const {
  DCHECK(final_status != FINAL_STATUS_MAX);
  base::UmaHistogramEnumeration(GetHistogramName(origin, "FinalStatus"),
                                final_status, FINAL_STATUS_MAX);
  base::UmaHistogramEnumeration(ComposeHistogramName("", "FinalStatus"),
                                final_status, FINAL_STATUS_MAX);
}

void PrerenderHistograms::RecordNetworkBytesConsumed(
    Origin origin,
    int64_t prerender_bytes,
    int64_t profile_bytes) const {
  const int kHistogramMin = 1;
  const int kHistogramMax = 100000000;  // 100M.
  const int kBucketCount = 50;

  UMA_HISTOGRAM_CUSTOM_COUNTS("Prerender.NetworkBytesTotalForProfile",
                              profile_bytes, kHistogramMin,
                              1000000000,  // 1G
                              kBucketCount);

  if (prerender_bytes == 0)
    return;

  base::UmaHistogramCustomCounts(GetHistogramName(origin, "NetworkBytesWasted"),
                                 prerender_bytes, kHistogramMin, kHistogramMax,
                                 kBucketCount);
  base::UmaHistogramCustomCounts(ComposeHistogramName("", "NetworkBytesWasted"),
                                 prerender_bytes, kHistogramMin, kHistogramMax,
                                 kBucketCount);
}

}  // namespace prerender
