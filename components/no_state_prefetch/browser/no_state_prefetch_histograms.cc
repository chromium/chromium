// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/browser/no_state_prefetch_histograms.h"

#include <string>

#include "base/check_op.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "components/no_state_prefetch/common/no_state_prefetch_utils.h"

namespace prerender {

namespace {

std::string GetHistogramName(Origin origin, const std::string& name) {
  return ComposeHistogramName(
      NoStatePrefetchHistograms::GetHistogramPrefix(origin), name);
}

}  // namespace

// These strings are persisted to logs. These should remain synchronized with
// the token key PrerenderSource in
// //tools/metrics/histograms/metadata/navigation/histograms.xml.
std::string NoStatePrefetchHistograms::GetHistogramPrefix(Origin origin) {
  switch (origin) {
    case ORIGIN_NONE:
      return "none";
    case ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN:
      return "websame";
    case ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN:
      return "webcross";
    case ORIGIN_LINK_REL_NEXT:
      return "webnext";
    case ORIGIN_NAVIGATION_PREDICTOR:
      return "navigationpredictor";
    case ORIGIN_MAX:
      NOTREACHED();
  }

  // Fake return value to make the compiler happy.
  return "none";
}

void NoStatePrefetchHistograms::RecordFinalStatus(
    Origin origin,
    FinalStatus final_status) const {
  DCHECK(final_status != FINAL_STATUS_MAX);
  base::UmaHistogramEnumeration(GetHistogramName(origin, "FinalStatus"),
                                final_status, FINAL_STATUS_MAX);
  base::UmaHistogramEnumeration(ComposeHistogramName("", "FinalStatus"),
                                final_status, FINAL_STATUS_MAX);
}

}  // namespace prerender
