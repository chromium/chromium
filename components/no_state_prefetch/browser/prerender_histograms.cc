// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/browser/prerender_histograms.h"

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
  return ComposeHistogramName(PrerenderHistograms::GetHistogramPrefix(origin),
                              name);
}

}  // namespace

std::string PrerenderHistograms::GetHistogramPrefix(Origin origin) {
  switch (origin) {
    case ORIGIN_NONE:
      return "none";
    case ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN:
      return "websame";
    case ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN:
      return "webcross";
    case ORIGIN_LINK_REL_NEXT:
      return "webnext";
    case ORIGIN_GWS_PRERENDER:
      return "gws";
    case ORIGIN_NAVIGATION_PREDICTOR:
      return "navigationpredictor";
    case ORIGIN_SAME_ORIGIN_SPECULATION:
      return "sameoriginspeculation";
    case ORIGIN_MAX:
      NOTREACHED_IN_MIGRATION();
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

}  // namespace prerender
