// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "components/search_engines/search_engine_utils.h"

namespace shared_highlighting {

namespace {

TextFragmentLinkOpenSource GetLinkSource(const GURL& referrer) {
  bool from_search_engine =
      referrer.is_valid() && SearchEngineUtils::GetEngineType(referrer) > 0;
  return from_search_engine ? TextFragmentLinkOpenSource::kSearchEngine
                            : TextFragmentLinkOpenSource::kUnknown;
}

}  // namespace

void LogLinkGenerationErrorReason(LinkGenerationError reason) {
  base::UmaHistogramEnumeration("SharedHighlights.LinkGenerated.Error", reason);
}

void LogLinkGenerationStatus(bool link_generated) {
  base::UmaHistogramBoolean("SharedHighlights.LinkGenerated", link_generated);
}

void LogTextFragmentAmbiguousMatch(bool ambiguous_match) {
  base::UmaHistogramBoolean("TextFragmentAnchor.AmbiguousMatch",
                            ambiguous_match);
}

void LogTextFragmentLinkOpenSource(const GURL& referrer) {
  base::UmaHistogramEnumeration("TextFragmentAnchor.LinkOpenSource",
                                GetLinkSource(referrer));
}

void LogTextFragmentMatchRate(int matches, int text_fragments) {
  if (text_fragments == 0) {
    NOTREACHED();
    return;
  }

  const int match_rate_percent =
      static_cast<int>(100 * ((matches + 0.0) / text_fragments));
  base::UmaHistogramPercentage("TextFragmentAnchor.MatchRate",
                               match_rate_percent);
}

void LogTextFragmentSelectorCount(int count) {
  base::UmaHistogramCounts100("TextFragmentAnchor.SelectorCount", count);
}

void LogGenerateErrorTabHidden() {
  LogLinkGenerationErrorReason(LinkGenerationError::kTabHidden);
}

void LogGenerateErrorOmniboxNavigation() {
  LogLinkGenerationErrorReason(LinkGenerationError::kOmniboxNavigation);
}

void LogGenerateErrorTabCrash() {
  LogLinkGenerationErrorReason(LinkGenerationError::kTabCrash);
}

void LogGenerateErrorIFrame() {
  LogLinkGenerationErrorReason(LinkGenerationError::kIFrame);
}
}  // namespace shared_highlighting
